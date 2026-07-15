/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for the radio-free Composition Data (Page 0) parsers in
 * blemesh_manager. These functions turn a raw Composition Data byte buffer
 * into the manager's per-node layout (logical elements, composite-light
 * channel element offsets) and are pure: no BLE stack, no peer node, fully
 * deterministic. They are the parts most prone to silent byte-offset
 * regressions, so they are exactly what an automated test should pin down.
 *
 * The parsers are declared in the component-private blemesh_internal.h; the
 * prototypes below mirror that header so the test can call them without
 * exposing the private header. Keep them in sync with blemesh_internal.h.
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "esp_ble_mesh_defs.h"
#include "blemesh_manager.h"   /* blemesh_device_profile_t, BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE */

/* ---- Mirrors of blemesh_internal.h (component-private) ------------------- */
uint16_t blemesh_primary_model_for_profile(blemesh_device_profile_t profile);
void blemesh_comp_logical_elem_offsets(const uint8_t *comp_data, size_t comp_len,
                                       blemesh_device_profile_t profile,
                                       uint8_t *offsets, uint8_t *count);
uint8_t blemesh_comp_ctl_temp_offset(const uint8_t *comp_data, size_t comp_len);
void blemesh_comp_hsl_offsets(const uint8_t *comp_data, size_t comp_len,
                              uint8_t *hue_off, uint8_t *sat_off);

/* ---- Composition Data Page 0 builder ------------------------------------ */
/*
 * Page 0 layout:
 *   CID(2) PID(2) VID(2) CRPL(2) Features(2)            <- 10-byte header
 *   per element: Loc(2) NumS(1) NumV(1)
 *                [SIG model id ×2]*NumS
 *                [(company id ×2, model id ×2)]*NumV
 */
typedef struct {
    uint8_t buf[256];
    size_t  len;
} comp_t;

static void comp_init(comp_t *c)
{
    memset(c, 0, sizeof(*c));
    c->len = 10; /* zeroed header is fine for these parsers */
}

static void put16(comp_t *c, uint16_t v)
{
    c->buf[c->len++] = (uint8_t)(v & 0xFF);
    c->buf[c->len++] = (uint8_t)(v >> 8);
}

/* Append one element carrying the given SIG server model ids (no vendor models). */
static void comp_add_elem(comp_t *c, const uint16_t *sig_ids, uint8_t nums)
{
    c->buf[c->len++] = 0x00; /* Loc lo */
    c->buf[c->len++] = 0x00; /* Loc hi */
    c->buf[c->len++] = nums; /* NumS   */
    c->buf[c->len++] = 0x00; /* NumV   */
    for (uint8_t i = 0; i < nums; i++) {
        put16(c, sig_ids[i]);
    }
}

/* ---- Tests --------------------------------------------------------------- */

TEST_CASE("primary_model_for_profile maps each profile", "[blemesh][parse]")
{
    TEST_ASSERT_EQUAL_HEX16(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV,
                            blemesh_primary_model_for_profile(BLEMESH_DEV_ONOFF));
    TEST_ASSERT_EQUAL_HEX16(ESP_BLE_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV,
                            blemesh_primary_model_for_profile(BLEMESH_DEV_DIMMABLE));
    TEST_ASSERT_EQUAL_HEX16(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV,
                            blemesh_primary_model_for_profile(BLEMESH_DEV_COLOR_TEMP));
    TEST_ASSERT_EQUAL_HEX16(ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SRV,
                            blemesh_primary_model_for_profile(BLEMESH_DEV_COLOR_HSL));
    TEST_ASSERT_EQUAL_HEX16(ESP_BLE_MESH_MODEL_ID_SENSOR_SRV,
                            blemesh_primary_model_for_profile(BLEMESH_DEV_TEMPERATURE));
    /* vendor / unknown -> no primary model (single logical element fallback) */
    TEST_ASSERT_EQUAL_HEX16(0, blemesh_primary_model_for_profile(BLEMESH_DEV_VENDOR));
    TEST_ASSERT_EQUAL_HEX16(0, blemesh_primary_model_for_profile(BLEMESH_DEV_UNKNOWN));
}

TEST_CASE("single-element OnOff -> one logical element at offset 0", "[blemesh][parse]")
{
    comp_t c;
    comp_init(&c);
    const uint16_t e0[] = { ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV };
    comp_add_elem(&c, e0, 1);

    uint8_t off[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE] = {0};
    uint8_t count = 0;
    blemesh_comp_logical_elem_offsets(c.buf, c.len, BLEMESH_DEV_ONOFF, off, &count);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(0, off[0]);
}

TEST_CASE("multi-gang OnOff -> one logical element per element", "[blemesh][parse]")
{
    comp_t c;
    comp_init(&c);
    const uint16_t gang[] = { ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV };
    comp_add_elem(&c, gang, 1);   /* offset 0 */
    comp_add_elem(&c, gang, 1);   /* offset 1 */
    comp_add_elem(&c, gang, 1);   /* offset 2 */

    uint8_t off[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE] = {0};
    uint8_t count = 0;
    blemesh_comp_logical_elem_offsets(c.buf, c.len, BLEMESH_DEV_ONOFF, off, &count);

    TEST_ASSERT_EQUAL_UINT8(3, count);
    TEST_ASSERT_EQUAL_UINT8(0, off[0]);
    TEST_ASSERT_EQUAL_UINT8(1, off[1]);
    TEST_ASSERT_EQUAL_UINT8(2, off[2]);
}

TEST_CASE("CTL light collapses Temperature channel into one logical element", "[blemesh][parse]")
{
    comp_t c;
    comp_init(&c);
    /* Primary element: Light CTL Server (+ the OnOff/Lightness it also bears). */
    const uint16_t prim[] = {
        ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV,
        ESP_BLE_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV,
        ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV,
    };
    comp_add_elem(&c, prim, 3);                       /* offset 0 */
    /* Secondary element: Light CTL Temperature Server (a channel, not its own
     * logical element). */
    const uint16_t temp_elem[] = { ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_TEMP_SRV };
    comp_add_elem(&c, temp_elem, 1);                  /* offset 1 */

    uint8_t off[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE] = {0};
    uint8_t count = 0;
    blemesh_comp_logical_elem_offsets(c.buf, c.len, BLEMESH_DEV_COLOR_TEMP, off, &count);

    /* Only the primary element carries Light CTL Server -> one logical element. */
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(0, off[0]);
    /* The Temperature channel element is found at offset 1. */
    TEST_ASSERT_EQUAL_UINT8(1, blemesh_comp_ctl_temp_offset(c.buf, c.len));
}

TEST_CASE("HSL light collapses Hue/Saturation channels and reports their offsets", "[blemesh][parse]")
{
    comp_t c;
    comp_init(&c);
    const uint16_t prim[] = {
        ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV,
        ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SRV,
    };
    comp_add_elem(&c, prim, 2);                       /* offset 0 */
    const uint16_t hue_elem[] = { ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_HUE_SRV };
    comp_add_elem(&c, hue_elem, 1);                   /* offset 1 */
    const uint16_t sat_elem[] = { ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SAT_SRV };
    comp_add_elem(&c, sat_elem, 1);                   /* offset 2 */

    uint8_t off[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE] = {0};
    uint8_t count = 0;
    blemesh_comp_logical_elem_offsets(c.buf, c.len, BLEMESH_DEV_COLOR_HSL, off, &count);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(0, off[0]);

    uint8_t hue = 0xFF, sat = 0xFF;
    blemesh_comp_hsl_offsets(c.buf, c.len, &hue, &sat);
    TEST_ASSERT_EQUAL_UINT8(1, hue);
    TEST_ASSERT_EQUAL_UINT8(2, sat);
}

TEST_CASE("truncated/empty Composition Data falls back to a single logical element", "[blemesh][parse]")
{
    uint8_t off[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE] = {0};
    uint8_t count = 0xFF;

    /* Shorter than the 10-byte header. */
    uint8_t shortbuf[4] = { 0 };
    blemesh_comp_logical_elem_offsets(shortbuf, sizeof(shortbuf), BLEMESH_DEV_ONOFF, off, &count);
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(0, off[0]);

    /* No channel elements present -> absent (0). */
    TEST_ASSERT_EQUAL_UINT8(0, blemesh_comp_ctl_temp_offset(shortbuf, sizeof(shortbuf)));
    uint8_t hue = 0xFF, sat = 0xFF;
    blemesh_comp_hsl_offsets(shortbuf, sizeof(shortbuf), &hue, &sat);
    TEST_ASSERT_EQUAL_UINT8(0, hue);
    TEST_ASSERT_EQUAL_UINT8(0, sat);
}

TEST_CASE("profile with no matching primary model falls back to single logical element", "[blemesh][parse]")
{
    comp_t c;
    comp_init(&c);
    /* A node that only advertises OnOff, but classified (hypothetically) as a
     * profile whose primary model it does not carry -> fallback, never zero. */
    const uint16_t e0[] = { ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV };
    comp_add_elem(&c, e0, 1);

    uint8_t off[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE] = {0};
    uint8_t count = 0;
    blemesh_comp_logical_elem_offsets(c.buf, c.len, BLEMESH_DEV_COLOR_HSL, off, &count);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(0, off[0]);
}

void app_main(void)
{
    unity_run_menu();
}
