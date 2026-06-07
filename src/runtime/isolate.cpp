#include "isolate.hpp"
#include "lunar_limits.h"
#include "sandbox.hpp"

Isolate::Isolate(const std::string &worker_path, const std::string &std_path)
    : m_alloc_state({.m_used = 0, .m_limit = 128ULL * 1024 * 1024, .m_peak = 0, .m_baseline = 0}),
      m_lua_state(lua_newstate(lunar_alloc, &m_alloc_state, 0)) {
    if (m_lua_state == nullptr) {
        m_last_error = "failed to create Lua state (out of memory?)";
        return;
    }

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
    : m_alloc_state(other.m_alloc_state), m_lua_state(other.m_lua_state),
      m_last_error(std::move(other.m_last_error)), m_worker_env_ref(other.m_worker_env_ref) {
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

void Isolate::push_request(const LuaRequest &req) {
    // push req table
    lua_newtable(m_lua_state);

    // req.method
    lua_pushstring(m_lua_state, req.m_method.c_str());
    lua_setfield(m_lua_state, -2, "method");

    // req.path
    lua_pushstring(m_lua_state, req.m_path.c_str());
    lua_setfield(m_lua_state, -2, "path");

    // req.body
    lua_pushstring(m_lua_state, req.m_body.c_str());
    lua_setfield(m_lua_state, -2, "body");

    // req.headers
    lua_newtable(m_lua_state);
    for (const auto &[k, v] : req.m_headers) {
        lua_pushstring(m_lua_state, v.c_str());
        lua_setfield(m_lua_state, -2, k.c_str());
    }
    lua_setfield(m_lua_state, -2, "headers");

    // req.query
    lua_newtable(m_lua_state);
    for (const auto &[k, v] : req.m_query) {
        lua_pushstring(m_lua_state, v.c_str());
        lua_setfield(m_lua_state, -2, k.c_str());
    }
    lua_setfield(m_lua_state, -2, "query");
}

LuaResponse Isolate::read_response() {
    LuaResponse res;

    if (!lua_istable(m_lua_state, -1)) {
        m_last_error = "handle() must return a table";
        res.m_status = 500;
        res.m_body = m_last_error;
        return res;
    }

    // read status
    lua_getfield(m_lua_state, -1, "status");
    res.m_status =
        (lua_isinteger(m_lua_state, -1) != 0) ? (int)lua_tointeger(m_lua_state, -1) : 200;
    lua_pop(m_lua_state, 1);

    // read body
    lua_getfield(m_lua_state, -1, "body");
    res.m_body = (lua_isstring(m_lua_state, -1) != 0) ? lua_tostring(m_lua_state, -1) : "";
    lua_pop(m_lua_state, 1);

    // read headers
    lua_getfield(m_lua_state, -1, "headers");
    if (lua_istable(m_lua_state, -1)) {
        lua_pushnil(m_lua_state);
        while (lua_next(m_lua_state, -2) != 0) {
            if ((lua_isstring(m_lua_state, -2) != 0) && (lua_isstring(m_lua_state, -1) != 0)) {
                std::string key = lua_tostring(m_lua_state, -2);
                std::string value = lua_tostring(m_lua_state, -1);
                res.m_headers[key] = value;
            }
            lua_pop(m_lua_state, 1); // pop value, keep key for next iteration
        }
    }
    lua_pop(m_lua_state, 1); // pop headers table

    return res;
}

std::optional<LuaResponse> Isolate::dispatch(const LuaRequest &req) {
    static constexpr size_t max_body = 1024 * 1024; // 1MB
    static constexpr size_t max_header = 8 * 1024;  // 8KB per value
    static constexpr size_t max_headers = 64;

    if (req.m_body.size() > max_body) {
        m_last_error = "request body too large";
        return std::nullopt;
    }
    if (req.m_headers.size() > max_headers) {
        m_last_error = "too many request headers";
        return std::nullopt;
    }
    for (const auto &[k, v] : req.m_headers) {
        if (k.size() > max_header || v.size() > max_header) {
            m_last_error = "request header too large";
            return std::nullopt;
        }
    }

    // rest of dispatch unchanged
    lua_rawgeti(m_lua_state, LUA_REGISTRYINDEX, m_worker_env_ref);

    // fetch the worker's private environment table from the registry
    lua_rawgeti(m_lua_state, LUA_REGISTRYINDEX, m_worker_env_ref);

    // look up the "handle" function inside that environment table
    lua_getfield(m_lua_state, -1, "handle");

    // remove the environment table from the stack, leaving just the function
    lua_remove(m_lua_state, -2);

    if (!lua_isfunction(m_lua_state, -1)) {
        m_last_error = "handle() function not found in worker environment";
        lua_pop(m_lua_state, 1);
        return std::nullopt;
    }

    // push req table as argument
    push_request(req);

    // call handle(req)
    if (lua_pcall(m_lua_state, 1, 1, 0) != LUA_OK) {
        m_last_error = lua_tostring(m_lua_state, -1);
        lua_pop(m_lua_state, 1);

        printf("[lua] used=%zu peak=%zu limit=%zu\n", m_alloc_state.m_used, m_alloc_state.m_peak,
               m_alloc_state.m_limit);

        return std::nullopt;
    }

    LuaResponse res = read_response();
    lua_pop(m_lua_state, 1);

    printf("[lua] used=%zu peak=%zu limit=%zu\n", m_alloc_state.m_used, m_alloc_state.m_peak,
           m_alloc_state.m_limit);

    lua_gc(m_lua_state, LUA_GCCOLLECT, 0);

    printf("[after gc] used=%zu peak=%zu limit=%zu\n", m_alloc_state.m_used, m_alloc_state.m_peak,
           m_alloc_state.m_limit);

    return res;
}
