#include "sandbox.hpp"

#include <array>
#include <filesystem>
#include <string>

static const std::array safe_libs = {
    luaL_Reg{"_G", luaopen_base},
    luaL_Reg{"string", luaopen_string},
    luaL_Reg{"table", luaopen_table},
    luaL_Reg{"math", luaopen_math},
    luaL_Reg{"coroutine", luaopen_coroutine},
    luaL_Reg{"utf8", luaopen_utf8},
};

void Sandbox::install(lua_State *m_lua_state, const std::string &std_path) {
    open_safe_libs(m_lua_state);
    remove_unsafe_globals(m_lua_state);
    
    // make certain stuff readonly
    lock_string_metatable(m_lua_state);
    lock_string_lib(m_lua_state);
    lock_math_lib(m_lua_state);
    lock_table_lib(m_lua_state);

    // install custom require
    install_require(m_lua_state, std_path);
}

void Sandbox::open_safe_libs(lua_State *m_lua_state) {
    for (const auto &lib : safe_libs) {
        luaL_requiref(m_lua_state, lib.name, lib.func, 1);
        lua_pop(m_lua_state, 1);
    }
}

void Sandbox::remove_unsafe_globals(lua_State *m_lua_state) {
    // these should not be accessible even if somehow loaded
    static constexpr std::array unsafe = {
        "dofile", "loadfile",
        "load",   // can compile arbitrary bytecode
        "rawget", // can bypass metatables
        "rawset", "rawequal", "rawlen", "collectgarbage",
    };

    for (const auto &name : unsafe) {
        lua_pushnil(m_lua_state);
        lua_setglobal(m_lua_state, name);
    }
}

void Sandbox::install_require(lua_State *m_lua_state, const std::string &std_path) {
    // store std_path in registry, not as a global
    lua_pushstring(m_lua_state, std_path.c_str());
    lua_setfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_std_path");

    // module cache in registry too
    lua_newtable(m_lua_state);
    lua_setfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_modules");

    lua_pushcfunction(m_lua_state, Sandbox::lua_require);
    lua_setglobal(m_lua_state, "require");
}

int Sandbox::lua_require(lua_State *m_lua_state) {
    const char *name = luaL_checkstring(m_lua_state, 1);

    // check cache first
    lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_modules");
    lua_getfield(m_lua_state, -1, name);

    if (!lua_isnil(m_lua_state, -1)) {
        // already loaded, return cached value
        lua_remove(m_lua_state, -2); // remove modules table
        return 1;
    }

    lua_pop(m_lua_state, 2); // pop nil + modules table

    // only allow lunar/ modules
    std::string module_name(name);
    if (module_name.substr(0, 6) != "lunar/") {
        return luaL_error(m_lua_state, "module '%s' not allowed (only lunar/ modules permitted)",
                          name);
    }

    // build file path: lunar/router -> {std_path}/router.lua
    lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_std_path");
    std::string std_path = lua_tostring(m_lua_state, -1);
    lua_pop(m_lua_state, 1);

    std::string relative = module_name.substr(6); // strip "lunar/"
    std::filesystem::path file_path = std::filesystem::path(std_path) / (relative + ".lua");

    // path traversal check
    auto canonical_std = std::filesystem::weakly_canonical(std_path);
    auto canonical_file = std::filesystem::weakly_canonical(file_path);
    if (canonical_file.string().substr(0, canonical_std.string().size()) !=
        canonical_std.string()) {
        return luaL_error(m_lua_state, "module '%s' is outside std path", name);
    }

    // check file exists
    if (!std::filesystem::exists(file_path)) {
        return luaL_error(m_lua_state, "module '%s' not found at %s", name, file_path.c_str());
    }

    // load and execute the module file
    if (luaL_dofile(m_lua_state, file_path.c_str()) != LUA_OK) {
        return luaL_error(m_lua_state, "error loading module '%s': %s", name,
                          lua_tostring(m_lua_state, -1));
    }

    // module must return a table
    if (!lua_istable(m_lua_state, -1)) {
        return luaL_error(m_lua_state, "module '%s' must return a table", name);
    }

    // cache it
    lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "lunar_modules");
    lua_pushvalue(m_lua_state, -2);
    lua_setfield(m_lua_state, -2, name);
    lua_pop(m_lua_state, 1);

    // return module table
    return 1;
}

void Sandbox::lock_string_metatable(lua_State *m_lua_state) {
    lua_pushliteral(m_lua_state, ""); // stack: [""]
    lua_getmetatable(m_lua_state, -1); // stack: ["", metatable]
    lua_remove(m_lua_state, -2); // stack: [metatable]  -- remove the string, keep metatable

    lua_pushboolean(m_lua_state, 0);
    lua_setfield(m_lua_state, -2, "__metatable");  // stack: [metatable]

    lua_pushcfunction(m_lua_state, [](lua_State *m_lua_state) -> int {
        return luaL_error(m_lua_state, "attempt to modify read-only string library");
    });
    lua_setfield(m_lua_state, -2, "__newindex"); // stack: [metatable]

    lua_pop(m_lua_state, 1); // stack: []
}

void Sandbox::lock_string_lib(lua_State *m_lua_state) {
    lua_getglobal(m_lua_state, "string");

    lua_newtable(m_lua_state); 
    lua_newtable(m_lua_state);

    lua_pushvalue(m_lua_state, -3);
    lua_setfield(m_lua_state, -2, "__index");

    lua_pushcfunction(m_lua_state, [](lua_State *m_lua_state) -> int {
        return luaL_error(m_lua_state, "attempt to modify read-only string library");
    });
    lua_setfield(m_lua_state, -2, "__newindex");

    lua_pushboolean(m_lua_state, 0);
    lua_setfield(m_lua_state, -2, "__metatable");

    lua_setmetatable(m_lua_state, -2); 

    lua_setglobal(m_lua_state, "string");
    lua_pop(m_lua_state, 1);
}

void Sandbox::lock_math_lib(lua_State *m_lua_state) {
    lua_getglobal(m_lua_state, "math");

    lua_newtable(m_lua_state);
    lua_newtable(m_lua_state);

    lua_pushvalue(m_lua_state, -3);
    lua_setfield(m_lua_state, -2, "__index");

    lua_pushcfunction(m_lua_state, [](lua_State *m_lua_state) -> int {
        return luaL_error(m_lua_state, "attempt to modify read-only math library");
    });
    lua_setfield(m_lua_state, -2, "__newindex");

    lua_pushboolean(m_lua_state, 0);
    lua_setfield(m_lua_state, -2, "__metatable");

    lua_setmetatable(m_lua_state, -2);

    lua_setglobal(m_lua_state, "math");
    lua_pop(m_lua_state, 1);
}

void Sandbox::lock_table_lib(lua_State *m_lua_state) {
    lua_getglobal(m_lua_state, "table");

    lua_newtable(m_lua_state);
    lua_newtable(m_lua_state);

    lua_pushvalue(m_lua_state, -3);
    lua_setfield(m_lua_state, -2, "__index");

    lua_pushcfunction(m_lua_state, [](lua_State *m_lua_state) -> int {
        return luaL_error(m_lua_state, "attempt to modify read-only table library");
    });
    lua_setfield(m_lua_state, -2, "__newindex");

    lua_pushboolean(m_lua_state, 0);
    lua_setfield(m_lua_state, -2, "__metatable");

    lua_setmetatable(m_lua_state, -2);

    lua_setglobal(m_lua_state, "table");
    lua_pop(m_lua_state, 1);
}