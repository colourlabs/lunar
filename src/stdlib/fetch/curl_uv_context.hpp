#pragma once

#include <curl/curl.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <uv.h>

struct CurlTransfer {
    std::string m_body;
    std::string m_headers_raw;
    std::function<void(CURLcode, CurlTransfer)> m_on_done;
    long m_status = 0;
};

class CurlUvContext {
  public:
    explicit CurlUvContext(uv_loop_t *loop);
    ~CurlUvContext();

    CurlUvContext(const CurlUvContext &) = delete;
    CurlUvContext &operator=(const CurlUvContext &) = delete;
    CurlUvContext(CurlUvContext &&) = delete;
    CurlUvContext &operator=(CurlUvContext &&) = delete;

    void add(CURL *easy, std::function<void(CURLcode, CurlTransfer)> on_done);

    static CurlUvContext &get(uv_loop_t *loop);

  private:
    CURLM *m_multi = nullptr;
    uv_loop_t *m_loop = nullptr;
    uv_timer_t m_timer{};

    std::unordered_map<CURL *, CurlTransfer> m_transfers;

    static int handle_socket(CURL *easy, curl_socket_t socket, int action, void *userp,
                             void *socketp);
    static int handle_timer(CURLM *multi, long timeout_ms, void *userp);

    void check_multi_info(); // drain curl's message queue
    void on_timeout();
};