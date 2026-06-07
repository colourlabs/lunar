#pragma once

#include "lunar_limits.h"

#include <stddef.h> // NOLINT(modernize-deprecated-headers)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { // NOLINT(modernize-use-using)
    size_t m_used; // current bytes allocated
    size_t m_limit; // hardcap in bytes
    size_t m_peak; // highest usage ever seen
    size_t m_baseline; // memory used after init, before any requests
    LunarLimits* m_limits;
} LunarAllocState;

// passed directly to lua_newstate as the allocator function
void* lunar_alloc(void* user_data, void* pointer, size_t osize, size_t nsize);

#ifdef __cplusplus
}
#endif
