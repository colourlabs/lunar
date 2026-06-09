#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "runtime/sandbox.hpp"

TEST_CASE("Sandbox install opens safe libs and removes unsafe globals") {
    lua_State *L = luaL_newstate();
    LunarCore::Sandbox::install(L, "src/lunar/lua");

    // safe globals should exist
    lua_getglobal(L, "print");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "string");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "math");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "table");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);

    // unsafe globals should be nil
    lua_getglobal(L, "dofile");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "loadfile");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "load");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "collectgarbage");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_close(L);
}

TEST_CASE("Sandbox install creates a working require for lunar/ modules") {
    lua_State *L = luaL_newstate();
    LunarCore::Sandbox::install(L, "src/lunar/lua");

    // require("lunar/router") should work
    lua_getglobal(L, "require");
    REQUIRE(lua_isfunction(L, -1));
    lua_pushstring(L, "lunar/router");
    CHECK_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);

    lua_close(L);
}
