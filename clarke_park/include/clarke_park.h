/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "IQmathLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Three-phase (U/V/W) coordinate, float backend
 */
typedef struct {
    float u; /*!< U phase component */
    float v; /*!< V phase component */
    float w; /*!< W phase component */
} clarke_park_uvw_f_t;

/**
 * @brief Stationary two-axis (alpha/beta) coordinate, float backend
 */
typedef struct {
    float alpha; /*!< alpha axis component */
    float beta;  /*!< beta axis component */
} clarke_park_ab_f_t;

/**
 * @brief Rotating two-axis (d/q) coordinate, float backend
 */
typedef struct {
    float d; /*!< direct axis component */
    float q; /*!< quadrature axis component */
} clarke_park_dq_f_t;

/**
 * @brief Clarke transform (U/V/W -> alpha/beta), equal-amplitude convention, float
 *
 *     alpha = (2/3) * (u - (v + w) / 2)
 *     beta  = (v - w) / sqrt(3)
 *
 * @param[in] uvw Three-phase coordinate
 * @param[out] ab Stationary alpha/beta coordinate
 */
void clarke_park_clarke_f(const clarke_park_uvw_f_t *uvw, clarke_park_ab_f_t *ab);

/**
 * @brief Inverse Clarke transform (alpha/beta -> U/V/W), float
 *
 *     u = alpha
 *     v = (sqrt(3) * beta - alpha) / 2
 *     w = -u - v
 *
 * @param[in] ab Stationary alpha/beta coordinate
 * @param[out] uvw Three-phase coordinate
 */
void clarke_park_iclarke_f(const clarke_park_ab_f_t *ab, clarke_park_uvw_f_t *uvw);

/**
 * @brief Park transform (alpha/beta -> d/q), float
 *
 *     d = alpha * cos(theta) + beta * sin(theta)
 *     q = -alpha * sin(theta) + beta * cos(theta)
 *
 * @param[in] theta_rad Electrical angle in radians
 * @param[in] ab Stationary alpha/beta coordinate
 * @param[out] dq Rotating d/q coordinate
 */
void clarke_park_park_f(float theta_rad, const clarke_park_ab_f_t *ab, clarke_park_dq_f_t *dq);

/**
 * @brief Inverse Park transform (d/q -> alpha/beta), float
 *
 *     alpha = d * cos(theta) - q * sin(theta)
 *     beta  = d * sin(theta) + q * cos(theta)
 *
 * @param[in] theta_rad Electrical angle in radians
 * @param[in] dq Rotating d/q coordinate
 * @param[out] ab Stationary alpha/beta coordinate
 */
void clarke_park_ipark_f(float theta_rad, const clarke_park_dq_f_t *dq, clarke_park_ab_f_t *ab);

/*
 * One list drives declarations, C++ overloads, C _Generic arms and the
 * backend instances in clarke_park_iq.c. Q30 is omitted: IQmath has no
 * radian _IQ30sin/_IQ30cos.
 */
#define CLARKE_PARK_IQ_FORMATS(_E) \
    _E(1)                          \
    _E(2)                          \
    _E(3)                          \
    _E(4)                          \
    _E(5)                          \
    _E(6)                          \
    _E(7)                          \
    _E(8)                          \
    _E(9)                          \
    _E(10)                         \
    _E(11)                         \
    _E(12)                         \
    _E(13)                         \
    _E(14)                         \
    _E(15)                         \
    _E(16)                         \
    _E(17)                         \
    _E(18)                         \
    _E(19)                         \
    _E(20)                         \
    _E(21)                         \
    _E(22)                         \
    _E(23)                         \
    _E(24)                         \
    _E(25)                         \
    _E(26)                         \
    _E(27)                         \
    _E(28)                         \
    _E(29)

/**
 * @brief Coordinate types and prototypes for one Q-format.
 *
 * Coordinate structs are a distinct type per format. C++ overloads reject
 * a mix. The C `_Generic` API keys off the input coordinate; a mismatched
 * output is an incompatible-pointer warning, which ESP-IDF treats as an
 * error (`-Werror`). IQmath `_iqN` scalars are all `int32_t`, so Park's
 * `theta_rad` must use the matching `_IQN()` helper.
 */
#define CLARKE_PARK_IQ_DECLARE(_q)                                                             \
    typedef struct {                                                                           \
        _iq##_q u; /*!< U phase component */                                                   \
        _iq##_q v; /*!< V phase component */                                                   \
        _iq##_q w; /*!< W phase component */                                                   \
    } clarke_park_uvw_iq##_q##_t;                                                              \
    typedef struct {                                                                           \
        _iq##_q alpha; /*!< alpha axis component */                                            \
        _iq##_q beta;  /*!< beta axis component */                                             \
    } clarke_park_ab_iq##_q##_t;                                                               \
    typedef struct {                                                                           \
        _iq##_q d; /*!< direct axis component */                                               \
        _iq##_q q; /*!< quadrature axis component */                                           \
    } clarke_park_dq_iq##_q##_t;                                                               \
    void clarke_park_clarke_iq##_q(const clarke_park_uvw_iq##_q##_t *uvw,                      \
                                   clarke_park_ab_iq##_q##_t *ab);                             \
    void clarke_park_iclarke_iq##_q(const clarke_park_ab_iq##_q##_t *ab,                       \
                                    clarke_park_uvw_iq##_q##_t *uvw);                          \
    void clarke_park_park_iq##_q(_iq##_q theta_rad,                                            \
                                 const clarke_park_ab_iq##_q##_t *ab,                          \
                                 clarke_park_dq_iq##_q##_t *dq);                               \
    void clarke_park_ipark_iq##_q(_iq##_q theta_rad,                                           \
                                  const clarke_park_dq_iq##_q##_t *dq,                         \
                                  clarke_park_ab_iq##_q##_t *ab);

CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_DECLARE)
#undef CLARKE_PARK_IQ_DECLARE

/*
 * Unsuffixed _iq types and wrappers follow this TU's GLOBAL_IQ. Must not
 * live in clarke_park_iq.c: GLOBAL_IQ there is the component's (usually 24).
 */
typedef struct {
    _iq u; /*!< U phase component */
    _iq v; /*!< V phase component */
    _iq w; /*!< W phase component */
} clarke_park_uvw_iq_t;

typedef struct {
    _iq alpha; /*!< alpha axis component */
    _iq beta;  /*!< beta axis component */
} clarke_park_ab_iq_t;

typedef struct {
    _iq d; /*!< direct axis component */
    _iq q; /*!< quadrature axis component */
} clarke_park_dq_iq_t;

#define CLARKE_PARK_IQ_GLOBAL_WRAPPERS() CLARKE_PARK_IQ_GLOBAL_WRAPPERS_I(GLOBAL_IQ)
#define CLARKE_PARK_IQ_GLOBAL_WRAPPERS_I(_q) CLARKE_PARK_IQ_GLOBAL_WRAPPERS_II(_q)
#define CLARKE_PARK_IQ_GLOBAL_WRAPPERS_II(_q)                                              \
    static inline void clarke_park_clarke_iq(const clarke_park_uvw_iq_t *uvw,              \
                                             clarke_park_ab_iq_t *ab)                      \
    {                                                                                      \
        clarke_park_uvw_iq##_q##_t uvw_n = { .u = uvw->u, .v = uvw->v, .w = uvw->w };      \
        clarke_park_ab_iq##_q##_t ab_n;                                                    \
        clarke_park_clarke_iq##_q(&uvw_n, &ab_n);                                          \
        ab->alpha = ab_n.alpha;                                                            \
        ab->beta = ab_n.beta;                                                              \
    }                                                                                      \
    static inline void clarke_park_iclarke_iq(const clarke_park_ab_iq_t *ab,               \
                                              clarke_park_uvw_iq_t *uvw)                   \
    {                                                                                      \
        clarke_park_ab_iq##_q##_t ab_n = { .alpha = ab->alpha, .beta = ab->beta };         \
        clarke_park_uvw_iq##_q##_t uvw_n;                                                  \
        clarke_park_iclarke_iq##_q(&ab_n, &uvw_n);                                         \
        uvw->u = uvw_n.u;                                                                  \
        uvw->v = uvw_n.v;                                                                  \
        uvw->w = uvw_n.w;                                                                  \
    }                                                                                      \
    static inline void clarke_park_park_iq(_iq theta_rad, const clarke_park_ab_iq_t *ab,   \
                                           clarke_park_dq_iq_t *dq)                        \
    {                                                                                      \
        clarke_park_ab_iq##_q##_t ab_n = { .alpha = ab->alpha, .beta = ab->beta };         \
        clarke_park_dq_iq##_q##_t dq_n;                                                    \
        clarke_park_park_iq##_q(theta_rad, &ab_n, &dq_n);                                  \
        dq->d = dq_n.d;                                                                    \
        dq->q = dq_n.q;                                                                    \
    }                                                                                      \
    static inline void clarke_park_ipark_iq(_iq theta_rad, const clarke_park_dq_iq_t *dq,  \
                                            clarke_park_ab_iq_t *ab)                       \
    {                                                                                      \
        clarke_park_dq_iq##_q##_t dq_n = { .d = dq->d, .q = dq->q };                       \
        clarke_park_ab_iq##_q##_t ab_n;                                                    \
        clarke_park_ipark_iq##_q(theta_rad, &dq_n, &ab_n);                                 \
        ab->alpha = ab_n.alpha;                                                            \
        ab->beta = ab_n.beta;                                                              \
    }

CLARKE_PARK_IQ_GLOBAL_WRAPPERS()
#undef CLARKE_PARK_IQ_GLOBAL_WRAPPERS
#undef CLARKE_PARK_IQ_GLOBAL_WRAPPERS_I
#undef CLARKE_PARK_IQ_GLOBAL_WRAPPERS_II

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

inline void clarke_park_clarke(const clarke_park_uvw_f_t *uvw, clarke_park_ab_f_t *ab)
{
    clarke_park_clarke_f(uvw, ab);
}
inline void clarke_park_iclarke(const clarke_park_ab_f_t *ab, clarke_park_uvw_f_t *uvw)
{
    clarke_park_iclarke_f(ab, uvw);
}
inline void clarke_park_park(float theta_rad, const clarke_park_ab_f_t *ab, clarke_park_dq_f_t *dq)
{
    clarke_park_park_f(theta_rad, ab, dq);
}
inline void clarke_park_ipark(float theta_rad, const clarke_park_dq_f_t *dq, clarke_park_ab_f_t *ab)
{
    clarke_park_ipark_f(theta_rad, dq, ab);
}

inline void clarke_park_clarke(const clarke_park_uvw_iq_t *uvw, clarke_park_ab_iq_t *ab)
{
    clarke_park_clarke_iq(uvw, ab);
}
inline void clarke_park_iclarke(const clarke_park_ab_iq_t *ab, clarke_park_uvw_iq_t *uvw)
{
    clarke_park_iclarke_iq(ab, uvw);
}
inline void clarke_park_park(_iq theta_rad, const clarke_park_ab_iq_t *ab, clarke_park_dq_iq_t *dq)
{
    clarke_park_park_iq(theta_rad, ab, dq);
}
inline void clarke_park_ipark(_iq theta_rad, const clarke_park_dq_iq_t *dq, clarke_park_ab_iq_t *ab)
{
    clarke_park_ipark_iq(theta_rad, dq, ab);
}

#define CLARKE_PARK_IQ_OVERLOADS(_q)                                                      \
    inline void clarke_park_clarke(const clarke_park_uvw_iq##_q##_t *uvw,                 \
                                   clarke_park_ab_iq##_q##_t *ab)                         \
    {                                                                                     \
        clarke_park_clarke_iq##_q(uvw, ab);                                               \
    }                                                                                     \
    inline void clarke_park_iclarke(const clarke_park_ab_iq##_q##_t *ab,                  \
                                    clarke_park_uvw_iq##_q##_t *uvw)                      \
    {                                                                                     \
        clarke_park_iclarke_iq##_q(ab, uvw);                                              \
    }                                                                                     \
    inline void clarke_park_park(_iq##_q theta_rad, const clarke_park_ab_iq##_q##_t *ab,  \
                                 clarke_park_dq_iq##_q##_t *dq)                           \
    {                                                                                     \
        clarke_park_park_iq##_q(theta_rad, ab, dq);                                       \
    }                                                                                     \
    inline void clarke_park_ipark(_iq##_q theta_rad, const clarke_park_dq_iq##_q##_t *dq, \
                                  clarke_park_ab_iq##_q##_t *ab)                          \
    {                                                                                     \
        clarke_park_ipark_iq##_q(theta_rad, dq, ab);                                      \
    }

CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_OVERLOADS)
#undef CLARKE_PARK_IQ_OVERLOADS

#else

#define CLARKE_PARK_IQ_ASSOC_UVW(_q)                                 \
    const clarke_park_uvw_iq##_q##_t *: clarke_park_clarke_iq##_q, \
    clarke_park_uvw_iq##_q##_t *: clarke_park_clarke_iq##_q,
#define CLARKE_PARK_IQ_ASSOC_AB_ICLARKE(_q)                            \
    const clarke_park_ab_iq##_q##_t *: clarke_park_iclarke_iq##_q,   \
    clarke_park_ab_iq##_q##_t *: clarke_park_iclarke_iq##_q,
#define CLARKE_PARK_IQ_ASSOC_AB_PARK(_q)                             \
    const clarke_park_ab_iq##_q##_t *: clarke_park_park_iq##_q,    \
    clarke_park_ab_iq##_q##_t *: clarke_park_park_iq##_q,
#define CLARKE_PARK_IQ_ASSOC_DQ(_q)                                  \
    const clarke_park_dq_iq##_q##_t *: clarke_park_ipark_iq##_q,   \
    clarke_park_dq_iq##_q##_t *: clarke_park_ipark_iq##_q,

#define clarke_park_clarke(uvw, ab)                                  \
    _Generic((uvw),                                                  \
        CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_ASSOC_UVW)             \
        const clarke_park_uvw_iq_t *: clarke_park_clarke_iq,         \
        clarke_park_uvw_iq_t *: clarke_park_clarke_iq,               \
        const clarke_park_uvw_f_t *: clarke_park_clarke_f,           \
        clarke_park_uvw_f_t *: clarke_park_clarke_f)((uvw), (ab))

#define clarke_park_iclarke(ab, uvw)                                 \
    _Generic((ab),                                                   \
        CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_ASSOC_AB_ICLARKE)      \
        const clarke_park_ab_iq_t *: clarke_park_iclarke_iq,         \
        clarke_park_ab_iq_t *: clarke_park_iclarke_iq,               \
        const clarke_park_ab_f_t *: clarke_park_iclarke_f,           \
        clarke_park_ab_f_t *: clarke_park_iclarke_f)((ab), (uvw))

#define clarke_park_park(theta_rad, ab, dq)                          \
    _Generic((ab),                                                   \
        CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_ASSOC_AB_PARK)         \
        const clarke_park_ab_iq_t *: clarke_park_park_iq,            \
        clarke_park_ab_iq_t *: clarke_park_park_iq,                  \
        const clarke_park_ab_f_t *: clarke_park_park_f,              \
        clarke_park_ab_f_t *: clarke_park_park_f)((theta_rad), (ab), (dq))

#define clarke_park_ipark(theta_rad, dq, ab)                         \
    _Generic((dq),                                                   \
        CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_ASSOC_DQ)              \
        const clarke_park_dq_iq_t *: clarke_park_ipark_iq,           \
        clarke_park_dq_iq_t *: clarke_park_ipark_iq,                 \
        const clarke_park_dq_f_t *: clarke_park_ipark_f,             \
        clarke_park_dq_f_t *: clarke_park_ipark_f)((theta_rad), (dq), (ab))

#endif
