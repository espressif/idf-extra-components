/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_pending_confirm.c
 * @brief Single-timer arbitration for Set → Status confirmation. Holds an
 *        unsorted array of (addr, state, deadline, retries_left, kind); a
 *        single FreeRTOS one-shot software timer is always (re-)armed to
 *        the earliest deadline. Single-threaded — accessed only from the
 *        manager task plus one timer callback that just posts a tick.
 */

#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "blemesh_internal.h"

#define TAG "blemesh_pend"

static void pending_timer_cb(TimerHandle_t t)
{
    (void)t;
    blemesh_msg_t m = { .kind = BLEMESH_MSG_PENDING_TICK };
    (void)blemesh_post(&m);
}

static blemesh_pending_entry_t *find_entry(blemesh_addr_t addr,
        blemesh_state_id_t state_id)
{
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE; i++) {
        blemesh_pending_entry_t *e = &g_blemesh_ctx.pending[i];
        if (e->used && e->addr == addr && e->state_id == state_id) {
            return e;
        }
    }
    return NULL;
}

static blemesh_pending_entry_t *alloc_entry(void)
{
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE; i++) {
        if (!g_blemesh_ctx.pending[i].used) {
            return &g_blemesh_ctx.pending[i];
        }
    }
    return NULL;
}

static uint64_t earliest_deadline(void)
{
    uint64_t best = UINT64_MAX;
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE; i++) {
        const blemesh_pending_entry_t *e = &g_blemesh_ctx.pending[i];
        if (e->used && e->deadline_ms < best) {
            best = e->deadline_ms;
        }
    }
    return best;
}

static void ensure_timer(void)
{
    if (g_blemesh_ctx.pending_timer != NULL) {
        return;
    }
    g_blemesh_ctx.pending_timer = xTimerCreate("ble_mgr_pend",
                                  pdMS_TO_TICKS(1000),
                                  pdFALSE, NULL,
                                  pending_timer_cb);
}

void blemesh_pending_init(void)
{
    memset(g_blemesh_ctx.pending, 0, sizeof(g_blemesh_ctx.pending));
    if (g_blemesh_ctx.pending_timer != NULL) {
        xTimerStop(g_blemesh_ctx.pending_timer, 0);
    }
}

void blemesh_pending_rearm(void)
{
    uint64_t deadline = earliest_deadline();
    if (deadline == UINT64_MAX) {
        if (g_blemesh_ctx.pending_timer != NULL) {
            xTimerStop(g_blemesh_ctx.pending_timer, 0);
        }
        return;
    }
    ensure_timer();
    if (g_blemesh_ctx.pending_timer == NULL) {
        ESP_LOGE(TAG, "timer create failed");
        return;
    }
    uint64_t now = blemesh_now_ms();
    uint64_t delay_ms = (deadline > now) ? (deadline - now) : 1;
    TickType_t period = pdMS_TO_TICKS((TickType_t)delay_ms);
    if (period == 0) {
        period = 1;
    }
    /* xTimerChangePeriod auto-restarts the timer. */
    xTimerChangePeriod(g_blemesh_ctx.pending_timer, period, 0);
}

void blemesh_pending_upsert(blemesh_addr_t addr,
                            blemesh_state_id_t state_id,
                            const blemesh_state_value_t *val,
                            uint64_t timeout_ms,
                            uint8_t retries)
{
    blemesh_pending_entry_t *e = find_entry(addr, state_id);
    if (e == NULL) {
        e = alloc_entry();
        if (e == NULL) {
            ESP_LOGE(TAG, "pending table full (addr=0x%04x state=%d)", addr, (int)state_id);
            return;
        }
        e->used     = true;
        e->addr     = addr;
        e->state_id = state_id;
    }
    e->kind         = BLEMESH_PENDING_KIND_SET;
    e->retries_left = retries;
    e->deadline_ms  = blemesh_now_ms() + timeout_ms;
    if (val) {
        e->last_value = *val;
    }
    blemesh_pending_rearm();
}

void blemesh_pending_clear(blemesh_addr_t addr, blemesh_state_id_t state_id)
{
    blemesh_pending_entry_t *e = find_entry(addr, state_id);
    if (e) {
        memset(e, 0, sizeof(*e));
        blemesh_pending_rearm();
    }
}

void blemesh_pending_clear_all_for(blemesh_addr_t addr)
{
    bool changed = false;
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE; i++) {
        blemesh_pending_entry_t *e = &g_blemesh_ctx.pending[i];
        if (e->used && e->addr == addr) {
            memset(e, 0, sizeof(*e));
            changed = true;
        }
    }
    if (changed) {
        blemesh_pending_rearm();
    }
}

void blemesh_pending_on_tick(void)
{
    uint64_t now = blemesh_now_ms();
    for (size_t i = 0; i < CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE; i++) {
        blemesh_pending_entry_t *e = &g_blemesh_ctx.pending[i];
        if (!e->used) {
            continue;
        }
        if (e->deadline_ms > now) {
            continue;
        }
        if (e->kind == BLEMESH_PENDING_KIND_SET && e->retries_left > 0) {
            /* No confirming Status yet. Sets go out unacknowledged with no
             * model-layer retransmit, so the most likely cause is the Set was
             * dropped on-air (radio contention). Re-send it before giving up —
             * this is what makes an OnOff Off actually stick instead of a stale
             * Get snapping the consumer's cached value back to its old value. */
            ESP_LOGD(TAG, "Set deadline elapsed (addr=0x%04x state=%d) — re-sending Set (%u left)",
                     e->addr, (int)e->state_id, (unsigned)e->retries_left);
            (void)blemesh_dispatcher_send_set(e->addr, &e->last_value, 0);
            e->retries_left--;
            e->deadline_ms  = now + g_blemesh_ctx.cfg.set_confirm_timeout_ms;
        } else if (e->kind == BLEMESH_PENDING_KIND_SET) {
            /* Re-sends exhausted: fall back to a single Get so the manager at
             * least learns (and reports) the leaf's actual state. */
            ESP_LOGD(TAG, "Set re-sends exhausted (addr=0x%04x state=%d) — escalating to Get",
                     e->addr, (int)e->state_id);
            (void)blemesh_dispatcher_send_get(e->addr, e->state_id);
            e->kind         = BLEMESH_PENDING_KIND_GET;
            e->deadline_ms  = now + g_blemesh_ctx.cfg.set_confirm_timeout_ms;
        } else {
            ESP_LOGW(TAG, "Set failed (addr=0x%04x state=%d)", e->addr, (int)e->state_id);
            if (g_blemesh_ctx.cb.on_set_failed) {
                g_blemesh_ctx.cb.on_set_failed(e->addr, e->state_id);
            }
            memset(e, 0, sizeof(*e));
        }
    }
    blemesh_pending_rearm();
}
