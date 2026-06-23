/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_initial_sync.c
 * @brief Paced initial-state synchronization. After a node is provisioned
 *        (fresh) or restored from NVS the manager doesn't yet know what
 *        the node's actual state is — Composition Data tells us its
 *        capabilities, not its current OnOff/Lightness/CTL/HSL/Sensor
 *        values. Without this layer, the consumer keeps showing whatever
 *        default/persisted value it had, and the first user write drives
 *        the leaf to a value it may already be at, causing log noise and
 *        stale UI.
 *
 *        This module pushes a per-profile list of Get requests into a
 *        small FIFO and drains it via a one-shot software timer that
 *        re-arms itself while the FIFO is non-empty. Each tick pops one
 *        entry and asks the dispatcher to send the matching Get; the
 *        ensuing Status arrives through the regular RX path and the
 *        consumer's `on_state_changed` callback.
 *
 *        Single-threaded — the FIFO is touched only from the manager
 *        task plus the timer callback, which posts a tick message back
 *        to the same task.
 */

#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "blemesh_internal.h"

#define TAG "blemesh_sync"

typedef struct {
    blemesh_addr_t     addr;
    blemesh_state_id_t state_id;
} sync_entry_t;

static sync_entry_t s_fifo[CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH];
static size_t       s_head;     /* next slot to pop */
static size_t       s_tail;     /* next slot to push */
static size_t       s_count;
static bool         s_armed;

static void sync_timer_cb(TimerHandle_t t)
{
    (void)t;
    blemesh_msg_t m = { .kind = BLEMESH_MSG_SYNC_TICK };
    (void)blemesh_post(&m);
}

static void ensure_timer(void)
{
    if (g_blemesh_ctx.sync_timer != NULL) {
        return;
    }
    TickType_t period = pdMS_TO_TICKS(CONFIG_BLEMESH_MGR_INITIAL_SYNC_INTERVAL_MS);
    if (period == 0) {
        period = 1;
    }
    g_blemesh_ctx.sync_timer = xTimerCreate("ble_mgr_sync",
                                            period,
                                            pdFALSE, NULL,
                                            sync_timer_cb);
    if (g_blemesh_ctx.sync_timer == NULL) {
        ESP_LOGE(TAG, "sync timer create failed");
    }
}

static void rearm_if_pending(void)
{
    if (s_count == 0) {
        s_armed = false;
        return;
    }
    ensure_timer();
    if (g_blemesh_ctx.sync_timer == NULL) {
        return;
    }
    TickType_t period = pdMS_TO_TICKS(CONFIG_BLEMESH_MGR_INITIAL_SYNC_INTERVAL_MS);
    if (period == 0) {
        period = 1;
    }
    /* xTimerChangePeriod auto-starts a stopped timer. */
    xTimerChangePeriod(g_blemesh_ctx.sync_timer, period, 0);
    s_armed = true;
}

static bool fifo_contains(blemesh_addr_t addr, blemesh_state_id_t state_id)
{
    for (size_t i = 0; i < s_count; i++) {
        size_t idx = (s_head + i) % CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH;
        if (s_fifo[idx].addr == addr && s_fifo[idx].state_id == state_id) {
            return true;
        }
    }
    return false;
}

static bool fifo_push(blemesh_addr_t addr, blemesh_state_id_t state_id)
{
    /* The same (addr, state) is enqueued from several places — fresh
     * provisioning, NVS restore, every heartbeat, and the unreachable→
     * reachable transition. Dedup so a node that publishes heartbeats
     * faster than the FIFO drains can't fill the buffer with duplicate
     * Gets. Once an entry is popped (Get already on the air), the next
     * heartbeat is free to enqueue another round if needed. */
    if (fifo_contains(addr, state_id)) {
        return false;
    }
    if (s_count >= CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH) {
        ESP_LOGW(TAG, "FIFO full, dropping sync (addr=0x%04x state=%d)",
                 addr, (int)state_id);
        return false;
    }
    s_fifo[s_tail].addr     = addr;
    s_fifo[s_tail].state_id = state_id;
    s_tail = (s_tail + 1) % CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH;
    s_count++;
    return true;
}

static bool fifo_pop(sync_entry_t *out)
{
    if (s_count == 0) {
        return false;
    }
    *out = s_fifo[s_head];
    s_head = (s_head + 1) % CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH;
    s_count--;
    return true;
}

void blemesh_initial_sync_init(void)
{
    memset(s_fifo, 0, sizeof(s_fifo));
    s_head  = 0;
    s_tail  = 0;
    s_count = 0;
    s_armed = false;
    if (g_blemesh_ctx.sync_timer != NULL) {
        xTimerStop(g_blemesh_ctx.sync_timer, 0);
    }
}

void blemesh_initial_sync_enqueue(blemesh_addr_t addr,
                                  blemesh_device_profile_t profile)
{
    bool pushed = false;

    /* Most light profiles also implement Generic OnOff Server, so always
     * Get OnOff first — it's cheap and gives the manager an unambiguous
     * power-state baseline before deriving Level / Color from the
     * profile-specific Status. Sensor-only nodes skip OnOff. */
    switch (profile) {
    case BLEMESH_DEV_ONOFF:
        pushed |= fifo_push(addr, BLEMESH_STATE_ONOFF);
        break;
    case BLEMESH_DEV_DIMMABLE:
        pushed |= fifo_push(addr, BLEMESH_STATE_ONOFF);
        pushed |= fifo_push(addr, BLEMESH_STATE_LIGHTNESS);
        break;
    case BLEMESH_DEV_COLOR_TEMP:
        pushed |= fifo_push(addr, BLEMESH_STATE_ONOFF);
        pushed |= fifo_push(addr, BLEMESH_STATE_CTL);
        break;
    case BLEMESH_DEV_COLOR_HSL:
        pushed |= fifo_push(addr, BLEMESH_STATE_ONOFF);
        pushed |= fifo_push(addr, BLEMESH_STATE_HSL);
        break;
    case BLEMESH_DEV_OCCUPANCY:
    case BLEMESH_DEV_TEMPERATURE:
        pushed |= fifo_push(addr, BLEMESH_STATE_SENSOR_VALUE);
        break;
    case BLEMESH_DEV_UNKNOWN:
    case BLEMESH_DEV_VENDOR:
    default:
        /* No SIG state catalogue match — nothing to Get automatically. */
        return;
    }

    if (pushed && !s_armed) {
        rearm_if_pending();
    }
}

void blemesh_initial_sync_on_tick(void)
{
    sync_entry_t e;
    s_armed = false;
    if (!fifo_pop(&e)) {
        return;
    }
    esp_err_t err = blemesh_dispatcher_send_get(e.addr, e.state_id);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Get send failed (addr=0x%04x state=%d): %d",
                 e.addr, (int)e.state_id, err);
    }
    rearm_if_pending();
}

void blemesh_initial_sync_clear_for(blemesh_addr_t addr)
{
    if (s_count == 0) {
        return;
    }
    /* Compact in place: walk the live range [head..tail) modulo capacity
     * and copy survivors into a fresh contiguous head=0 layout. */
    sync_entry_t survivors[CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH];
    size_t kept = 0;
    for (size_t i = 0; i < s_count; i++) {
        size_t idx = (s_head + i) % CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH;
        if (s_fifo[idx].addr != addr) {
            survivors[kept++] = s_fifo[idx];
        }
    }
    if (kept != s_count) {
        memcpy(s_fifo, survivors, kept * sizeof(sync_entry_t));
        s_head  = 0;
        s_tail  = kept;
        s_count = kept;
        if (s_count == 0 && g_blemesh_ctx.sync_timer != NULL) {
            xTimerStop(g_blemesh_ctx.sync_timer, 0);
            s_armed = false;
        }
    }
}

void blemesh_initial_sync_clear_all(void)
{
    s_head  = 0;
    s_tail  = 0;
    s_count = 0;
    s_armed = false;
    if (g_blemesh_ctx.sync_timer != NULL) {
        xTimerStop(g_blemesh_ctx.sync_timer, 0);
    }
}
