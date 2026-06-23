/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_heartbeat.c
 * @brief Per-node reachability tracking driven by the heartbeats nodes
 *        publish at their configured period. One FreeRTOS auto-reload
 *        software timer posts a tick to the manager task; the task walks
 *        the directory and flips reachability after N consecutive misses.
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "blemesh_internal.h"

#define TAG "blemesh_hb"

static void heartbeat_timer_cb(TimerHandle_t t)
{
    (void)t;
    blemesh_msg_t m = { .kind = BLEMESH_MSG_HEARTBEAT_TICK };
    (void)blemesh_post(&m);
}

void blemesh_heartbeat_init(void)
{
    g_blemesh_ctx.heartbeat_timer = NULL;
}

void blemesh_heartbeat_start(void)
{
    if (g_blemesh_ctx.heartbeat_timer != NULL) {
        return;
    }
    TickType_t period = pdMS_TO_TICKS(g_blemesh_ctx.cfg.heartbeat_period_ms);
    if (period == 0) {
        period = 1;
    }
    g_blemesh_ctx.heartbeat_timer = xTimerCreate("ble_mgr_hb",
                                    period,
                                    pdTRUE, NULL,
                                    heartbeat_timer_cb);
    if (g_blemesh_ctx.heartbeat_timer == NULL) {
        ESP_LOGE(TAG, "heartbeat timer create failed");
        return;
    }
    xTimerStart(g_blemesh_ctx.heartbeat_timer, 0);
}

void blemesh_heartbeat_stop(void)
{
    if (g_blemesh_ctx.heartbeat_timer != NULL) {
        xTimerStop(g_blemesh_ctx.heartbeat_timer, 0);
        xTimerDelete(g_blemesh_ctx.heartbeat_timer, 0);
        g_blemesh_ctx.heartbeat_timer = NULL;
    }
}

void blemesh_heartbeat_on_packet(blemesh_addr_t addr)
{
    blemesh_node_entry_t *e = blemesh_dir_find(addr);
    if (e == NULL) {
        return;
    }
    e->missed_heartbeats = 0;
    if (!e->info.reachable) {
        e->info.reachable = true;
        ESP_LOGI(TAG, "node 0x%04x reachable", addr);
        if (g_blemesh_ctx.cb.on_reachability) {
            g_blemesh_ctx.cb.on_reachability(addr, true);
        }
        /* Leaf may have rebooted while we still cached it as reachable=true,
         * fallen below the loss threshold, and now come back — its actual
         * state may have drifted from what the manager remembers (e.g. user
         * pressed the physical button, or the leaf restored persisted
         * state different from the manager's last known value). Re-query
         * via the same paced Get pipeline used at fresh-provisioning and
         * NVS-restore time so subsequent consumer writes hit the right
         * baseline instead of forcing whatever the manager happened to
         * remember. */
        blemesh_initial_sync_enqueue(addr, e->info.profile);
    }
}

void blemesh_heartbeat_on_tick(void)
{
    uint8_t thresh = g_blemesh_ctx.cfg.heartbeat_loss_threshold;
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
        blemesh_node_entry_t *e = &g_blemesh_ctx.nodes[i];
        if (!e->used) {
            continue;
        }
        if (e->missed_heartbeats < UINT8_MAX) {
            e->missed_heartbeats++;
        }
        if (e->info.reachable && e->missed_heartbeats >= thresh) {
            e->info.reachable = false;
            ESP_LOGW(TAG, "node 0x%04x unreachable (%u missed)",
                     e->info.addr, e->missed_heartbeats);
            if (g_blemesh_ctx.cb.on_reachability) {
                g_blemesh_ctx.cb.on_reachability(e->info.addr, false);
            }
        }
    }
}
