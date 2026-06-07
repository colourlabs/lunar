#include "lunar_alloc.h"
#include <stdio.h>
#include <stdlib.h>

// stored in the lua_State's ud pointer via lua_newstate
// retrieved via lua_getallocf
void *lunar_alloc(void *user_data, void *pointer, size_t osize, size_t nsize) {
    if (user_data == NULL) {
        return realloc(pointer, nsize);
    }

    LunarAllocState *state = (LunarAllocState *)user_data;
    if (state == NULL) {
        return realloc(pointer, nsize);
    }

    if (nsize == 0) {
        if (pointer != NULL) {
            if (osize > state->m_used - state->m_baseline) {
                state->m_used = state->m_baseline;
            } else {
                state->m_used -= osize;
            }
            free(pointer);
        }
        return NULL;
    }

    if (pointer == NULL) {
        osize = 0; 
    }

    size_t new_used = state->m_used;

    if (nsize > osize) {
        size_t delta = nsize - osize;
        size_t headroom = state->m_limit - state->m_baseline;
        size_t current_above_baseline = new_used - state->m_baseline;
        if (delta > headroom || current_above_baseline > headroom - delta) {
            return NULL;
        }
        new_used += delta;
    } else {
        size_t delta = osize - nsize;
        if (delta > state->m_used) {
            state->m_used = 0;
        } else {
            state->m_used -= delta;
        }
        
        if (state->m_used < state->m_baseline) {
            new_used = state->m_baseline;
        } else {
            new_used = state->m_used;
        }
    }

    void *result = realloc(pointer, nsize);

    if (result == NULL) {
        if (nsize < osize) {
            return pointer;
        }
        return NULL;
    }

    state->m_used = new_used;
    if (state->m_used > state->m_peak) {
        state->m_peak = state->m_used;
    }
    return result;
}
