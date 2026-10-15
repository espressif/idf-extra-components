/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_cpu.h"
#include "clarke_park.h"

#define BENCH_ITERATIONS   10000

/* Timing sweep only: enough distinct angles that the compiler cannot hoist
 * the call. Correctness of wrapping is checked separately, including θ > 2π. */
#define BENCH_ANGLE_STEPS 940
#define BENCH_ANGLE_STEP  (1.0f / (float)BENCH_ANGLE_STEPS)

/* a wrap bug is thousands of LSB; a few LSB is normal. */
#define CHECK_MAX_LSB     4

#define RULE "------------------------------------------------------------------------"

static const clarke_park_uvw_iq15_t s_uvw = {
    .u = _IQ15(0.8f), .v = _IQ15(-0.3f), .w = _IQ15(-0.5f)
};
static const clarke_park_ab_iq15_t s_ab = {
    .alpha = _IQ15(0.6f), .beta = _IQ15(0.4f)
};
static const clarke_park_dq_iq15_t s_dq = {
    .d = _IQ15(0.5f), .q = _IQ15(0.2f)
};

typedef struct {
    const char *label;
    float rad;
} check_angle_t;

typedef struct {
    uint32_t cycles;
    uint32_t ns;
} bench_stat_t;

typedef struct {
    bench_stat_t clarke;
    bench_stat_t iclarke;
    bench_stat_t park;
    bench_stat_t ipark;
} bench_set_t;

/* ASCII column widths. UTF-8 glyphs break printf padding. */
#define W_LABEL 10
#define W_D      6
#define W_NUM    5
#define W_BNAME  8

static int32_t delta_lsb(_iq15 iq, _iq15 hw, int32_t *worst)
{
    int32_t d = (int32_t)hw - (int32_t)iq;
    int32_t a = d < 0 ? -d : d;
    if (a > *worst) {
        *worst = a;
    }
    return d;
}

static void cell_delta(int32_t d)
{
    if (d == 0) {
        printf("  %*d", W_D, 0);
    } else {
        printf("  %+*d", W_D, (int)d);
    }
}

static void check_cordic(void)
{
    static const check_angle_t angles[] = {
        // *INDENT-OFF*
        { "0",       0.0f },
        { "0.5",     0.5f },
        { "pi/2",    (float)(M_PI / 2.0) },
        { "pi",      (float)M_PI },
        { "3pi/2",   (float)(3.0 * M_PI / 2.0) },
        { "2pi",     (float)(2.0 * M_PI) },
        { "2pi+0.5", (float)(2.0 * M_PI + 0.5) },
        { "3pi",     (float)(3.0 * M_PI) },
        { "4pi",     (float)(4.0 * M_PI) },
        { "-0.5",    -0.5f },
        { "-pi",     (float)(-M_PI) },
        { "-2pi-0.5", (float)(-2.0 * M_PI - 0.5) },
        // *INDENT-ON*
    };
    const size_t n = sizeof(angles) / sizeof(angles[0]);
    clarke_park_dq_iq15_t park_iq[n];
    clarke_park_ab_iq15_t ipark_iq[n];
    int32_t worst = 0;

    printf(RULE "\n");
    printf(" CORDIC correctness check   Fail if |d| > %d LSB\n", CHECK_MAX_LSB);
    printf(RULE "\n");
    printf(" Park / iPark   d LSB  (CORDIC - IQmath)\n");
    printf("  %*s  %*s  %*s  %*s  %*s\n",
           W_LABEL, "theta", W_D, "d", W_D, "q", W_D, "alpha", W_D, "beta");

    ESP_ERROR_CHECK(clarke_park_disable_cordic());
    for (size_t i = 0; i < n; i++) {
        const _iq15 theta = _IQ15(angles[i].rad);
        clarke_park_park_iq15(theta, &s_ab, &park_iq[i]);
        clarke_park_ipark_iq15(theta, &s_dq, &ipark_iq[i]);
    }

    ESP_ERROR_CHECK(clarke_park_enable_cordic());
    for (size_t i = 0; i < n; i++) {
        const _iq15 theta = _IQ15(angles[i].rad);
        clarke_park_dq_iq15_t park_hw;
        clarke_park_ab_iq15_t ipark_hw;
        int32_t dd, dq, da, db;

        clarke_park_park_iq15(theta, &s_ab, &park_hw);
        clarke_park_ipark_iq15(theta, &s_dq, &ipark_hw);
        dd = delta_lsb(park_iq[i].d, park_hw.d, &worst);
        dq = delta_lsb(park_iq[i].q, park_hw.q, &worst);
        da = delta_lsb(ipark_iq[i].alpha, ipark_hw.alpha, &worst);
        db = delta_lsb(ipark_iq[i].beta, ipark_hw.beta, &worst);

        printf("  %*s", W_LABEL, angles[i].label);
        cell_delta(dd);
        cell_delta(dq);
        cell_delta(da);
        cell_delta(db);
        printf("\n");
    }

    printf("\n  max |d|  %" PRId32 " LSB\n", worst);
    printf("  result   %s\n", worst <= CHECK_MAX_LSB ? "PASS" : "FAIL");
    ESP_ERROR_CHECK(worst <= CHECK_MAX_LSB ? ESP_OK : ESP_FAIL);
}

static void bench_measure(void (*fn)(void), bench_stat_t *stat)
{
    /* First run is warmup: cache misses a steady loop would not pay every call. */
    fn();
    esp_cpu_cycle_count_t start = esp_cpu_get_cycle_count();
    fn();
    uint32_t total = (uint32_t)(esp_cpu_get_cycle_count() - start);
    const uint32_t mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    stat->cycles = (total + BENCH_ITERATIONS / 2) / BENCH_ITERATIONS;
    stat->ns = (uint32_t)(((uint64_t)stat->cycles * 1000 + mhz / 2) / mhz);
}

static void bench_float_clarke(void)
{
    clarke_park_uvw_f_t uvw = { .u = 0.8f, .v = -0.3f, .w = -0.5f };
    clarke_park_ab_f_t ab;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        uvw.u += 1e-6f;
        clarke_park_clarke(&uvw, &ab);
    }
}

static void bench_float_iclarke(void)
{
    clarke_park_uvw_f_t uvw;
    clarke_park_ab_f_t ab = { .alpha = 0.6f, .beta = 0.4f };

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        ab.alpha += 1e-6f;
        clarke_park_iclarke(&ab, &uvw);
    }
}

static void bench_float_park(void)
{
    clarke_park_ab_f_t ab = { .alpha = 0.6f, .beta = 0.4f };
    clarke_park_dq_f_t dq;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_park((float)(i % BENCH_ANGLE_STEPS) * BENCH_ANGLE_STEP, &ab, &dq);
    }
}

static void bench_float_ipark(void)
{
    clarke_park_dq_f_t dq = { .d = 0.5f, .q = 0.2f };
    clarke_park_ab_f_t ab;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_ipark((float)(i % BENCH_ANGLE_STEPS) * BENCH_ANGLE_STEP, &dq, &ab);
    }
}

static void bench_iq15_clarke(void)
{
    clarke_park_uvw_iq15_t uvw = s_uvw;
    clarke_park_ab_iq15_t ab;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        uvw.u += 1;
        clarke_park_clarke_iq15(&uvw, &ab);
    }
}

static void bench_iq15_iclarke(void)
{
    clarke_park_uvw_iq15_t uvw;
    clarke_park_ab_iq15_t ab = s_ab;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        ab.alpha += 1;
        clarke_park_iclarke_iq15(&ab, &uvw);
    }
}

static void bench_iq15_park(void)
{
    clarke_park_ab_iq15_t ab = s_ab;
    clarke_park_dq_iq15_t dq;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_park_iq15(_IQ15((float)(i % BENCH_ANGLE_STEPS) * BENCH_ANGLE_STEP), &ab, &dq);
    }
}

static void bench_iq15_ipark(void)
{
    clarke_park_dq_iq15_t dq = s_dq;
    clarke_park_ab_iq15_t ab;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_ipark_iq15(_IQ15((float)(i % BENCH_ANGLE_STEPS) * BENCH_ANGLE_STEP), &dq, &ab);
    }
}

static void bench_collect(bench_set_t *set,
                          void (*clarke)(void), void (*iclarke)(void),
                          void (*park)(void), void (*ipark)(void))
{
    bench_measure(clarke, &set->clarke);
    bench_measure(iclarke, &set->iclarke);
    bench_measure(park, &set->park);
    bench_measure(ipark, &set->ipark);
}

static void print_stat(const bench_stat_t *s)
{
    printf("  %*" PRIu32 "  %*" PRIu32, W_NUM, s->cycles, W_NUM, s->ns);
}

static void print_cmp_row(const char *name, const bench_stat_t *f,
                          const bench_stat_t *iq, const bench_stat_t *hw)
{
    printf("  %-*s", W_BNAME, name);
    print_stat(f);
    print_stat(iq);
    print_stat(hw);
    printf("\n");
}

static void group_title(const char *s)
{
    const int width = (2 + W_NUM) * 2;
    int len = (int)strlen(s);
    int left = (width - len) / 2;
    int right = width - len - left;

    if (left < 0) {
        printf("%s", s);
        return;
    }
    printf("%*s%s%*s", left, "", s, right, "");
}

static void bench_print_table(const bench_set_t *flt, const bench_set_t *iq, const bench_set_t *hw)
{
    printf(RULE "\n");
    printf(" Performance %s @%d MHz GCC -O2\n",
           CONFIG_IDF_TARGET, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf(RULE "\n");

    printf("  %-*s", W_BNAME, "");
    group_title("float");
    group_title("IQmath");
    group_title("CORDIC");
    printf("\n");

    printf("  %-*s", W_BNAME, "");
    for (int i = 0; i < 3; i++) {
        printf("  %*s  %*s", W_NUM, "cyc", W_NUM, "ns");
    }
    printf("\n");

    print_cmp_row("clarke", &flt->clarke, &iq->clarke, &hw->clarke);
    print_cmp_row("iclarke", &flt->iclarke, &iq->iclarke, &hw->iclarke);
    print_cmp_row("park", &flt->park, &iq->park, &hw->park);
    print_cmp_row("ipark", &flt->ipark, &iq->ipark, &hw->ipark);
}

void app_main(void)
{
    bench_set_t flt = { 0 };
    bench_set_t iq = { 0 };
    bench_set_t hw = { 0 };

    check_cordic();

    bench_collect(&flt, bench_float_clarke, bench_float_iclarke,
                  bench_float_park, bench_float_ipark);

    ESP_ERROR_CHECK(clarke_park_disable_cordic());
    bench_collect(&iq, bench_iq15_clarke, bench_iq15_iclarke,
                  bench_iq15_park, bench_iq15_ipark);

    ESP_ERROR_CHECK(clarke_park_enable_cordic());
    bench_collect(&hw, bench_iq15_clarke, bench_iq15_iclarke,
                  bench_iq15_park, bench_iq15_ipark);

    printf("\n");
    bench_print_table(&flt, &iq, &hw);
    printf(RULE "\n");
    printf(" clarke_park benchmark finished\n");
}
