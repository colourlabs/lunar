#include "isolate_pool.hpp"

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

void IsolatePool::submit(LuaRequest req, std::function<void(LuaResponse)> callback,
                         uv_loop_t *loop) {
    size_t idx = m_next.fetch_add(1, std::memory_order_relaxed) % m_isolates.size();

    auto *item = new WorkItem{
        .m_req = std::move(req),
        .m_res = {},
        .m_callback = std::move(callback),
        .m_work = {},
        .m_isolate = m_isolates[idx].get(),
    };
    item->m_work.data = item;

    uv_queue_work(
        loop, &item->m_work,
        [](uv_work_t *work) {
            auto *item = static_cast<WorkItem *>(work->data);
            auto res = item->m_isolate->dispatch(item->m_req);
            item->m_res = res.value_or(
                LuaResponse{.m_status = 500, .m_headers = {}, .m_body = "internal error"});
        },
        [](uv_work_t *work, int) {
            auto *item = static_cast<WorkItem *>(work->data);
            item->m_callback(std::move(item->m_res));
            delete item;
        });
}