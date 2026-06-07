#pragma once

#include "runtime/isolate.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <uv.h>
#include <vector>

class IsolatePool {
  public:
    IsolatePool(const std::string &worker, const std::string &std_path, size_t count);
    
    IsolatePool(const IsolatePool&) = delete;
    IsolatePool& operator=(const IsolatePool&) = delete;
    IsolatePool(IsolatePool&&) = delete;
    IsolatePool& operator=(IsolatePool&&) = delete;

    ~IsolatePool() = default;

    void submit(LuaRequest req, std::function<void(LuaResponse)> callback, uv_loop_t *loop);

  private:
    std::vector<std::unique_ptr<Isolate>> m_isolates;
    std::atomic<size_t> m_next{0};

    struct WorkItem {
        LuaRequest m_req;
        LuaResponse m_res;
        std::function<void(LuaResponse)> m_callback;
        uv_work_t m_work;
        Isolate *m_isolate;
    };
};