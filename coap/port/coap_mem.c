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

#if COAP_MEMORY_TYPE_TRACK
static int track_counts[COAP_MEM_TAG_LAST];
static int peak_counts[COAP_MEM_TAG_LAST];
static int fail_counts[COAP_MEM_TAG_LAST];
#endif /* COAP_MEMORY_TYPE_TRACK */

#include "esp_heap_caps_init.h"

#if ((CONFIG_SPIRAM || CONFIG_SPIRAM_SUPPORT) && \
        (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
#define MEM_ALLOC_SPIRAM(size)         heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define MEM_REALLOC_SPIRAM(ptr, size)  heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define MEM_FREE_SPIRAM(ptr)           heap_caps_free(ptr)
#else
#define MEM_ALLOC_SPIRAM(size)         malloc(size)
#define MEM_REALLOC_SPIRAM(ptr, size)  realloc(ptr, size)
#define MEM_FREE_SPIRAM(ptr)           free(ptr)
#endif

void
coap_memory_init(void)
{
}

void *
coap_malloc_type(coap_memory_tag_t type, size_t size)
{
    void *ptr;

    (void)type;
    ptr = MEM_ALLOC_SPIRAM(size);
#if COAP_MEMORY_TYPE_TRACK
    assert(type < COAP_MEM_TAG_LAST);
    if (ptr) {
        track_counts[type]++;
        if (track_counts[type] > peak_counts[type]) {
            peak_counts[type] = track_counts[type];
        }
    } else {
        fail_counts[type]++;
    }
#endif /* COAP_MEMORY_TYPE_TRACK */
    return ptr;
}

void *
coap_realloc_type(coap_memory_tag_t type, void *p, size_t size)
{
    void *ptr;

    (void)type;
    ptr = MEM_REALLOC_SPIRAM(p, size);
#if COAP_MEMORY_TYPE_TRACK
    if (ptr) {
        assert(type < COAP_MEM_TAG_LAST);
        if (!p) {
            track_counts[type]++;
        }
        if (track_counts[type] > peak_counts[type]) {
            peak_counts[type] = track_counts[type];
        }
    } else {
        fail_counts[type]++;
    }
#endif /* COAP_MEMORY_TYPE_TRACK */
    return ptr;
}

void
coap_free_type(coap_memory_tag_t type, void *p)
{
    (void)type;
#if COAP_MEMORY_TYPE_TRACK
    assert(type < COAP_MEM_TAG_LAST);
    if (p) {
        track_counts[type]--;
    }
#endif /* COAP_MEMORY_TYPE_TRACK */
    MEM_FREE_SPIRAM(p);
}

#define MAKE_CASE(n) case n: name = #n; break
void
coap_dump_memory_type_counts(coap_log_t level)
{
#if COAP_MEMORY_TYPE_TRACK
    int i;

    coap_log(level, "*  Memory type counts\n");
    for (i = 0; i < COAP_MEM_TAG_LAST; i++) {
        const char *name = "?";


        switch (i) {
            MAKE_CASE(COAP_STRING);
            MAKE_CASE(COAP_ATTRIBUTE_NAME);
            MAKE_CASE(COAP_ATTRIBUTE_VALUE);
            MAKE_CASE(COAP_PACKET);
            MAKE_CASE(COAP_NODE);
            MAKE_CASE(COAP_CONTEXT);
            MAKE_CASE(COAP_ENDPOINT);
            MAKE_CASE(COAP_PDU);
            MAKE_CASE(COAP_PDU_BUF);
            MAKE_CASE(COAP_RESOURCE);
            MAKE_CASE(COAP_RESOURCEATTR);
            MAKE_CASE(COAP_DTLS_SESSION);
            MAKE_CASE(COAP_SESSION);
            MAKE_CASE(COAP_OPTLIST);
            MAKE_CASE(COAP_CACHE_KEY);
            MAKE_CASE(COAP_CACHE_ENTRY);
            MAKE_CASE(COAP_LG_XMIT);
            MAKE_CASE(COAP_LG_CRCV);
            MAKE_CASE(COAP_LG_SRCV);
            MAKE_CASE(COAP_DIGEST_CTX);
            MAKE_CASE(COAP_SUBSCRIPTION);
            MAKE_CASE(COAP_DTLS_CONTEXT);
            MAKE_CASE(COAP_OSCORE_COM);
            MAKE_CASE(COAP_OSCORE_SEN);
            MAKE_CASE(COAP_OSCORE_REC);
            MAKE_CASE(COAP_OSCORE_EX);
            MAKE_CASE(COAP_OSCORE_EP);
            MAKE_CASE(COAP_OSCORE_BUF);
            MAKE_CASE(COAP_COSE);
        case COAP_MEM_TAG_LAST:
        default:
            break;
        }
        coap_log(level, "*    %-20s in-use %3d peak %3d failed %2d\n",
                 name, track_counts[i], peak_counts[i], fail_counts[i]);
    }
#else /* COAP_MEMORY_TYPE_TRACK */
    (void)level;
#endif /* COAP_MEMORY_TYPE_TRACK */
}
