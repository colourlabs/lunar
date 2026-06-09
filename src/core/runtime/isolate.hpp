#pragma once
#include <functional>
#include <lua.hpp>
#include <string>
#include <unordered_map>

#include <uv.h>

#include "lunar_alloc.h"

namespace LunarCore {

struct PendingRequest {
    std::function<void(lua_State *)> m_on_result;
    int m_thread_ref = LUA_NOREF;
};

class Isolate {
  public:
    explicit Isolate(const std::string &worker_path, const std::string &std_path = "lunar");
    ~Isolate();

    Isolate(const Isolate &) = delete;
    Isolate &operator=(const Isolate &) = delete;

    Isolate(Isolate &&other) noexcept;
    Isolate &operator=(Isolate &&other) noexcept;

    void dispatch_async(uv_loop_t *loop, std::function<int(lua_State *)> push_args,
                        std::function<void(lua_State *)> on_result);

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

    std::unordered_map<lua_State *, PendingRequest> m_pending;

    bool load_worker(const std::string &path);

    void step_coroutine(lua_State *thread, int nargs);
};

} // namespace LunarCore