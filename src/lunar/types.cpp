#include "types.hpp"

namespace Lunar {

void push_request(lua_State *thread, const LuaRequest &req) {
    lua_newtable(thread);

    lua_pushstring(thread, req.m_method.c_str());
    lua_setfield(thread, -2, "method");

    lua_pushstring(thread, req.m_path.c_str());
    lua_setfield(thread, -2, "path");

    lua_pushstring(thread, req.m_body.c_str());
    lua_setfield(thread, -2, "body");

    lua_newtable(thread);
    for (const auto &[k, v] : req.m_headers) {
        lua_pushstring(thread, v.c_str());
        lua_setfield(thread, -2, k.c_str());
    }
    lua_setfield(thread, -2, "headers");

    lua_newtable(thread);
    for (const auto &[k, v] : req.m_query) {
        lua_pushstring(thread, v.c_str());
        lua_setfield(thread, -2, k.c_str());
    }
    lua_setfield(thread, -2, "query");
}

LuaResponse read_response(lua_State *thread) {
    LuaResponse res;

    if (!lua_istable(thread, -1)) {
        res.m_status = 500;
        res.m_body = "handle() must return a table";
        return res;
    }

    lua_getfield(thread, -1, "status");
    res.m_status =
        (lua_isinteger(thread, -1) != 0) ? static_cast<int>(lua_tointeger(thread, -1)) : 200;
    lua_pop(thread, 1);

    lua_getfield(thread, -1, "body");
    res.m_body = (lua_isstring(thread, -1) != 0) ? lua_tostring(thread, -1) : "";
    lua_pop(thread, 1);

    lua_getfield(thread, -1, "headers");
    if (lua_istable(thread, -1)) {
        lua_pushnil(thread);
        while (lua_next(thread, -2) != 0) {
            if ((lua_isstring(thread, -2) != 0) && (lua_isstring(thread, -1) != 0)) {
                std::string key = lua_tostring(thread, -2);
                std::string value = lua_tostring(thread, -1);
                res.m_headers[key] = value;
            }
            lua_pop(thread, 1);
        }
    }
    lua_pop(thread, 1);

    return res;
}

} // namespace Lunar
