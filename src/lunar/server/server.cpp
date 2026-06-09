#include "server.hpp"
#include "connection.hpp"
#include "utils/logger.hpp"

namespace Lunar {

using LunarCore::Logger;

Server::Server(uv_loop_t *loop, IsolatePool *pool) : m_loop(loop), m_tcp{}, m_pool(pool) {
    uv_tcp_init(m_loop, &m_tcp);
    m_tcp.data = this;
}

bool Server::listen(const std::string &host, int port) {
    sockaddr_in addr{};
    uv_ip4_addr(host.c_str(), port, &addr);

    uv_tcp_bind(&m_tcp, static_cast<const sockaddr *>(static_cast<const void *>(&addr)), 0);

    if (int result =
            uv_listen(static_cast<uv_stream_t *>(static_cast<void *>(&m_tcp)), 128, on_connection);
        result != 0) {
        Logger::error("listen failed: {}", uv_strerror(result));
        return false;
    }

    Logger::info("listening on http://{}:{}", host, port);
    return true;
}

void Server::run() {
    uv_run(m_loop, UV_RUN_DEFAULT);
}

void Server::on_connection(uv_stream_t *stream, int status) {
    if (status < 0) {
        Logger::error("connection error: {}", uv_strerror(status));
        return;
    }
    auto *self = static_cast<Server *>(stream->data);
    Connection::create(self->m_loop, self->m_pool, stream);
}

} // namespace Lunar