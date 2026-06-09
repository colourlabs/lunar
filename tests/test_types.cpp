#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "runtime/isolate.hpp"
#include "types.hpp"

#include <lua.hpp>

TEST_CASE("push_request creates a Lua table from LuaRequest") {
    lua_State *L = luaL_newstate();
    luaopen_base(L);

    Lunar::LuaRequest req;
    req.m_method = "GET";
    req.m_path = "/hello";
    req.m_body = "";
    req.m_headers["Content-Type"] = "text/plain";
    req.m_query["q"] = "test";

    Lunar::push_request(L, req);

    REQUIRE(lua_istable(L, -1));

    lua_getfield(L, -1, "method");
    CHECK_EQ(lua_tostring(L, -1), std::string("GET"));
    lua_pop(L, 1);

    lua_getfield(L, -1, "path");
    CHECK_EQ(lua_tostring(L, -1), std::string("/hello"));
    lua_pop(L, 1);

    lua_getfield(L, -1, "headers");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "Content-Type");
    CHECK_EQ(lua_tostring(L, -1), std::string("text/plain"));
    lua_pop(L, 2);

    lua_getfield(L, -1, "query");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "q");
    CHECK_EQ(lua_tostring(L, -1), std::string("test"));
    lua_pop(L, 2);

    lua_pop(L, 1);
    lua_close(L);
}

TEST_CASE("read_response parses a Lua result table into LuaResponse") {
    lua_State *L = luaL_newstate();
    luaopen_base(L);

    lua_newtable(L);
    lua_pushinteger(L, 201);
    lua_setfield(L, -2, "status");
    lua_pushstring(L, "created");
    lua_setfield(L, -2, "body");
    lua_newtable(L);
    lua_pushstring(L, "application/json");
    lua_setfield(L, -2, "Content-Type");
    lua_setfield(L, -2, "headers");

    Lunar::LuaResponse res = Lunar::read_response(L);

    CHECK_EQ(res.m_status, 201);
    CHECK_EQ(res.m_body, "created");
    CHECK_EQ(res.m_headers["Content-Type"], "application/json");

    lua_pop(L, 1);
    lua_close(L);
}
