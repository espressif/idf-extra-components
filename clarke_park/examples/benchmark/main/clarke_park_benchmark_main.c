/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * Cycle-count benchmark for the clarke_park component.
 *
 * Every variant of every transform is executed BENCH_ITERATIONS times and the
 * elapsed CPU cycles (esp_cpu_get_cycle_count()) are reported per call, so the
 * numbers can be compared against each other and across builds.
 *
 * The benchmark is intentionally plain: no cache flushing, no interrupt
 * masking, no statistics besides the average. It is a first order comparison
 * tool, not a cycle accurate measurement.
 *
 * What to look at:
 *   - park_iq / ipark_iq versus clarke_iq / iclarke_iq: the Park transforms are
 *     dominated by the sin/cos, the Clarke transforms are plain fixed-point
 *     arithmetic. The difference is the cost of the trigonometry.
 *   - With CONFIG_CLARKE_PARK_USE_CORDIC_HW enabled and Q15, a software reference
 *     (IQmath sin/cos) is printed as well, so the CORDIC hardware path and the
 *     software implementation can be compared within a single run.
 *
 * Note on the fixed-point backend: the Q-format is a compile time property
 * (GLOBAL_IQ / CONFIG_CLARKE_PARK_IQ_FORMAT), so this example only ever measures
 * the format it was built for. sdkconfig.defaults pins Q15, the format the
 * CORDIC hardware supports; build it with CONFIG_CLARKE_PARK_IQ_FORMAT=24 to
 * compare the formats. The banner below always states which configuration the
 * numbers belong to.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_cpu.h"
#include "clarke_park.h"

/* Number of measured transform calls. Each call uses a different input vector
 * and (for the Park transforms) a different angle, so nothing can be hoisted
 * out of the loop and no call can be optimized away. */
#define BENCH_ITERATIONS 10000

/* Frequency of a typical motor control loop (16 kHz PWM). The cost of a
 * transform is reported as a percentage of it, which is what usually matters
 * in practice. */
#define BENCH_LOOP_FREQ_HZ 16000

/* ---------------------------------------------------------------- utilities */

/* Results are written here after a measured loop. The compiler is not allowed
 * to remove the computation it depends on, and the store itself is outside the
 * measurement window. */
static volatile int32_t s_bench_sink;

/**
 * @brief CPU frequency in MHz, 0 if it is unknown for this target
 *
 * The value is the configured frequency (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ). It
 * is only used to convert cycles into a time and into a load percentage, so it
 * does not affect the cycle counts themselves. It is wrong if the application
 * changes the CPU frequency at runtime (dynamic frequency scaling).
 */
static uint32_t bench_cpu_freq_mhz(void)
{
#ifdef CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
    return (uint32_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
#else
    /* e.g. the Linux target has no meaningful CPU frequency, report cycles only */
    return 0;
#endif
}

static void bench_print_config(void)
{
    uint32_t cpu_freq_mhz = bench_cpu_freq_mhz();

    printf("\n");
    printf("=== clarke_park benchmark ===\n");
    printf("Iterations     : %d\n", BENCH_ITERATIONS);
    printf("Optimization   : %s\n",
#if CONFIG_COMPILER_OPTIMIZATION_PERF
           "-O2 (COMPILER_OPTIMIZATION_PERF)"
#elif CONFIG_COMPILER_OPTIMIZATION_SIZE
           "-Os (COMPILER_OPTIMIZATION_SIZE)"
#elif CONFIG_COMPILER_OPTIMIZATION_DEBUG
           "-Og (COMPILER_OPTIMIZATION_DEBUG)"
#elif CONFIG_COMPILER_OPTIMIZATION_NONE
           "-O0 (COMPILER_OPTIMIZATION_NONE)"
#else
           "unknown, set COMPILER_OPTIMIZATION_PERF for comparable numbers"
#endif
          );
    printf("IQ format      : Q%d (CONFIG_CLARKE_PARK_IQ_FORMAT=%d)\n",
           CLARKE_PARK_GLOBAL_IQ, CONFIG_CLARKE_PARK_IQ_FORMAT);
    printf("CORDIC backend : %s\n",
#if defined(CONFIG_CLARKE_PARK_USE_CORDIC_HW) && CONFIG_CLARKE_PARK_USE_CORDIC_HW
#if CLARKE_PARK_GLOBAL_IQ == 15
           "enabled, used for the Q15 Park transforms"
#else
           "compiled in, but not used: the CORDIC hardware only supports Q15"
#endif
#else
           "disabled (IQmath software sin/cos)"
#endif
          );
    printf("Cost per call  : average cycles");
    if (cpu_freq_mhz != 0) {
        printf(", time at %" PRIu32 " MHz", cpu_freq_mhz);
    }
    printf(", load in a %d Hz loop\n", BENCH_LOOP_FREQ_HZ);
    printf("\n");
}

static void bench_report(const char *name, uint32_t cycles)
{
    /* Average cycles per call, rounded to the nearest integer. */
    uint32_t cycles_per_call = (cycles + BENCH_ITERATIONS / 2) / BENCH_ITERATIONS;
    uint32_t cpu_freq_mhz = bench_cpu_freq_mhz();

    printf("%-26s %6" PRIu32 " cycles", name, cycles_per_call);

    if (cpu_freq_mhz != 0) {
        /* Cycles available per period of the control loop. */
        uint32_t cycles_per_loop = cpu_freq_mhz * (1000000 / BENCH_LOOP_FREQ_HZ);
        uint32_t ns = cycles_per_call * 1000 / cpu_freq_mhz;
        uint32_t load_permille = cycles_per_call * 1000 / cycles_per_loop;

        printf("  %6" PRIu32 " ns  %3" PRIu32 ".%01" PRIu32 " %%",
               ns, load_permille / 10, load_permille % 10);
    }
    printf("\n");
}

/* ------------------------------------------------------------ float backend */

static void bench_float(void)
{
    clarke_park_uvw_f_t uvw = { .u = 0.8f, .v = -0.3f, .w = -0.5f };
    clarke_park_ab_f_t ab = { .alpha = 0.6f, .beta = 0.4f };
    clarke_park_dq_f_t dq = { .d = 0.5f, .q = 0.2f };
    uint32_t start;
    uint32_t total;
    int i;

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        uvw.u += 1e-6f;
        clarke_park_clarke_f(&uvw, &ab);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("clarke_f", total);

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        ab.alpha += 1e-6f;
        clarke_park_iclarke_f(&ab, &uvw);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("iclarke_f", total);

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_park_f((float)i * 1e-4f, &ab, &dq);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("park_f", total);

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_ipark_f((float)i * 1e-4f, &dq, &ab);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("ipark_f", total);
}

/* ------------------------------------------------------------- _iq backend */

static void bench_iq(void)
{
    clarke_park_uvw_iq_t uvw = { .u = _IQ(0.8f), .v = _IQ(-0.3f), .w = _IQ(-0.5f) };
    clarke_park_ab_iq_t ab = { .alpha = _IQ(0.6f), .beta = _IQ(0.4f) };
    clarke_park_dq_iq_t dq = { .d = _IQ(0.5f), .q = _IQ(0.2f) };
    uint32_t start;
    uint32_t total;
    int i;

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        uvw.u += 1; /* smallest possible increment of the _iq type */
        clarke_park_clarke_iq(&uvw, &ab);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("clarke_iq", total);

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        ab.alpha += 1;
        clarke_park_iclarke_iq(&ab, &uvw);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("iclarke_iq", total);

    /* The angle sweeps [0, 1) rad, the range every Q-format can represent
     * (Q15 only covers [-1, 1)). */
    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_park_iq(_IQ((float)(i % 1000) * 1e-3f), &ab, &dq);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("park_iq", total);

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_ipark_iq(_IQ((float)(i % 1000) * 1e-3f), &dq, &ab);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("ipark_iq", total);
}

/* ---------------------------------------------------- CORDIC vs. IQmath (Q15)
 *
 * Only measurable when the CORDIC backend is compiled in AND the Q-format is
 * Q15, because that is the only format the hardware supports. The reference is
 * the plain IQmath implementation of the Park transform, i.e. what the
 * component would use without CONFIG_CLARKE_PARK_USE_CORDIC_HW.
 *
 * The hardware path itself is the park_iq line above, so the two can be read
 * side by side.
 */

#if defined(CONFIG_CLARKE_PARK_USE_CORDIC_HW) && CONFIG_CLARKE_PARK_USE_CORDIC_HW && (CLARKE_PARK_GLOBAL_IQ == 15)

static void bench_iq15_sw_park(void)
{
    clarke_park_ab_iq_t ab = { .alpha = _IQ15(0.6f), .beta = _IQ15(0.4f) };
    clarke_park_dq_iq_t dq = { .d = _IQ15(0.5f), .q = _IQ15(0.2f) };
    int32_t acc = 0;
    uint32_t start;
    uint32_t total;
    int i;

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        _iq15 theta = _IQ15((float)(i % 1000) * 1e-3f);
        _iq15 sin_theta = _IQ15sin(theta);
        _iq15 cos_theta = _IQ15cos(theta);

        dq.d = _IQ15mpy(ab.alpha, cos_theta) + _IQ15mpy(ab.beta, sin_theta);
        dq.q = -_IQ15mpy(ab.alpha, sin_theta) + _IQ15mpy(ab.beta, cos_theta);
        acc += dq.d + dq.q;
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("park_iq, IQmath sin/cos", total);

    s_bench_sink = acc;
}

#endif // CORDIC && Q15

/* ----------------------------------------------------- unified (generic) API
 *
 * The dispatch is resolved at compile time, so these numbers must match the
 * numbers of the corresponding backend. They are here to document that the
 * unsuffixed API costs nothing extra.
 */

static void bench_generic(void)
{
    clarke_park_uvw_f_t uvw = { .u = 0.8f, .v = -0.3f, .w = -0.5f };
    clarke_park_ab_f_t ab = { .alpha = 0.6f, .beta = 0.4f };
    clarke_park_dq_f_t dq = { .d = 0.5f, .q = 0.2f };
    uint32_t start;
    uint32_t total;
    int i;

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        uvw.u += 1e-6f;
        clarke_park_clarke(&uvw, &ab);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("clarke (generic, float)", total);

    start = esp_cpu_get_cycle_count();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        clarke_park_park((float)i * 1e-4f, &ab, &dq);
    }
    total = esp_cpu_get_cycle_count() - start;
    bench_report("park (generic, float)", total);
}

/* ----------------------------------------------------------------- app_main */

void app_main(void)
{
    bench_print_config();

    printf("--- float backend ---\n");
    bench_float();

    printf("\n--- IQmath backend (Q%d) ---\n", CLARKE_PARK_GLOBAL_IQ);
    bench_iq();

#if defined(CONFIG_CLARKE_PARK_USE_CORDIC_HW) && CONFIG_CLARKE_PARK_USE_CORDIC_HW && (CLARKE_PARK_GLOBAL_IQ == 15)
    printf("\n--- Q15 reference without CORDIC ---\n");
    bench_iq15_sw_park();
#endif

    printf("\n--- unified API ---\n");
    bench_generic();
    printf("\n");

    printf("clarke_park benchmark finished\n");
}
