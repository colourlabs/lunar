#pragma once
#include <functional>
#include <lua.hpp>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include <uv.h>

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

struct PendingRequest {
    std::function<void(std::optional<LuaResponse>)> m_on_done;
    int m_thread_ref = LUA_NOREF; // registry ref keeping the coroutine alive
};

class Isolate {
  public:
    explicit Isolate(const std::string &worker_path, const std::string &std_path = "lunar");
    ~Isolate();

    Isolate(const Isolate &) = delete;
    Isolate &operator=(const Isolate &) = delete;

    Isolate(Isolate &&other) noexcept;
    Isolate &operator=(Isolate &&other) noexcept;

    // async dispatch - must be called from the uv loop thread
    void dispatch_async(const LuaRequest &req, uv_loop_t *loop,
                        std::function<void(std::optional<LuaResponse>)> on_done);

    // called by the fetch callback when a coroutine is ready to resume
    void resume_coroutine(lua_State *thread, int nargs);

    [[nodiscard]] bool ok() const { return m_lua_state != nullptr; }
    [[nodiscard]] const std::string &error() const { return m_last_error; }

    [[nodiscard]] size_t memory_used() const {
        size_t used = m_alloc_state.m_used;
        size_t base = m_alloc_state.m_baseline;
        return used > base ? used - base : 0;
    }
    [[nodiscard]] size_t memory_peak() const { return m_alloc_state.m_peak; }

  private:
    LunarLimits m_limits;
    LunarAllocState m_alloc_state;
    lua_State *m_lua_state = nullptr;
    std::string m_last_error;
    int m_worker_env_ref = LUA_NOREF;

    // coroutine -> pending request, for resuming after fetch completes
    std::unordered_map<lua_State *, PendingRequest> m_pending;

    bool load_worker(const std::string &path);
    static void push_request(lua_State *thread, const LuaRequest &req);
    LuaResponse read_response(lua_State *thread);

    // drives a coroutine forward; calls on_done when it finishes or errors
    void step_coroutine(lua_State *thread, int nargs);
};