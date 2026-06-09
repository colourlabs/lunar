#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "runtime/sandbox.hpp"
#include "stdlib/json/json.hpp"

#include <lua.hpp>

TEST_CASE("json.encode produces valid JSON from Lua table") {
    lua_State *L = luaL_newstate();
    LunarCore::Sandbox::install(L, "src/lunar/lua");

    lua_getglobal(L, "require");
    lua_pushstring(L, "lunar/json");
    REQUIRE_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    lua_setglobal(L, "json");

    lua_getglobal(L, "json");
    lua_getfield(L, -1, "encode");
    lua_newtable(L);
    lua_pushstring(L, "world");
    lua_setfield(L, -2, "hello");
    REQUIRE_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    CHECK_EQ(lua_tostring(L, -1), std::string(R"({"hello":"world"})"));
    lua_pop(L, 2);

    lua_close(L);
}

TEST_CASE("json.decode parses JSON into Lua table") {
    lua_State *L = luaL_newstate();
    LunarCore::Sandbox::install(L, "src/lunar/lua");

    lua_getglobal(L, "require");
    lua_pushstring(L, "lunar/json");
    REQUIRE_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    lua_setglobal(L, "json");

    lua_getglobal(L, "json");
    lua_getfield(L, -1, "decode");
    lua_pushstring(L, R"({"foo":42})");
    REQUIRE_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "foo");
    CHECK_EQ(lua_tointeger(L, -1), 42);
    lua_pop(L, 3);

    lua_close(L);
}
