#pragma once

#include <lua.hpp>
#include <map>
#include <string>

namespace Lunar {

struct LuaRequest {
    std::string m_method;
    std::string m_path;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_query;
    std::string m_body;
};

struct LuaResponse {
    int m_status = 200;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
};

void push_request(lua_State *thread, const LuaRequest &req);
LuaResponse read_response(lua_State *thread);

} // namespace Lunar
