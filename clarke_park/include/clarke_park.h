/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

#ifndef GLOBAL_IQ
#define GLOBAL_IQ CONFIG_CLARKE_PARK_IQ_FORMAT
#elif GLOBAL_IQ != CONFIG_CLARKE_PARK_IQ_FORMAT
#error "GLOBAL_IQ must match CONFIG_CLARKE_PARK_IQ_FORMAT"
#endif

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

/**
 * @brief Three-phase (U/V/W) coordinate, IQmath backend
 */
typedef struct {
    _iq u; /*!< U phase component */
    _iq v; /*!< V phase component */
    _iq w; /*!< W phase component */
} clarke_park_uvw_iq_t;

/**
 * @brief Stationary two-axis (alpha/beta) coordinate, IQmath backend
 */
typedef struct {
    _iq alpha; /*!< alpha axis component */
    _iq beta;  /*!< beta axis component */
} clarke_park_ab_iq_t;

/**
 * @brief Rotating two-axis (d/q) coordinate, IQmath backend
 */
typedef struct {
    _iq d; /*!< direct axis component */
    _iq q; /*!< quadrature axis component */
} clarke_park_dq_iq_t;

/**
 * @brief Clarke transform (U/V/W -> alpha/beta), IQmath backend
 *
 * @param[in] uvw Three-phase coordinate
 * @param[out] ab Stationary alpha/beta coordinate
 */
void clarke_park_clarke_iq(const clarke_park_uvw_iq_t *uvw, clarke_park_ab_iq_t *ab);

/**
 * @brief Inverse Clarke transform (alpha/beta -> U/V/W), IQmath backend
 *
 * @param[in] ab Stationary alpha/beta coordinate
 * @param[out] uvw Three-phase coordinate
 */
void clarke_park_iclarke_iq(const clarke_park_ab_iq_t *ab, clarke_park_uvw_iq_t *uvw);

/**
 * @brief Park transform (alpha/beta -> d/q), IQmath backend
 *
 * @param[in] theta_rad Electrical angle in radians
 * @param[in] ab Stationary alpha/beta coordinate
 * @param[out] dq Rotating d/q coordinate
 */
void clarke_park_park_iq(_iq theta_rad, const clarke_park_ab_iq_t *ab, clarke_park_dq_iq_t *dq);

/**
 * @brief Inverse Park transform (d/q -> alpha/beta), IQmath backend
 *
 * @param[in] theta_rad Electrical angle in radians
 * @param[in] dq Rotating d/q coordinate
 * @param[out] ab Stationary alpha/beta coordinate
 */
void clarke_park_ipark_iq(_iq theta_rad, const clarke_park_dq_iq_t *dq, clarke_park_ab_iq_t *ab);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

/* C++: overloaded inline wrappers dispatch to _f or _iq based on argument type. */
inline void clarke_park_clarke(const clarke_park_uvw_f_t *uvw, clarke_park_ab_f_t *ab)
{
    clarke_park_clarke_f(uvw, ab);
}
inline void clarke_park_clarke(const clarke_park_uvw_iq_t *uvw, clarke_park_ab_iq_t *ab)
{
    clarke_park_clarke_iq(uvw, ab);
}

inline void clarke_park_iclarke(const clarke_park_ab_f_t *ab, clarke_park_uvw_f_t *uvw)
{
    clarke_park_iclarke_f(ab, uvw);
}
inline void clarke_park_iclarke(const clarke_park_ab_iq_t *ab, clarke_park_uvw_iq_t *uvw)
{
    clarke_park_iclarke_iq(ab, uvw);
}

inline void clarke_park_park(float theta_rad, const clarke_park_ab_f_t *ab, clarke_park_dq_f_t *dq)
{
    clarke_park_park_f(theta_rad, ab, dq);
}
inline void clarke_park_park(_iq theta_rad, const clarke_park_ab_iq_t *ab, clarke_park_dq_iq_t *dq)
{
    clarke_park_park_iq(theta_rad, ab, dq);
}

inline void clarke_park_ipark(float theta_rad, const clarke_park_dq_f_t *dq, clarke_park_ab_f_t *ab)
{
    clarke_park_ipark_f(theta_rad, dq, ab);
}
inline void clarke_park_ipark(_iq theta_rad, const clarke_park_dq_iq_t *dq, clarke_park_ab_iq_t *ab)
{
    clarke_park_ipark_iq(theta_rad, dq, ab);
}

#else

#define clarke_park_clarke(uvw, ab)                                       \
    _Generic((uvw),                                                       \
        const clarke_park_uvw_f_t *: clarke_park_clarke_f,                \
        clarke_park_uvw_f_t *: clarke_park_clarke_f,                      \
        const clarke_park_uvw_iq_t *: clarke_park_clarke_iq,              \
        clarke_park_uvw_iq_t *: clarke_park_clarke_iq)((uvw), (ab))

#define clarke_park_iclarke(ab, uvw)                                      \
    _Generic((ab),                                                        \
        const clarke_park_ab_f_t *: clarke_park_iclarke_f,                \
        clarke_park_ab_f_t *: clarke_park_iclarke_f,                      \
        const clarke_park_ab_iq_t *: clarke_park_iclarke_iq,              \
        clarke_park_ab_iq_t *: clarke_park_iclarke_iq)((ab), (uvw))

#define clarke_park_park(theta_rad, ab, dq)                               \
    _Generic((ab),                                                        \
        const clarke_park_ab_f_t *: clarke_park_park_f,                   \
        clarke_park_ab_f_t *: clarke_park_park_f,                         \
        const clarke_park_ab_iq_t *: clarke_park_park_iq,                 \
        clarke_park_ab_iq_t *: clarke_park_park_iq)((theta_rad), (ab), (dq))

#define clarke_park_ipark(theta_rad, dq, ab)                              \
    _Generic((dq),                                                        \
        const clarke_park_dq_f_t *: clarke_park_ipark_f,                  \
        clarke_park_dq_f_t *: clarke_park_ipark_f,                        \
        const clarke_park_dq_iq_t *: clarke_park_ipark_iq,                \
        clarke_park_dq_iq_t *: clarke_park_ipark_iq)((theta_rad), (dq), (ab))

#endif // __cplusplus
