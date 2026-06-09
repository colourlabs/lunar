#include "curl_uv_context.hpp"
#include "fetch_limits.hpp"

#include <stdexcept>

namespace LunarCore {

static auto &get_instances() {
    static std::unordered_map<uv_loop_t *, CurlUvContext *> instances;
    return instances;
}

CurlUvContext &CurlUvContext::get(uv_loop_t *loop) {
    auto &instances = get_instances();
    auto found = instances.find(loop);
    if (found != instances.end()) {
        return *found->second;
    }
    auto *ctx = new CurlUvContext(loop);
    instances[loop] = ctx;
    return *ctx;
}

// write callbacks

static size_t write_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *xfer = static_cast<CurlTransfer *>(userdata);
    size_t total = size * nmemb;
    if (xfer->m_body.size() + total > static_cast<size_t>(max_response_body)) {
        return 0;
    }
    xfer->m_body.append(ptr, total);
    return total;
}

static size_t write_headers(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *xfer = static_cast<CurlTransfer *>(userdata);
    size_t total = size * nmemb;
    if (xfer->m_headers_raw.size() + total > static_cast<size_t>(max_response_headers)) {
        return total;
    }
    xfer->m_headers_raw.append(ptr, total);
    return total;
}

// constructor / destructor

CurlUvContext::CurlUvContext(uv_loop_t *loop) : m_multi(curl_multi_init()), m_loop(loop) {
    if (m_multi == nullptr) {
        throw std::runtime_error("curl_multi_init failed");
    }

    curl_multi_setopt(m_multi, CURLMOPT_SOCKETFUNCTION, handle_socket);
    curl_multi_setopt(m_multi, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(m_multi, CURLMOPT_TIMERFUNCTION, handle_timer);
    curl_multi_setopt(m_multi, CURLMOPT_TIMERDATA, this);

    uv_timer_init(loop, &m_timer);
    m_timer.data = this;
}

CurlUvContext::~CurlUvContext() {
    uv_timer_stop(&m_timer);

    for (auto &[easy, xfer] : m_transfers) {
        curl_multi_remove_handle(m_multi, easy);
        curl_easy_cleanup(easy);
        xfer.m_on_done(CURLE_ABORTED_BY_CALLBACK, std::move(xfer));
    }
    m_transfers.clear();

    curl_multi_cleanup(m_multi);

    get_instances().erase(m_loop);
}

// add

void CurlUvContext::add(CURL *easy, std::function<void(CURLcode, CurlTransfer)> on_done) {
    auto [iter, inserted] = m_transfers.emplace(easy, CurlTransfer{});
    iter->second.m_on_done = std::move(on_done);

    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &iter->second);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, write_headers);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &iter->second);

    curl_multi_add_handle(m_multi, easy);
}

// curl socket callback

int CurlUvContext::handle_socket(CURL * /*easy*/, curl_socket_t sock, int action, void *userp,
                                 void *socketp) {
    auto *self = static_cast<CurlUvContext *>(userp);
    auto *poll = static_cast<uv_poll_t *>(socketp);

    if (action == CURL_POLL_REMOVE) {
        if (poll != nullptr) {
            uv_poll_stop(poll);
            uv_close( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<uv_handle_t *>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    poll),               
                [](uv_handle_t *handle) {
                    delete reinterpret_cast<uv_poll_t *>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                        handle); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                });
            curl_multi_assign(self->m_multi, sock, nullptr);
        }
        return 0;
    }

    if (poll == nullptr) {
        poll = new uv_poll_t;
        uv_poll_init_socket(self->m_loop, poll, sock);
        poll->data = self;
        curl_multi_assign(self->m_multi, sock, poll);
    }

    int events = 0;
    if ((action & CURL_POLL_IN) != 0) {
        events |= UV_READABLE;
    }
    if ((action & CURL_POLL_OUT) != 0) {
        events |= UV_WRITABLE;
    }

    uv_poll_start(poll, events, [](uv_poll_t *handle, int status, int revents) {
        auto *ctx = static_cast<CurlUvContext *>(handle->data);

        int flags = 0;
        if (status < 0) {
            flags = CURL_CSELECT_ERR;
        }
        if ((revents & UV_READABLE) != 0) {
            flags |= CURL_CSELECT_IN;
        }
        if ((revents & UV_WRITABLE) != 0) {
            flags |= CURL_CSELECT_OUT;
        }

        uv_os_fd_t file_descriptor = {};
        // NOLINT: reinterpret_cast required by libuv's C API
        uv_fileno( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            reinterpret_cast<uv_handle_t *>(handle), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            &file_descriptor); 

        int running = 0;
        curl_multi_socket_action(ctx->m_multi, static_cast<curl_socket_t>(file_descriptor), flags,
                                 &running);
        ctx->check_multi_info();
    });

    return 0;
}

// curl timer callback

int CurlUvContext::handle_timer(CURLM * /*multi*/, long timeout_ms, void *userp) {
    auto *self = static_cast<CurlUvContext *>(userp);

    if (timeout_ms < 0) {
        uv_timer_stop(&self->m_timer);
        return 0;
    }

    uv_timer_start(
        &self->m_timer,
        [](uv_timer_t *timer) {
            auto *ctx = static_cast<CurlUvContext *>(timer->data);
            int running = 0;
            curl_multi_socket_action(ctx->m_multi, CURL_SOCKET_TIMEOUT, 0, &running);
            ctx->check_multi_info();
        },
        timeout_ms == 0 ? 1 : timeout_ms, 0);

    return 0;
}

// drain curl message queue

void CurlUvContext::check_multi_info() {
    int pending = 0;
    CURLMsg *msg = nullptr;
    while ((msg = curl_multi_info_read(m_multi, &pending)) != nullptr) {
        if (msg->msg != CURLMSG_DONE) {
            continue;
        }

        CURL *easy = msg->easy_handle;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        CURLcode result = msg->data.result;

        auto found = m_transfers.find(easy);
        if (found != m_transfers.end()) {
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &found->second.m_status);
        }

        curl_multi_remove_handle(m_multi, easy);

        if (found != m_transfers.end()) {
            // extract on_done first, then move the rest of the transfer
            auto on_done = std::move(found->second.m_on_done);
            auto xfer = std::move(found->second);
            m_transfers.erase(found);
            curl_easy_cleanup(easy);
            on_done(result, std::move(xfer));
        } else {
            curl_easy_cleanup(easy);
        }
    }
}

} // namespace LunarCore