#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "lua.h"

#ifdef __cplusplus
extern "C" {
#endif

// limits configuration per lua_State
// set these before loading any user code
typedef struct { // NOLINT(modernize-use-using)
    // instruction counting
    unsigned int m_instruction_count;
    unsigned int m_instruction_limit;

    // string limits
    size_t m_max_string_len;
    int    m_max_string_count;

    // coroutine limits
    int m_coroutine_count;
    int m_coroutine_limit;

    // pattern match complexity (ReDoS protection)
    int m_pattern_steps;
    int m_pattern_step_limit;
} LunarLimits;

// retrieve the limits struct from a lua_State
// returns NULL if not installed
LunarLimits* lunar_get_limits(lua_State* m_lua_state);

// install limits into a lua_State
// must be called before loading user code
bool lunar_install_limits(lua_State* m_lua_state);

// instruction limit
void lunar_set_instruction_limit(lua_State* m_lua_state, unsigned int limit);
void lunar_reset_instruction_count(lua_State* m_lua_state);
unsigned int lunar_get_instruction_count(lua_State* m_lua_state);

// string limits
void lunar_set_string_limits(lua_State* m_lua_state, size_t max_len, int max_count);

// coroutine limits
void lunar_set_coroutine_limit(lua_State* m_lua_state, int limit);
void lunar_coroutine_inc(lua_State* m_lua_state);
void lunar_coroutine_dec(lua_State* m_lua_state);

// pattern match limits
void lunar_set_pattern_limit(lua_State* m_lua_state, int max_steps);
void lunar_reset_pattern_steps(lua_State* m_lua_state);
void lunar_check_pattern_steps(lua_State* m_lua_state);

// destroys limits
void lunar_destroy_limits(lua_State *m_lua_state);

#ifdef __cplusplus
}
#endif
