#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <uv.h>

#include "runtime/isolate.hpp"

class IsolatePool {
public:
    IsolatePool(const std::string &worker, const std::string &std_path, size_t count);

    void submit(const LuaRequest &req, std::function<void(LuaResponse)> callback,
                uv_loop_t *loop);

private:
    std::vector<std::unique_ptr<Isolate>> m_isolates;
    std::atomic<size_t> m_next = 0;
};