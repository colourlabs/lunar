#pragma once
#include <map>
#include <optional>
#include <string>
#include <lua.hpp>

#include "lunar_alloc.h"

// request and response
struct LuaRequest {
    std::string m_method;
    std::string m_path;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_query;
    std::string m_body;
};

struct LuaResponse {
    int m_status = 200;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
};

// actual isolate
class Isolate {
public:
    explicit Isolate(const std::string &worker_path, const std::string &std_path = "lunar");
    ~Isolate();

    Isolate(const Isolate &) = delete;
    Isolate &operator=(const Isolate &) = delete;

    Isolate(Isolate &&other) noexcept;
    Isolate &operator=(Isolate &&other) noexcept;

    std::optional<LuaResponse> dispatch(const LuaRequest &req);

    [[nodiscard]] bool ok() const { return m_lua_state != nullptr; }
    [[nodiscard]] const std::string &error() const { return m_last_error; }

    // expose allocator stats for monitoring
    [[nodiscard]] size_t memory_used() const { return m_alloc_state.m_used; }
    [[nodiscard]] size_t memory_peak() const { return m_alloc_state.m_peak; }

private:
    LunarAllocState m_alloc_state;
    lua_State *m_lua_state = nullptr;
    std::string m_last_error;
    int m_worker_env_ref = LUA_NOREF;

    bool load_worker(const std::string &path);
    void push_request(const LuaRequest &req);
    LuaResponse read_response();
};