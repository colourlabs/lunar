#pragma once
#include <uv.h>
#include <string>

namespace Lunar {

class IsolatePool;

class Server {
public:
    Server(uv_loop_t *loop, IsolatePool *pool);
    bool listen(const std::string &host, int port);
    void run();

private:
    uv_loop_t *m_loop;
    uv_tcp_t m_tcp;
    IsolatePool *m_pool;

    static void on_connection(uv_stream_t *stream, int status);
};

} // namespace Lunar