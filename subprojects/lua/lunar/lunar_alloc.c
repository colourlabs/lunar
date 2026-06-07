#include "lunar_alloc.h"
#include <stdlib.h>


// stored in the lua_State's ud pointer via lua_newstate
// retrieved via lua_getallocf

void* lunar_alloc(void* user_data, void* pointer, size_t osize, size_t nsize) {
    LunarAllocState* state = (LunarAllocState*)user_data;

    // freeing
    if (nsize == 0) {
        if (pointer != NULL) {
            state->m_used -= osize;
            free(pointer);
        }
        return NULL;
    }

    // calculate new usage
    size_t new_used = state->m_used + nsize - osize;

    // enforce limit
    if (new_used > state->m_limit) {
        return NULL;  // Lua raises a memory error on NULL
    }

    void* result = realloc(pointer, nsize);

    if (result != NULL) {
        state->m_used = new_used;
        if (state->m_used > state->m_peak) {
            state->m_peak = state->m_used;
        }
    }

    return result;
}
