#pragma once

#include <string>

#include <lua.hpp>

namespace LunarCore {

class Sandbox {
  public:
    static void install(lua_State *m_lua_state, const std::string& std_path);

  private:
    static void open_safe_libs(lua_State *m_lua_state);
    static void install_require(lua_State *m_lua_state, const std::string& std_path);
    static void remove_unsafe_globals(lua_State *m_lua_state);
    static int lua_require(lua_State *m_lua_state);

    static void lock_string_metatable(lua_State *m_lua_state);
    static void lock_string_lib(lua_State *m_lua_state);
    static void lock_math_lib(lua_State *m_lua_state);
    static void lock_table_lib(lua_State *m_lua_state);
    static void register_c_module(lua_State *m_lua_state, const char *name, lua_CFunction function);
};

} // namespace LunarCore