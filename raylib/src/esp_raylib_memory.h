/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *esp_raylib_malloc(size_t size);
void *esp_raylib_calloc(size_t count, size_t size);
void *esp_raylib_realloc(void *ptr, size_t size);
void esp_raylib_free(void *ptr);

#ifdef __cplusplus
}
#endif

/* Route raylib's allocator macros to the functions above. */
#ifndef RL_MALLOC
#define RL_MALLOC(sz)        esp_raylib_malloc(sz)
#endif
#ifndef RL_CALLOC
#define RL_CALLOC(n, sz)     esp_raylib_calloc(n, sz)
#endif
#ifndef RL_REALLOC
#define RL_REALLOC(ptr, sz)  esp_raylib_realloc(ptr, sz)
#endif
#ifndef RL_FREE
#define RL_FREE(ptr)         esp_raylib_free(ptr)
#endif
