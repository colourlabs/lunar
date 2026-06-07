#include "isolate.hpp"
#include "lunar_limits.h"
#include "sandbox.hpp"
#include "utils/logger.hpp"

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

void Isolate::push_request(lua_State *thread, const LuaRequest &req) {
    lua_newtable(thread);

    // req.method
    lua_pushstring(thread, req.m_method.c_str());
    lua_setfield(thread, -2, "method");

    // req.path
    lua_pushstring(thread, req.m_path.c_str());
    lua_setfield(thread, -2, "path");

    // req.body
    lua_pushstring(thread, req.m_body.c_str());
    lua_setfield(thread, -2, "body");

    // req.headers
    lua_newtable(thread);
    for (const auto &[k, v] : req.m_headers) {
        lua_pushstring(thread, v.c_str());
        lua_setfield(thread, -2, k.c_str());
    }
    lua_setfield(thread, -2, "headers");

    // req.query
    lua_newtable(thread);
    for (const auto &[k, v] : req.m_query) {
        lua_pushstring(thread, v.c_str());
        lua_setfield(thread, -2, k.c_str());
    }
    lua_setfield(thread, -2, "query");
}

LuaResponse Isolate::read_response(lua_State *thread) {
    LuaResponse res;

    if (!lua_istable(thread, -1)) {
        m_last_error = "handle() must return a table";
        res.m_status = 500;
        res.m_body = m_last_error;
        return res;
    }

    // read status
    lua_getfield(thread, -1, "status");
    res.m_status =
        (lua_isinteger(thread, -1) != 0) ? static_cast<int>(lua_tointeger(thread, -1)) : 200;
    lua_pop(thread, 1);

    // read body
    lua_getfield(thread, -1, "body");
    res.m_body = (lua_isstring(thread, -1) != 0) ? lua_tostring(thread, -1) : "";
    lua_pop(thread, 1);

    // read headers
    lua_getfield(thread, -1, "headers");
    if (lua_istable(thread, -1)) {
        lua_pushnil(thread);
        while (lua_next(thread, -2) != 0) {
            if ((lua_isstring(thread, -2) != 0) && (lua_isstring(thread, -1) != 0)) {
                std::string key = lua_tostring(thread, -2);
                std::string value = lua_tostring(thread, -1);
                res.m_headers[key] = value;
            }
            lua_pop(thread, 1); // pop value, keep key for next iteration
        }
    }
    lua_pop(thread, 1); // pop headers table

    return res;
}

void Isolate::dispatch_async(const LuaRequest &req, uv_loop_t *loop,
                             std::function<void(std::optional<LuaResponse>)> on_done) {
    lunar_reset_instruction_count(m_lua_state);

    static constexpr size_t max_body = 1024 * 1024;
    static constexpr size_t max_header = 8 * 1024;
    static constexpr size_t max_headers = 64;

    if (req.m_body.size() > max_body) {
        m_last_error = "request body too large";
        on_done(std::nullopt);
        return;
    }
    if (req.m_headers.size() > max_headers) {
        m_last_error = "too many request headers";
        on_done(std::nullopt);
        return;
    }

    for (const auto &[k, v] : req.m_headers) {
        if (k.size() > max_header || v.size() > max_header) {
            m_last_error = "request header too large";
            on_done(std::nullopt);
            return;
        }
    }

    // stash the loop so lua_fetch can find it
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
        on_done(std::nullopt);
        return;
    }

    push_request(thread, req);
    m_pending[thread] = PendingRequest{
        .m_on_done = std::move(on_done),
        .m_thread_ref = thread_ref,
    };

    Logger::debug("[isolate] starting coroutine");
    step_coroutine(thread, 1);
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
        pending.m_on_done(std::nullopt);
        return;
    }

    LuaResponse res = read_response(thread);
    lua_pop(thread, nres);

    Logger::debug("[lua] used={} peak={} limit={}", m_alloc_state.m_used, m_alloc_state.m_peak,
                  m_alloc_state.m_limit);

    lua_gc(m_lua_state, LUA_GCCOLLECT, 0);

    Logger::debug("[lua after gc] used={} peak={} limit={}", m_alloc_state.m_used,
                  m_alloc_state.m_peak, m_alloc_state.m_limit);

    pending.m_on_done(res);
}

void Isolate::resume_coroutine(lua_State *thread, int nargs) {
    // called from the fetch curl callback - just drive the coroutine forward
    step_coroutine(thread, nargs);
}