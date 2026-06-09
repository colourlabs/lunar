#include "isolate.hpp"
#include "lunar_limits.h"
#include "sandbox.hpp"
#include "utils/logger.hpp"

namespace LunarCore {

Isolate::Isolate(const std::string &worker_path, const std::string &std_path)
    : m_limits(), m_alloc_state({.m_used = 0,
                                 .m_limit = 128ULL * 1024 * 1024,
                                 .m_peak = 0,
                                 .m_baseline = 0,
                                 .m_limits = &m_limits}),
      m_lua_state(lua_newstate(lunar_alloc, &m_alloc_state, 0)) {
    if (m_lua_state == nullptr) {
        m_last_error = "failed to create Lua state (out of memory?)";
        return;
    }

    m_limits.m_instruction_limit = 50000000;
    m_limits.m_max_string_len = 1024 * 1024;

    if (!lunar_install_limits(m_lua_state)) {
        m_last_error = "failed to allocate limits struct";
        lua_close(m_lua_state);
        m_lua_state = nullptr;
        return;
    }

    lunar_set_instruction_limit(m_lua_state, 50000000);
    lunar_set_string_limits(m_lua_state, 1024 * 1024, 100000);
    lunar_set_coroutine_limit(m_lua_state, 1000);
    lunar_set_pattern_limit(m_lua_state, 100000);

    Sandbox::install(m_lua_state, std_path);

    if (!load_worker(worker_path)) {
        lunar_destroy_limits(m_lua_state);
        lua_close(m_lua_state);
        m_lua_state = nullptr;
        return;
    }

    lua_gc(m_lua_state, LUA_GCCOLLECT, 0);
    lua_gc(m_lua_state, LUA_GCCOLLECT, 0);

    m_alloc_state.m_baseline = m_alloc_state.m_used;
    m_alloc_state.m_peak = 0;
}

Isolate::~Isolate() {
    if (m_lua_state != nullptr) {
        if (m_worker_env_ref != LUA_NOREF) {
            luaL_unref(m_lua_state, LUA_REGISTRYINDEX, m_worker_env_ref);
        }
        lunar_destroy_limits(m_lua_state);
        lua_close(m_lua_state);
        m_lua_state = nullptr;
    }
}

Isolate::Isolate(Isolate &&other) noexcept
    : m_limits(other.m_limits), m_alloc_state(other.m_alloc_state), m_lua_state(other.m_lua_state),
      m_last_error(std::move(other.m_last_error)), m_worker_env_ref(other.m_worker_env_ref) {

    m_alloc_state.m_limits = &m_limits;

    if (m_lua_state != nullptr) {
        lua_setallocf(m_lua_state, lunar_alloc, &m_alloc_state);
    }
    other.m_lua_state = nullptr;
    other.m_worker_env_ref = LUA_NOREF;
}

Isolate &Isolate::operator=(Isolate &&other) noexcept {
    if (this != &other) {
        if (m_lua_state != nullptr) {
            if (m_worker_env_ref != LUA_NOREF) {
                luaL_unref(m_lua_state, LUA_REGISTRYINDEX, m_worker_env_ref);
            }
            lunar_destroy_limits(m_lua_state);
            lua_close(m_lua_state);
        }

        m_alloc_state.m_limits = &m_limits;

        m_alloc_state = other.m_alloc_state;
        m_lua_state = other.m_lua_state;
        m_last_error = std::move(other.m_last_error);
        m_worker_env_ref = other.m_worker_env_ref;

        if (m_lua_state != nullptr) {
            lua_setallocf(m_lua_state, lunar_alloc, &m_alloc_state);
        }

        other.m_lua_state = nullptr;
        other.m_worker_env_ref = LUA_NOREF;
    }
    return *this;
}

bool Isolate::load_worker(const std::string &path) {
    if (luaL_loadfile(m_lua_state, path.c_str()) != LUA_OK) {
        m_last_error = lua_tostring(m_lua_state, -1);
        lua_pop(m_lua_state, 1);
        return false;
    }

    // create the restricted environment table
    lua_newtable(m_lua_state);

    // Inherit from master globals via __index metatable
    lua_rawgeti(m_lua_state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    lua_newtable(m_lua_state);
    lua_pushvalue(m_lua_state, -2);
    lua_setfield(m_lua_state, -2, "__index");
    lua_setmetatable(m_lua_state, -3);
    lua_pop(m_lua_state, 1);

    // save a reference to this environment for C++ use ---
    lua_pushvalue(m_lua_state, -1); // duplicate the env table on the stack
    m_worker_env_ref =
        luaL_ref(m_lua_state, LUA_REGISTRYINDEX); // pops the duplicate, returns a ref ID

    // bind the table as '_ENV' upvalue to the loaded worker chunk
    lua_setupvalue(m_lua_state, -2, 1);

    if (lua_pcall(m_lua_state, 0, 0, 0) != LUA_OK) {
        m_last_error = lua_tostring(m_lua_state, -1);
        lua_pop(m_lua_state, 1);
        return false;
    }

    return true;
}

void Isolate::dispatch_async(uv_loop_t *loop,
                             std::function<int(lua_State*)> push_args,
                             std::function<void(lua_State*)> on_result) {
    lunar_reset_instruction_count(m_lua_state);

    lua_pushlightuserdata(m_lua_state, loop);
    lua_setfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_uv_loop");
    lua_pushlightuserdata(m_lua_state, this);
    lua_setfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_isolate");

    lua_State *thread = lua_newthread(m_lua_state);
    int thread_ref = luaL_ref(m_lua_state, LUA_REGISTRYINDEX);

    lua_rawgeti(thread, LUA_REGISTRYINDEX, m_worker_env_ref);
    lua_getfield(thread, -1, "handle");
    lua_remove(thread, -2);

    if (!lua_isfunction(thread, -1)) {
        m_last_error = "handle() function not found in worker environment";
        Logger::error("[isolate] handle() not found");
        luaL_unref(m_lua_state, LUA_REGISTRYINDEX, thread_ref);
        lua_pushnil(thread);
        on_result(thread);
        return;
    }

    int nargs = push_args(thread);
    m_pending[thread] = PendingRequest{
        .m_on_result = std::move(on_result),
        .m_thread_ref = thread_ref,
    };

    Logger::debug("[isolate] starting coroutine");
    step_coroutine(thread, nargs);
}

void Isolate::step_coroutine(lua_State *thread, int nargs) {
    int nres = 0;
    int status = lua_resume(thread, nullptr, nargs, &nres);

    Logger::debug("[isolate] lua_resume returned status={}", status);

    if (status == LUA_YIELD) {
        Logger::debug("[isolate] coroutine yielded (waiting for fetch)");
        return;
    }

    auto node = m_pending.extract(thread);
    if (node.empty()) {
        Logger::error("[isolate] step_coroutine: no pending entry for thread");
        return;
    }

    PendingRequest pending = std::move(node.mapped());
    luaL_unref(m_lua_state, LUA_REGISTRYINDEX, pending.m_thread_ref);

    if (status != LUA_OK) {
        m_last_error = lua_tostring(thread, -1);
        Logger::error("[isolate] coroutine error: {}", m_last_error);
        Logger::error("[isolate] used={} peak={} limit={} baseline={}",
            m_alloc_state.m_used, m_alloc_state.m_peak,
            m_alloc_state.m_limit, m_alloc_state.m_baseline);
        lua_pop(thread, nres);
        pending.m_on_result(thread);
        return;
    }

    pending.m_on_result(thread);
    lua_pop(thread, nres);

    Logger::debug("[lua] used={} peak={} limit={}", m_alloc_state.m_used, m_alloc_state.m_peak,
                  m_alloc_state.m_limit);

    lua_gc(m_lua_state, LUA_GCCOLLECT, 0);

    Logger::debug("[lua after gc] used={} peak={} limit={}", m_alloc_state.m_used,
                  m_alloc_state.m_peak, m_alloc_state.m_limit);
}

void Isolate::resume_coroutine(lua_State *thread, int nargs) {
    step_coroutine(thread, nargs);
}

} // namespace LunarCore