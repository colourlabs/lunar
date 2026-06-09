#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <yyjson.h>

namespace LunarCore {

static constexpr int max_depth = 64;

static void* get_empty_object_marker() {
    static char marker = 0;
    return &marker;
}

static void* get_empty_array_marker() {
    static char marker = 0;
    return &marker;
}

// decode: yyjson -> lua

static void push_yyjson_value(lua_State *m_lua_state, yyjson_val *val, int depth) {
    if (depth > max_depth) {
        luaL_error(m_lua_state, "json.decode: object too deeply nested (max %d)", max_depth);
    }

    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL:
        lua_pushnil(m_lua_state);
        break;

    case YYJSON_TYPE_BOOL:
        lua_pushboolean(m_lua_state, yyjson_get_bool(val) ? 1 : 0);
        break;

    case YYJSON_TYPE_NUM:
        if (yyjson_is_uint(val)) {
            lua_pushinteger(m_lua_state, static_cast<lua_Integer>(yyjson_get_uint(val)));
        } else if (yyjson_is_sint(val)) {
            lua_pushinteger(m_lua_state, static_cast<lua_Integer>(yyjson_get_sint(val)));
        } else {
            lua_pushnumber(m_lua_state, static_cast<lua_Number>(yyjson_get_real(val)));
        }
        break;

    case YYJSON_TYPE_STR:
        lua_pushlstring(m_lua_state, yyjson_get_str(val), yyjson_get_len(val));
        break;

    case YYJSON_TYPE_ARR: {
        size_t arr_size = yyjson_arr_size(val);
        lua_createtable(m_lua_state, static_cast<int>(arr_size), 0);
        yyjson_val *item = nullptr;
        yyjson_arr_iter iter = yyjson_arr_iter_with(val);
        int idx = 1;
        while ((item = yyjson_arr_iter_next(&iter)) != nullptr) {
            push_yyjson_value(m_lua_state, item, depth + 1);
            lua_rawseti(m_lua_state, -2, idx++);
        }
        break;
    }

    case YYJSON_TYPE_OBJ: {
        size_t obj_size = yyjson_obj_size(val);
        lua_createtable(m_lua_state, 0, static_cast<int>(obj_size));
        yyjson_val *key = nullptr;
        yyjson_val *obj_val = nullptr;
        yyjson_obj_iter obj_iter = yyjson_obj_iter_with(val);
        while ((key = yyjson_obj_iter_next(&obj_iter)) != nullptr) {
            obj_val = yyjson_obj_iter_get_val(key);
            lua_pushlstring(m_lua_state, yyjson_get_str(key), yyjson_get_len(key));
            push_yyjson_value(m_lua_state, obj_val, depth + 1);
            lua_rawset(m_lua_state, -3);
        }
        break;
    }

    default:
        lua_pushnil(m_lua_state);
        break;
    }
}

static int json_decode(lua_State *m_lua_state) {
    size_t len = 0;
    const char *str = luaL_checklstring(m_lua_state, 1, &len);

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts(
        // yyjson_read_opts takes char* but does not modify it without INSITU flag
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        const_cast<char *>(str), len, YYJSON_READ_NOFLAG, nullptr, &err);

    if (doc == nullptr) {
        return luaL_error(m_lua_state, "json.decode: %s (position %zu)", err.msg, err.pos);
    }

    push_yyjson_value(m_lua_state, yyjson_doc_get_root(doc), 0);
    yyjson_doc_free(doc);
    return 1;
}

// encode: Lua -> yyjson

static void encode_lua_value(lua_State *m_lua_state, yyjson_mut_doc *doc, yyjson_mut_val *parent,
                             const char *key, int idx, int depth);

static void encode_lua_number(lua_State *m_lua_state, yyjson_mut_doc *doc, yyjson_mut_val *parent,
                              const char *key, int idx) {
    yyjson_mut_val *num_val = nullptr;
    if (lua_isinteger(m_lua_state, idx) != 0) {
        num_val = yyjson_mut_int(doc, lua_tointeger(m_lua_state, idx));
    } else {
        double num = lua_tonumber(m_lua_state, idx);
        if (!std::isfinite(num)) {
            luaL_error(m_lua_state, "json.encode: cannot encode non-finite number");
        }
        num_val = yyjson_mut_real(doc, num);
    }
    if (key != nullptr) {
        yyjson_mut_obj_add_val(doc, parent, key, num_val);
    } else {
        yyjson_mut_arr_append(parent, num_val);
    }
}

static bool lua_table_is_array(lua_State *m_lua_state, int idx, lua_Integer &out_max) {
    out_max = 0;
    
    lua_pushnil(m_lua_state);
    bool is_empty = (lua_next(m_lua_state, idx) == 0);
    
    if (is_empty) {
        lua_getfield(m_lua_state, LUA_REGISTRYINDEX, "LUNAR_JSON_ARRAY_MT");
        
        if (lua_getmetatable(m_lua_state, idx) != 0) {
            bool has_array_mt = (lua_rawequal(m_lua_state, -1, -2) != 0);
            lua_pop(m_lua_state, 2);
            return has_array_mt;
        }
        
        lua_pop(m_lua_state, 1);
        return false;
    }

    lua_pushnil(m_lua_state);
    while (lua_next(m_lua_state, idx) != 0) {
        if (lua_isinteger(m_lua_state, -2) != 0) {
            lua_Integer table_key = lua_tointeger(m_lua_state, -2);
            if (table_key >= 1) {
                out_max = std::max(out_max, table_key);
                lua_pop(m_lua_state, 1);
                continue;
            }
        }
        lua_pop(m_lua_state, 2);
        return false;
    }

    lua_Integer count = 0;
    lua_pushnil(m_lua_state);
    while (lua_next(m_lua_state, idx) != 0) {
        count++;
        lua_pop(m_lua_state, 1);
    }
    return count == out_max;
}

static void encode_lua_value(lua_State *m_lua_state, yyjson_mut_doc *doc, yyjson_mut_val *parent,
                             const char *key, int idx, int depth) {
    if (depth > max_depth) {
        luaL_error(m_lua_state, "json.encode: object too deeply nested (max %d)", max_depth);
    }

    if (idx < 0) {
        idx = lua_gettop(m_lua_state) + idx + 1;
    }

    auto append = [&](yyjson_mut_val *val) {
        if (key != nullptr) {
            yyjson_mut_obj_add_val(doc, parent, key, val);
        } else {
            yyjson_mut_arr_append(parent, val);
        }
    };

    switch (lua_type(m_lua_state, idx)) {
    case LUA_TNIL:
        append(yyjson_mut_null(doc));
        break;

    case LUA_TBOOLEAN:
        append(yyjson_mut_bool(doc, lua_toboolean(m_lua_state, idx) != 0));
        break;

    case LUA_TNUMBER:
        encode_lua_number(m_lua_state, doc, parent, key, idx);
        break;

    case LUA_TSTRING: {
        size_t slen = 0;
        const char *str = lua_tolstring(m_lua_state, idx, &slen);
        append(yyjson_mut_strn(doc, str, slen));
        break;
    }

    case LUA_TTABLE: {
        luaL_checkstack(m_lua_state, 4, "json.encode: too deeply nested");
        
        lua_Integer max_n = 0;
        if (lua_table_is_array(m_lua_state, idx, max_n)) {
            yyjson_mut_val *arr = yyjson_mut_arr(doc);
            for (lua_Integer arr_idx = 1; arr_idx <= max_n; arr_idx++) {
                lua_rawgeti(m_lua_state, idx, arr_idx);
                encode_lua_value(m_lua_state, doc, arr, nullptr, -1, depth + 1);
                lua_pop(m_lua_state, 1);
            }
            append(arr);
        } else {
            yyjson_mut_val *obj = yyjson_mut_obj(doc);
            lua_pushnil(m_lua_state);
            while (lua_next(m_lua_state, idx) != 0) {
                const char *obj_key = nullptr;
                if (lua_type(m_lua_state, -2) == LUA_TSTRING) {
                    obj_key = lua_tostring(m_lua_state, -2);
                } else if (lua_isnumber(m_lua_state, -2) != 0) {
                    lua_pushvalue(m_lua_state, -2);
                    obj_key = lua_tostring(m_lua_state, -1);
                    lua_pop(m_lua_state, 1);
                } else {
                    luaL_error(m_lua_state, "json.encode: table key must be a string or number");
                }
                encode_lua_value(m_lua_state, doc, obj, obj_key, -1, depth + 1);
                lua_pop(m_lua_state, 1);
            }
            append(obj);
        }
        break;
    }

    case LUA_TLIGHTUSERDATA: {
        void* ptr = lua_touserdata(m_lua_state, idx);
        
        if (ptr == get_empty_object_marker()) {
            append(yyjson_mut_obj(doc));
        } else if (ptr == get_empty_array_marker()) {
            append(yyjson_mut_arr(doc));
        } else {
            luaL_error(m_lua_state, "json.encode: unrecognized light userdata");
        }
        break;
    }

    default:
        luaL_error(m_lua_state, "json.encode: cannot encode %s",
                   lua_typename(m_lua_state, lua_type(m_lua_state, idx)));
    }
}

static int json_encode(lua_State *m_lua_state) {
    luaL_checkany(m_lua_state, 1);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (doc == nullptr) {
        return luaL_error(m_lua_state, "json.encode: failed to create document");
    }

    // encode top-level value into a temporary array wrapper so we can get the root
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);
    encode_lua_value(m_lua_state, doc, arr, nullptr, 1, 0);

    // extract the single element as root
    yyjson_mut_val *root = yyjson_mut_arr_get_first(arr);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_write_err err;
    size_t out_len = 0;
    char *out = yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, nullptr, &out_len, &err);
    yyjson_mut_doc_free(doc);

    if (out == nullptr) {
        return luaL_error(m_lua_state, "json.encode: %s", err.msg);
    }

    lua_pushlstring(m_lua_state, out, out_len);
    free(out); // NOLINT(cppcoreguidelines-no-malloc)
    return 1;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
static const luaL_Reg json_lib[] = {
    {"encode", json_encode},
    {"decode", json_decode},
    {nullptr, nullptr},
};

int luaopen_json(lua_State *m_lua_state) {
    // NOLINTNEXTLINE(readability-math-missing-parentheses,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    luaL_newlib(m_lua_state, json_lib);

    lua_createtable(m_lua_state, 0, 1);
    lua_pushstring(m_lua_state, "array");
    lua_setfield(m_lua_state, -2, "__jsontype");
    
    lua_pushvalue(m_lua_state, -1);
    lua_setfield(m_lua_state, -3, "array_mt");
    
    lua_pushvalue(m_lua_state, -1);
    lua_setfield(m_lua_state, LUA_REGISTRYINDEX, "LUNAR_JSON_ARRAY_MT");

    lua_pop(m_lua_state, 1);

    lua_pushlightuserdata(m_lua_state, get_empty_object_marker());
    lua_setfield(m_lua_state, -2, "empty_object");

    lua_pushlightuserdata(m_lua_state, get_empty_array_marker());
    lua_setfield(m_lua_state, -2, "empty_array");

    return 1;
}

} // namespace LunarCore