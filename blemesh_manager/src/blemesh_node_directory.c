/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_node_directory.c
 * @brief Volatile, in-memory directory of provisioned nodes. Linear-scan over
 *        a fixed-size array — fine for the expected fleet size and avoids
 *        dynamic allocation in the hot path. Only mutated from the manager
 *        task.
 */

#include <string.h>

#include "esp_ble_mesh_defs.h"

#include "blemesh_internal.h"

uint16_t blemesh_primary_model_for_profile(blemesh_device_profile_t profile)
{
    switch (profile) {
    case BLEMESH_DEV_ONOFF:
        return ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV;
    case BLEMESH_DEV_DIMMABLE:
        /* Lightness-bearing dimmables key on Light Lightness Server. A
         * level-only dimmable has no match here and falls back to a single
         * logical element, which is correct for the single-channel case. */
        return ESP_BLE_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV;
    case BLEMESH_DEV_COLOR_TEMP:
        return ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV;
    case BLEMESH_DEV_COLOR_HSL:
        return ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SRV;
    case BLEMESH_DEV_OCCUPANCY:
    case BLEMESH_DEV_TEMPERATURE:
        return ESP_BLE_MESH_MODEL_ID_SENSOR_SRV;
    default:
        return 0; /* vendor/unknown -> single logical element at the primary element */
    }
}

void blemesh_comp_logical_elem_offsets(const uint8_t *comp_data, size_t comp_len,
                                       blemesh_device_profile_t profile,
                                       uint8_t *offsets, uint8_t *count)
{
    *count = 0;
    uint16_t primary = blemesh_primary_model_for_profile(profile);

    /* Composition Data Page 0: CID/PID/VID/CRPL/Features (10 bytes), then per
     * element Loc(2) NumS(1) NumV(1) [SIG ids x2] [vendor ids x4]. Collect the
     * offset of every element carrying the primary SIG model — each becomes a
     * logical element. */
    if (primary != 0 && comp_data != NULL && comp_len >= 10) {
        size_t  off  = 10;
        uint8_t elem = 0;
        while (off + 4 <= comp_len) {
            uint8_t nums = comp_data[off + 2];
            uint8_t numv = comp_data[off + 3];
            off += 4;
            bool found = false;
            for (uint8_t i = 0; i < nums && off + 2 <= comp_len; i++, off += 2) {
                uint16_t id = (uint16_t)(comp_data[off] | ((uint16_t)comp_data[off + 1] << 8));
                if (id == primary) {
                    found = true;
                }
            }
            off += (size_t)numv * 4u;
            if (found && *count < BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE) {
                offsets[(*count)++] = elem;
            }
            elem++;
        }
    }

    if (*count == 0) {
        offsets[0] = 0;
        *count     = 1;
    }
}

uint8_t blemesh_comp_ctl_temp_offset(const uint8_t *comp_data, size_t comp_len)
{
    /* Composition Data Page 0: CID/PID/VID/CRPL/Features (10 bytes), then per
     * element Loc(2) NumS(1) NumV(1) [SIG ids x2] [vendor ids x4]. Return the
     * offset of the first element carrying the Light CTL Temperature Server. */
    if (comp_data == NULL || comp_len < 10) {
        return 0;
    }
    size_t  off  = 10;
    uint8_t elem = 0;
    while (off + 4 <= comp_len) {
        uint8_t nums = comp_data[off + 2];
        uint8_t numv = comp_data[off + 3];
        off += 4;
        for (uint8_t i = 0; i < nums && off + 2 <= comp_len; i++, off += 2) {
            uint16_t id = (uint16_t)(comp_data[off] | ((uint16_t)comp_data[off + 1] << 8));
            if (id == ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_TEMP_SRV) {
                return elem;
            }
        }
        off += (size_t)numv * 4u;
        elem++;
    }
    return 0;
}

void blemesh_comp_hsl_offsets(const uint8_t *comp_data, size_t comp_len,
                              uint8_t *hue_off, uint8_t *sat_off)
{
    *hue_off = 0;
    *sat_off = 0;
    if (comp_data == NULL || comp_len < 10) {
        return;
    }
    size_t  off  = 10;
    uint8_t elem = 0;
    while (off + 4 <= comp_len) {
        uint8_t nums = comp_data[off + 2];
        uint8_t numv = comp_data[off + 3];
        off += 4;
        for (uint8_t i = 0; i < nums && off + 2 <= comp_len; i++, off += 2) {
            uint16_t id = (uint16_t)(comp_data[off] | ((uint16_t)comp_data[off + 1] << 8));
            if (id == ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_HUE_SRV && *hue_off == 0) {
                *hue_off = elem;
            } else if (id == ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SAT_SRV && *sat_off == 0) {
                *sat_off = elem;
            }
        }
        off += (size_t)numv * 4u;
        elem++;
    }
}

void blemesh_dir_clear(void)
{
    memset(g_blemesh_ctx.nodes, 0, sizeof(g_blemesh_ctx.nodes));
}

blemesh_node_entry_t *blemesh_dir_find(blemesh_addr_t addr)
{
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
        if (g_blemesh_ctx.nodes[i].used && g_blemesh_ctx.nodes[i].info.addr == addr) {
            return &g_blemesh_ctx.nodes[i];
        }
    }
    return NULL;
}

blemesh_node_entry_t *blemesh_dir_find_by_element(blemesh_addr_t addr)
{
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
        blemesh_node_entry_t *e = &g_blemesh_ctx.nodes[i];
        if (!e->used) {
            continue;
        }
        uint8_t count = (e->info.element_count > 0) ? e->info.element_count : 1;
        if (addr >= e->info.addr && addr < (blemesh_addr_t)(e->info.addr + count)) {
            return e;
        }
    }
    return NULL;
}

blemesh_node_entry_t *blemesh_dir_insert(const blemesh_node_info_t *info)
{
    blemesh_node_entry_t *existing = blemesh_dir_find(info->addr);
    if (existing) {
        /* Re-configuring a node already in the directory: refresh its
         * descriptive info but preserve the live reachability flag. The
         * caller's *info carries reachable=false (it describes composition,
         * not liveness), so a blind copy would mark an online node offline. */
        bool reachable = existing->info.reachable;
        existing->info = *info;
        existing->info.reachable = reachable;
        return existing;
    }
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
        if (!g_blemesh_ctx.nodes[i].used) {
            g_blemesh_ctx.nodes[i].used               = true;
            g_blemesh_ctx.nodes[i].info               = *info;
            g_blemesh_ctx.nodes[i].info.reachable     = true;
            g_blemesh_ctx.nodes[i].missed_heartbeats  = 0;
            g_blemesh_ctx.nodes[i].provisioned_ts_ms  = (uint32_t)blemesh_now_ms();
            return &g_blemesh_ctx.nodes[i];
        }
    }
    return NULL;
}

esp_err_t blemesh_dir_remove(blemesh_addr_t addr)
{
    blemesh_node_entry_t *e = blemesh_dir_find(addr);
    if (!e) {
        return ESP_ERR_NOT_FOUND;
    }
    memset(e, 0, sizeof(*e));
    return ESP_OK;
}

size_t blemesh_dir_count(void)
{
    size_t n = 0;
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
        if (g_blemesh_ctx.nodes[i].used) {
            n++;
        }
    }
    return n;
}

void blemesh_dir_snapshot(blemesh_node_info_t *out, size_t *count)
{
    size_t cap = *count;
    size_t n = 0;
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
        if (!g_blemesh_ctx.nodes[i].used) {
            continue;
        }
        if (out && n < cap) {
            out[n] = g_blemesh_ctx.nodes[i].info;
        }
        n++;
    }
    *count = n;
}
