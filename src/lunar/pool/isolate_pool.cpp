#include "isolate_pool.hpp"

namespace Lunar {

using LunarCore::Isolate;

IsolatePool::IsolatePool(const std::string &worker, const std::string &std_path, size_t count) {
    size_t safe_count = (count == 0) ? 1 : count;
    m_isolates.reserve(safe_count);

    for (size_t i = 0; i < safe_count; i++) {
        auto isolate = std::make_unique<Isolate>(worker, std_path);
        if (!isolate->ok()) {
            throw std::runtime_error("failed to create isolate: " + isolate->error());
        }
        m_isolates.push_back(std::move(isolate));
    }
}

void IsolatePool::submit(const LuaRequest &req, std::function<void(LuaResponse)> callback,
                         uv_loop_t *loop) {
    size_t idx = m_next.fetch_add(1, std::memory_order_relaxed) % m_isolates.size();
    Isolate *isolate = m_isolates[idx].get();

    isolate->dispatch_async(
        loop,
        [req](lua_State *thread) -> int {
            push_request(thread, req);
            return 1;
        },
        [callback = std::move(callback)](lua_State *thread) {
            LuaResponse res = read_response(thread);
            callback(std::move(res));
        });
}

} // namespace Lunar