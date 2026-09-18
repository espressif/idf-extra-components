/* coap_mem.c -- CoAP memory handling
 *
 * Copyright (C) 2014--2015,2019--2026 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use.
 */

/**
 * @file coap_mem.c
 * @brief Memory handling functions
 */

#include "coap3/coap_libcoap_build.h"

#include "esp_heap_caps_init.h"

void
coap_memory_init(void)
{
}

void
coap_dump_memory_type_counts(coap_log_t level)
{
    (void)level;
}

#ifndef CONFIG_COAP_CUSTOM_MEM_ALLOC

void *
coap_malloc_type(coap_memory_tag_t type, size_t size)
{
    (void)type;
#ifdef CONFIG_COAP_INTERNAL_MEM_ALLOC
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif CONFIG_COAP_EXTERNAL_MEM_ALLOC
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif CONFIG_COAP_IRAM_8BIT_MEM_ALLOC
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_IRAM_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

void *
coap_realloc_type(coap_memory_tag_t type, void *p, size_t size)
{
    (void)type;
#ifdef CONFIG_COAP_INTERNAL_MEM_ALLOC
    return heap_caps_realloc(p, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif CONFIG_COAP_EXTERNAL_MEM_ALLOC
    return heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif CONFIG_COAP_IRAM_8BIT_MEM_ALLOC
    return heap_caps_realloc_prefer(p, size, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_IRAM_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    return realloc(p. size);
#endif
}

void
coap_free_type(coap_memory_tag_t type, void *p)
{
    (void)type;
    heap_caps_free(p);
}
#endif /* CONFIG_COAP_CUSTOM_MEM_ALLOC */
