/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

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
 * IQmath needs GLOBAL_IQ to select the Q format of the _iq type used by this
 * component. The value comes from CONFIG_CLARKE_PARK_IQ_FORMAT, unless the
 * application has already defined GLOBAL_IQ itself (in that case both must
 * match, otherwise the _iq type would have a different Q format than the one
 * the component was built with).
 *
 * The macro stays defined after this header, like it does for every other
 * IQmath based component (e.g. pid_ctrl). If that collides with the GLOBAL_IQ
 * of another part of your application, define CLARKE_PARK_DONT_DEFINE_GLOBAL_IQ
 * before including this header: the Q format is then applied only to the
 * clarke_park types and the macro is removed again afterwards.
 *
 * CLARKE_PARK_GLOBAL_IQ always holds the Q format used by the _iq types
 * declared below, independently of whether GLOBAL_IQ is left defined.
 */
#ifndef GLOBAL_IQ
#define GLOBAL_IQ CONFIG_CLARKE_PARK_IQ_FORMAT
#define CLARKE_PARK_GLOBAL_IQ_DEFINED_HERE
#elif GLOBAL_IQ != CONFIG_CLARKE_PARK_IQ_FORMAT
#error "GLOBAL_IQ must match CONFIG_CLARKE_PARK_IQ_FORMAT"
#endif
#include "IQmathLib.h"
#ifndef CLARKE_PARK_GLOBAL_IQ
#define CLARKE_PARK_GLOBAL_IQ CONFIG_CLARKE_PARK_IQ_FORMAT
#endif
#if defined(CLARKE_PARK_GLOBAL_IQ_DEFINED_HERE) && defined(CLARKE_PARK_DONT_DEFINE_GLOBAL_IQ)
#undef GLOBAL_IQ
#undef CLARKE_PARK_GLOBAL_IQ_DEFINED_HERE
#endif

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

/*
 * The CORDIC API is only compiled in when CONFIG_CLARKE_PARK_USE_CORDIC_HW is
 * enabled. DOXYGEN is defined while the API reference is generated, so that
 * these declarations show up in the documentation even though the option is
 * off by default.
 */
#if (defined(CONFIG_CLARKE_PARK_USE_CORDIC_HW) && CONFIG_CLARKE_PARK_USE_CORDIC_HW) || defined(DOXYGEN)

#include "driver/cordic.h"

/**
 * @brief Initialize the CORDIC engine used by the IQmath backend
 *
 * The CORDIC peripheral has a single hardware unit, so its engine handle is a
 * process-wide singleton owned by this component. The engine is created lazily
 * on the first Park/Inverse-Park call as well, so calling this function is
 * optional. Call it if you want to detect (and fail early) when the CORDIC
 * unit is already occupied by another component.
 *
 * @return
 *      - ESP_OK: CORDIC engine ready
 *      - ESP_ERR_NOT_FOUND: the CORDIC unit is already occupied by someone else
 *      - ESP_ERR_NO_MEM: out of memory
 */
esp_err_t clarke_park_cordic_init(void);

/**
 * @brief Release the CORDIC engine
 *
 * After this call the CORDIC unit can be used by other parts of the
 * application. The engine is re-created automatically on the next Park
 * transform that needs it.
 *
 * @return
 *      - ESP_OK: CORDIC engine released (or was not initialized)
 *      - other: error from the CORDIC driver
 */
esp_err_t clarke_park_cordic_deinit(void);

/**
 * @brief Get the shared CORDIC engine handle
 *
 * Returns the engine handle owned by this component, so that other parts of
 * the application can share the CORDIC unit for their own calculations.
 *
 * @note The caller must coordinate access with clarke_park transforms, e.g. by
 *       not using the engine concurrently from another task. Do not delete the
 *       returned handle with `cordic_delete_engine()`, use
 *       `clarke_park_cordic_deinit()` instead.
 *
 * @note The engine can be released by `clarke_park_cordic_deinit()` at any
 *       time, so the returned handle is only valid as long as the caller keeps
 *       the CORDIC unit for itself.
 *
 * @return CORDIC engine handle, or NULL if not initialized yet
 */
cordic_engine_handle_t clarke_park_cordic_get_engine(void);

#endif // CONFIG_CLARKE_PARK_USE_CORDIC_HW || DOXYGEN

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

/* C: _Generic based dispatch. Requires C11 (or newer). With an older standard
 * the unsuffixed names are not defined and only the explicitly suffixed
 * _f / _iq functions can be used. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L

#define clarke_park_clarke(uvw, ab)                                        \
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

#endif // C11

#endif // __cplusplus
