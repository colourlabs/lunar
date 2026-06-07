#include "lunar_limits.h"
#include "lauxlib.h"
#include <stdbool.h>
#include <stdlib.h>

enum {
    DEFAULT_INSTRUCTION_LIMIT = 50000000,
    DEFAULT_MAX_STRING_COUNT = 100000,
    DEFAULT_COROUTINE_LIMIT = 1000,
    DEFAULT_PATTERN_STEP_LIMIT = 100000,
};

static const size_t default_max_string_len = 1024 * 1024;

bool lunar_install_limits(lua_State *m_lua_state) {
    LunarLimits *limits = (LunarLimits *)malloc(sizeof(LunarLimits));
    if (limits == NULL) {
        return false;  // caller sets m_last_error and closes state cleanly
    }

    limits->m_instruction_count   = 0;
    limits->m_instruction_limit   = DEFAULT_INSTRUCTION_LIMIT;
    limits->m_max_string_len      = default_max_string_len;
    limits->m_max_string_count    = DEFAULT_MAX_STRING_COUNT;
    limits->m_coroutine_count     = 0;
    limits->m_coroutine_limit     = DEFAULT_COROUTINE_LIMIT;
    limits->m_pattern_steps       = 0;
    limits->m_pattern_step_limit  = DEFAULT_PATTERN_STEP_LIMIT;

    *(LunarLimits **)lua_getextraspace(m_lua_state) = limits;
    return true;
}

LunarLimits *lunar_get_limits(lua_State *m_lua_state) {
    return *(LunarLimits **)lua_getextraspace(m_lua_state);
}

void lunar_destroy_limits(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        *(LunarLimits **)lua_getextraspace(m_lua_state) = NULL;
        free(limits);
    }
}

void lunar_set_instruction_limit(lua_State *m_lua_state, unsigned int limit) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        limits->m_instruction_limit = limit;
    }
}

void lunar_reset_instruction_count(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        limits->m_instruction_count = 0;
    }
}

unsigned int lunar_get_instruction_count(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    return limits != NULL ? limits->m_instruction_count : 0;
}

void lunar_set_string_limits(lua_State *m_lua_state, size_t max_len, int max_count) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        limits->m_max_string_len = max_len;
        limits->m_max_string_count = max_count;
    }
}

void lunar_set_coroutine_limit(lua_State *m_lua_state, int limit) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        limits->m_coroutine_limit = limit;
    }
}

void lunar_coroutine_inc(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits == NULL) {
        return;
    }
    limits->m_coroutine_count++;
    if (limits->m_coroutine_count > limits->m_coroutine_limit) {
        luaL_error(m_lua_state, "coroutine limit exceeded (max %d)", limits->m_coroutine_limit);
    }
}

void lunar_coroutine_dec(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL && limits->m_coroutine_count > 0) {
        limits->m_coroutine_count--;
    }
}

void lunar_set_pattern_limit(lua_State *m_lua_state, int max_steps) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        limits->m_pattern_step_limit = max_steps;
    }
}

void lunar_reset_pattern_steps(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits != NULL) {
        limits->m_pattern_steps = 0;
    }
}

void lunar_check_pattern_steps(lua_State *m_lua_state) {
    LunarLimits *limits = lunar_get_limits(m_lua_state);
    if (limits == NULL) {
        return;
    }

    limits->m_pattern_steps++;

    if (limits->m_pattern_steps > limits->m_pattern_step_limit) {
        luaL_error(m_lua_state, "pattern match too complex (possible ReDoS)");
    }
}
