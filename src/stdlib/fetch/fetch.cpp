#include "fetch.hpp"
#include "curl_uv_context.hpp"
#include "runtime/isolate.hpp"

#include <cstring>
#include <curl/curl.h>
#include <string>
#include <uv.h>
#include <vector>

static constexpr long default_timeout_ms = 10000;
static constexpr long max_timeout_ms = 30000;

// curl write callbacks
struct FetchResponse {
    std::string m_body;
    std::string m_headers_raw;
    long m_status = 0;
};

struct FetchContinuation {
    lua_State  *m_thread;
    uv_loop_t  *m_loop;
    Isolate    *m_isolate;
    curl_slist *m_headers;
    std::string m_body_buf;
};

// header parsing

static void parse_headers(lua_State *m_lua_state, const std::string &raw) {
    lua_newtable(m_lua_state);

    size_t pos = 0;
    while (pos < raw.size()) {
        auto end = raw.find("\r\n", pos);
        if (end == std::string::npos) {
            end = raw.size();
        }

        std::string line = raw.substr(pos, end - pos);
        pos = end + 2;

        // skip status line and empty lines
        if (line.empty() || line.substr(0, 5) == "HTTP/") {
            continue;
        }

        auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // trim leading whitespace from value
        size_t value_start = value.find_first_not_of(' ');
        if (value_start != std::string::npos) {
            value = value.substr(value_start);
        }

        // lowercase the key
        for (char &chr : key) {
            chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
        }

        lua_pushlstring(m_lua_state, key.c_str(), key.size());
        lua_pushlstring(m_lua_state, value.c_str(), value.size());
        lua_rawset(m_lua_state, -3);
    }
}

// response metatable

static int fetch_response_json(lua_State *m_lua_state) {
    luaL_checktype(m_lua_state, 1, LUA_TTABLE);
    lua_getfield(m_lua_state, 1, "body");
    if (lua_isstring(m_lua_state, -1) == 0) {
        return luaL_error(m_lua_state, "fetch: response body is not a string");
    }
    // reuse json.decode by calling it via the module cache
    lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_modules");
    lua_getfield(m_lua_state, -1, "lunar/json");
    lua_remove(m_lua_state, -2);
    if (!lua_istable(m_lua_state, -1)) {
        return luaL_error(m_lua_state, "fetch: lunar/json not loaded");
    }
    lua_getfield(m_lua_state, -1, "decode");
    lua_remove(m_lua_state, -2);
    lua_pushvalue(m_lua_state, -2); // push body string
    lua_call(m_lua_state, 1, 1);
    return 1;
}

static void push_response(lua_State *m_lua_state, const FetchResponse &res) {
    lua_newtable(m_lua_state);

    // status
    lua_pushinteger(m_lua_state, res.m_status);
    lua_setfield(m_lua_state, -2, "status");

    // ok
    lua_pushboolean(m_lua_state, (res.m_status >= 200 && res.m_status < 300) ? 1 : 0);
    lua_setfield(m_lua_state, -2, "ok");

    // body
    lua_pushlstring(m_lua_state, res.m_body.c_str(), res.m_body.size());
    lua_setfield(m_lua_state, -2, "body");

    // headers
    parse_headers(m_lua_state, res.m_headers_raw);
    lua_setfield(m_lua_state, -2, "headers");

    // :json() method
    lua_pushcfunction(m_lua_state, fetch_response_json);
    lua_setfield(m_lua_state, -2, "json");
}

// fetch options

struct FetchOptions {
    std::string m_method = "GET";
    std::string m_body;
    std::vector<std::string> m_headers;
    long m_timeout = default_timeout_ms;
    bool m_follow_redirects = true;
};

static FetchOptions parse_options(lua_State *m_lua_state, int idx) {
    FetchOptions opts;

    if (!lua_istable(m_lua_state, idx)) {
        return opts;
    }

    // method
    lua_getfield(m_lua_state, idx, "method");
    if (lua_isstring(m_lua_state, -1) != 0) {
        opts.m_method = lua_tostring(m_lua_state, -1);
        // uppercase it
        for (char &chr : opts.m_method) {
            chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
        }
    }
    lua_pop(m_lua_state, 1);

    // body
    lua_getfield(m_lua_state, idx, "body");
    if (lua_isstring(m_lua_state, -1) != 0) {
        size_t blen = 0;
        const char *bstr = lua_tolstring(m_lua_state, -1, &blen);
        opts.m_body.assign(bstr, blen);
    }
    lua_pop(m_lua_state, 1);

    // timeout
    lua_getfield(m_lua_state, idx, "timeout");
    if (lua_isinteger(m_lua_state, -1) != 0) {
        opts.m_timeout =
            std::min(static_cast<long>(lua_tointeger(m_lua_state, -1)), max_timeout_ms);
    }
    lua_pop(m_lua_state, 1);

    // follow_redirects
    lua_getfield(m_lua_state, idx, "follow_redirects");
    if (lua_isboolean(m_lua_state, -1)) {
        opts.m_follow_redirects = (lua_toboolean(m_lua_state, -1) != 0);
    }
    lua_pop(m_lua_state, 1);

    // headers table
    lua_getfield(m_lua_state, idx, "headers");
    if (lua_istable(m_lua_state, -1)) {
        lua_pushnil(m_lua_state);
        while (lua_next(m_lua_state, -2) != 0) {
            if ((lua_isstring(m_lua_state, -2) != 0) && (lua_isstring(m_lua_state, -1) != 0)) {
                std::string header = std::string(lua_tostring(m_lua_state, -2)) + ": " +
                                     std::string(lua_tostring(m_lua_state, -1));
                opts.m_headers.push_back(std::move(header));
            }
            lua_pop(m_lua_state, 1);
        }
    }
    lua_pop(m_lua_state, 1);

    return opts;
}

static int lua_fetch(lua_State *m_lua_state) {
    const char *url = luaL_checkstring(m_lua_state, 1);
    FetchOptions opts = parse_options(m_lua_state, 2);

    lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_uv_loop");
    if (!lua_islightuserdata(m_lua_state, -1)) {
        lua_pop(m_lua_state, 1);
        return luaL_error(m_lua_state, "fetch: no event loop available");
    }
    auto *loop = static_cast<uv_loop_t *>(lua_touserdata(m_lua_state, -1));
    lua_pop(m_lua_state, 1);

    lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_isolate");
    if (!lua_islightuserdata(m_lua_state, -1)) {
        lua_pop(m_lua_state, 1);
        return luaL_error(m_lua_state, "fetch: no isolate available");
    }
    auto *isolate = static_cast<Isolate *>(lua_touserdata(m_lua_state, -1));
    lua_pop(m_lua_state, 1);

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        return luaL_error(m_lua_state, "fetch: failed to init curl");
    }

    curl_slist *headers = nullptr;
    for (const auto &header : opts.m_headers) {
        headers = curl_slist_append(headers, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, opts.m_timeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, opts.m_follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    if (headers != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (opts.m_method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(opts.m_body.size()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, opts.m_body.c_str());
    } else if (opts.m_method == "PUT" || opts.m_method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, opts.m_method.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(opts.m_body.size()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, opts.m_body.c_str());
    } else if (opts.m_method != "GET" && opts.m_method != "HEAD") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, opts.m_method.c_str());
    }

    auto *cont = new FetchContinuation{
        .m_thread   = m_lua_state,
        .m_loop     = loop,
        .m_isolate  = isolate,
        .m_headers  = headers,
        .m_body_buf = opts.m_body,
    };

    // re-point curl at the stable copy inside the continuation
    if (opts.m_method == "POST" || opts.m_method == "PUT" || opts.m_method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cont->m_body_buf.c_str());
    }

    CurlUvContext::get(loop).add(curl, [cont](CURLcode code, CurlTransfer xfer) {
        curl_slist_free_all(cont->m_headers);

        lua_State *thread  = cont->m_thread;
        Isolate   *isolate = cont->m_isolate;
        delete cont;

        if (code != CURLE_OK) {
            lua_pushnil(thread);
            lua_pushstring(thread, curl_easy_strerror(code));
            isolate->resume_coroutine(thread, 2);
            return;
        }

        FetchResponse fres{
            .m_body        = std::move(xfer.m_body),
            .m_headers_raw = std::move(xfer.m_headers_raw),
            .m_status      = xfer.m_status,
        };
        push_response(thread, fres);
        isolate->resume_coroutine(thread, 1);
    });

    return lua_yield(m_lua_state, 0);
}

int luaopen_fetch(lua_State *m_lua_state) {
    lua_pushcfunction(m_lua_state, lua_fetch);
    return 1;
}