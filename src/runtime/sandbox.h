#pragma once

#include <string>

#include <lua.hpp>

class Sandbox {
  public:
    static void install(lua_State *m_lua_state, const std::string& std_path);

  private:
    static void open_safe_libs(lua_State *m_lua_state);
    static void install_require(lua_State *m_lua_state, const std::string& std_path);
    static void remove_unsafe_globals(lua_State *m_lua_state);
    static int lua_require(lua_State *m_lua_state);
};