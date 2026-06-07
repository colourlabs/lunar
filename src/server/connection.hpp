#pragma once
#include "runtime/isolate.hpp"
#include <llhttp.h>
#include <string>
#include <uv.h>

class IsolatePool;

class Connection {
  public:
    bool accept(uv_stream_t *server);
    static void create(uv_loop_t *loop, IsolatePool *pool, uv_stream_t *server);

  private:
    Connection(uv_loop_t *loop, IsolatePool *pool);

    uv_loop_t *m_loop;
    uv_tcp_t m_tcp;
    llhttp_t m_parser;
    llhttp_settings_t m_settings;
    IsolatePool *m_pool;

    // built up by parser callbacks
    LuaRequest m_req;
    std::string m_current_field;

    void write_response(const LuaResponse &res);
    void close();

    static void on_alloc(uv_handle_t *handle, size_t suggested, uv_buf_t *buf);
    static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void on_write_done(uv_write_t *req, int status);
    static void on_close(uv_handle_t *handle);

    // llhttp callbacks
    static int on_url(llhttp_t *parser, const char *buf, size_t len);
    static int on_header_field(llhttp_t *parser, const char *buf, size_t len);
    static int on_header_value(llhttp_t *parser, const char *buf, size_t len);
    static int on_body(llhttp_t *parser, const char *buf, size_t len);
    static int on_message_complete(llhttp_t *parser);
};