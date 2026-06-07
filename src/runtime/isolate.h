#pragma once

#include <map>
#include <optional>
#include <string>

#include <lua.hpp>

// base Request and Response
struct LuaRequest {
    std::string m_method;
    std::string m_path;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_query;
    std::string m_body;
};

struct LuaResponse {
    int m_status;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
};

class Isolate {
  public:
    explicit Isolate(const std::string &worker_path, const std::string &std_path = "lunar");
    ~Isolate();
    Isolate &operator=(Isolate &&other) noexcept;

    // no copying - lua_State is not copyable
    Isolate(const Isolate &) = delete;
    Isolate &operator=(const Isolate &) = delete;

    // moving is fine
    Isolate(Isolate &&other) noexcept;

    std::optional<LuaResponse> dispatch(const LuaRequest &req);
    [[nodiscard]] bool ok() const { return m_lua_state != nullptr; }
    [[nodiscard]] const std::string &error() const { return m_last_error; }

  private:
    lua_State *m_lua_state = nullptr;
    std::string m_last_error;
    int m_worker_env_ref = -1;

    bool load_worker(const std::string &path);
    void push_request(const LuaRequest &req);
    LuaResponse read_response();
};