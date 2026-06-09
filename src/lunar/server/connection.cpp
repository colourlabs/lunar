#include "connection.hpp"
#include "pool/isolate_pool.hpp"
#include "utils/http.hpp"
#include "utils/logger.hpp"

namespace Lunar {

using LunarCore::Logger;

Connection::Connection(uv_loop_t *loop, IsolatePool *pool)
    : m_loop(loop), m_tcp(), m_parser(), m_settings(), m_pool(pool) {
    uv_tcp_init(m_loop, &m_tcp);
    m_tcp.data = this;

    llhttp_settings_init(&m_settings);
    m_settings.on_url = on_url;
    m_settings.on_header_field = on_header_field;
    m_settings.on_header_value = on_header_value;
    m_settings.on_body = on_body;
    m_settings.on_message_complete = on_message_complete;
    llhttp_init(&m_parser, HTTP_REQUEST, &m_settings);
    m_parser.data = this;
}

void Connection::create(uv_loop_t *loop, IsolatePool *pool, uv_stream_t *server) {
    auto *conn = new Connection(loop, pool);
    if (!conn->accept(server)) {
        delete conn;
    }
}

bool Connection::accept(uv_stream_t *server) {
    if (uv_accept(server, static_cast<uv_stream_t *>(static_cast<void *>(&m_tcp))) != 0) {
        return false;
    }
    uv_read_start(static_cast<uv_stream_t *>(static_cast<void *>(&m_tcp)), on_alloc, on_read);
    return true;
}

void Connection::on_alloc(uv_handle_t * /*unused*/, size_t suggested, uv_buf_t *buf) {
    buf->base = new char[suggested];
    buf->len = suggested;
}

void Connection::on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    auto *self = static_cast<Connection *>(stream->data);

    if (nread < 0) {
        delete[] buf->base;
        self->close();
        return;
    }

    if (nread > 0) {
        llhttp_errno_t err = llhttp_execute(&self->m_parser, buf->base, nread);
        if (err != HPE_OK) {
            Logger::error("http parse error: {}", llhttp_errno_name(err));
            delete[] buf->base;
            self->close();
            return;
        }
    }

    delete[] buf->base;
}

int Connection::on_url(llhttp_t *parser, const char *buf, size_t len) {
    auto *self = static_cast<Connection *>(parser->data);
    self->m_req.m_path.append(buf, len);
    return 0;
}

int Connection::on_header_field(llhttp_t *parser, const char *buf, size_t len) {
    auto *self = static_cast<Connection *>(parser->data);
    self->m_current_field.append(buf, len);
    return 0;
}

int Connection::on_header_value(llhttp_t *parser, const char *buf, size_t len) {
    auto *self = static_cast<Connection *>(parser->data);
    self->m_req.m_headers[self->m_current_field].append(buf, len);
    self->m_current_field.clear();
    return 0;
}

int Connection::on_body(llhttp_t *parser, const char *buf, size_t len) {
    auto *self = static_cast<Connection *>(parser->data);
    self->m_req.m_body.append(buf, len);
    return 0;
}

int Connection::on_message_complete(llhttp_t *parser) {
    auto *self = static_cast<Connection *>(parser->data);

    self->m_req.m_method = llhttp_method_name(static_cast<llhttp_method_t>(self->m_parser.method));
    self->m_current_field.clear();

    const std::string &raw = self->m_req.m_path;
    auto querypos = raw.find('?');
    if (querypos != std::string::npos) {
        std::string query_str = raw.substr(querypos + 1);
        self->m_req.m_path = url_decode(raw.substr(0, querypos));

        size_t pos = 0;
        while (pos < query_str.size()) {
            auto amp = query_str.find('&', pos);
            std::string pair =
                query_str.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
            auto equalpos = pair.find('=');
            if (equalpos != std::string::npos) {
                self->m_req.m_query[url_decode(pair.substr(0, equalpos))] =
                    url_decode(pair.substr(equalpos + 1));
            }
            if (amp == std::string::npos) {
                break;
            }
            pos = amp + 1;
        }
    } else {
        self->m_req.m_path = url_decode(raw);
    }

    uv_read_stop(static_cast<uv_stream_t *>(static_cast<void *>(&self->m_tcp)));

    LuaRequest req = std::move(self->m_req);
    self->m_req = {};

    self->m_pool->submit(
        req,
        [self](const LuaResponse &res) {
            self->write_response(res);
            uv_read_start(static_cast<uv_stream_t *>(static_cast<void *>(&self->m_tcp)),
                          on_alloc, on_read);
        },
        self->m_loop);

    return 0;
}

void Connection::write_response(const LuaResponse &res) {
    auto *str_buf = new std::string(build_response(res));

    auto *write_req = new uv_write_t;
    write_req->data = str_buf;

    uv_buf_t uvbuf = uv_buf_init(str_buf->data(), str_buf->size());
    uv_write(write_req, static_cast<uv_stream_t *>(static_cast<void *>(&m_tcp)), &uvbuf, 1,
             on_write_done);
}

void Connection::on_write_done(uv_write_t *write_req, int /*unused*/) {
    delete static_cast<std::string *>(write_req->data);
    delete write_req;
}

void Connection::close() {
    uv_close(static_cast<uv_handle_t *>(static_cast<void *>(&m_tcp)), on_close);
}

void Connection::on_close(uv_handle_t *handle) {
    delete static_cast<Connection *>(handle->data);
}

} // namespace Lunar