/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief PID calculation type
 */
typedef enum {
    PID_CAL_TYPE_INCREMENTAL, /*!< Incremental PID control */
    PID_CAL_TYPE_POSITIONAL,  /*!< Positional PID control */
} pid_calculate_type_t;

/**
 * @brief Type of float PID control block handle
 */
typedef struct pid_ctrl_block_f_t *pid_ctrl_block_handle_f_t;

/**
 * @brief PID control parameters (float)
 */
typedef struct {
    float kp;                      /*!< PID Kp parameter */
    float ki;                      /*!< PID Ki parameter */
    float kd;                      /*!< PID Kd parameter */
    float max_output;              /*!< PID maximum output limitation */
    float min_output;              /*!< PID minimum output limitation */
    float max_integral;            /*!< PID maximum integral value limitation */
    float min_integral;            /*!< PID minimum integral value limitation */
    pid_calculate_type_t cal_type; /*!< PID calculation type */
} pid_ctrl_parameter_f_t;

/**
 * @brief PID control configuration (float)
 */
typedef struct {
    pid_ctrl_parameter_f_t init_param; /*!< Initial parameters */
} pid_ctrl_config_f_t;

esp_err_t pid_new_control_block_f(const pid_ctrl_config_f_t *config, pid_ctrl_block_handle_f_t *ret_pid);
esp_err_t pid_del_control_block_f(pid_ctrl_block_handle_f_t pid);
esp_err_t pid_update_parameters_f(pid_ctrl_block_handle_f_t pid, const pid_ctrl_parameter_f_t *params);
esp_err_t pid_compute_f(pid_ctrl_block_handle_f_t pid, float input_error, float *ret_result);
esp_err_t pid_reset_ctrl_block_f(pid_ctrl_block_handle_f_t pid);

/*
 * One list drives declarations, C++ overloads, C _Generic arms and the
 * backend instances in pid_ctrl_iq.c. IQmath provides Q1..Q30.
 */
#define PID_IQ_FORMATS(_E) \
    _E(1)                  \
    _E(2)                  \
    _E(3)                  \
    _E(4)                  \
    _E(5)                  \
    _E(6)                  \
    _E(7)                  \
    _E(8)                  \
    _E(9)                  \
    _E(10)                 \
    _E(11)                 \
    _E(12)                 \
    _E(13)                 \
    _E(14)                 \
    _E(15)                 \
    _E(16)                 \
    _E(17)                 \
    _E(18)                 \
    _E(19)                 \
    _E(20)                 \
    _E(21)                 \
    _E(22)                 \
    _E(23)                 \
    _E(24)                 \
    _E(25)                 \
    _E(26)                 \
    _E(27)                 \
    _E(28)                 \
    _E(29)                 \
    _E(30)

/**
 * @brief Typed configuration, handle and prototypes for one Q-format.
 */
#define PID_IQ_DECLARE(_q)                                                                   \
    typedef struct {                                                                         \
        _iq##_q kp;                    /*!< PID Kp parameter */                              \
        _iq##_q ki;                    /*!< PID Ki parameter */                              \
        _iq##_q kd;                    /*!< PID Kd parameter */                              \
        _iq##_q max_output;            /*!< PID maximum output limitation */                 \
        _iq##_q min_output;            /*!< PID minimum output limitation */                 \
        _iq##_q max_integral;          /*!< PID maximum integral value limitation */         \
        _iq##_q min_integral;          /*!< PID minimum integral value limitation */         \
        pid_calculate_type_t cal_type; /*!< PID calculation type */                          \
    } pid_ctrl_parameter_iq##_q##_t;                                                         \
    typedef struct {                                                                         \
        pid_ctrl_parameter_iq##_q##_t init_param;                                            \
    } pid_ctrl_config_iq##_q##_t;                                                            \
    typedef struct pid_ctrl_block_iq##_q##_t *pid_ctrl_block_handle_iq##_q##_t;              \
    esp_err_t pid_new_control_block_iq##_q(const pid_ctrl_config_iq##_q##_t *config,         \
                                           pid_ctrl_block_handle_iq##_q##_t *ret_pid);       \
    esp_err_t pid_del_control_block_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid);            \
    esp_err_t pid_update_parameters_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid,             \
                                           const pid_ctrl_parameter_iq##_q##_t *params);     \
    esp_err_t pid_compute_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid,                       \
                                 _iq##_q input_error, _iq##_q *ret_result);                  \
    esp_err_t pid_reset_ctrl_block_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid);

PID_IQ_FORMATS(PID_IQ_DECLARE)
#undef PID_IQ_DECLARE

/*
 * Unsuffixed _iq API. Distinct types from pid_ctrl_*_iqN_t so that
 * `_iq` and `_iqN` (N == GLOBAL_IQ) can coexist in one translation unit.
 */
typedef struct {
    _iq kp;                        /*!< PID Kp parameter */
    _iq ki;                        /*!< PID Ki parameter */
    _iq kd;                        /*!< PID Kd parameter */
    _iq max_output;                /*!< PID maximum output limitation */
    _iq min_output;                /*!< PID minimum output limitation */
    _iq max_integral;              /*!< PID maximum integral value limitation */
    _iq min_integral;              /*!< PID minimum integral value limitation */
    pid_calculate_type_t cal_type; /*!< PID calculation type */
} pid_ctrl_parameter_iq_t;

typedef struct {
    pid_ctrl_parameter_iq_t init_param; /*!< Initial parameters */
} pid_ctrl_config_iq_t;
typedef struct pid_ctrl_block_iq_t *pid_ctrl_block_handle_iq_t;

#define PID_IQ_GLOBAL_WRAPPERS() PID_IQ_GLOBAL_WRAPPERS_I(GLOBAL_IQ)
#define PID_IQ_GLOBAL_WRAPPERS_I(_q) PID_IQ_GLOBAL_WRAPPERS_II(_q)
#define PID_IQ_GLOBAL_WRAPPERS_II(_q)                                                        \
    static inline esp_err_t pid_new_control_block_iq(const pid_ctrl_config_iq_t *config,     \
                                                     pid_ctrl_block_handle_iq_t *ret_pid)    \
    {                                                                                        \
        return pid_new_control_block_iq##_q(                                                 \
            (const pid_ctrl_config_iq##_q##_t *)config,                                      \
            (pid_ctrl_block_handle_iq##_q##_t *)ret_pid);                                    \
    }                                                                                        \
    static inline esp_err_t pid_del_control_block_iq(pid_ctrl_block_handle_iq_t pid)         \
    {                                                                                        \
        return pid_del_control_block_iq##_q((pid_ctrl_block_handle_iq##_q##_t)pid);          \
    }                                                                                        \
    static inline esp_err_t pid_update_parameters_iq(pid_ctrl_block_handle_iq_t pid,         \
                                                     const pid_ctrl_parameter_iq_t *params)  \
    {                                                                                        \
        return pid_update_parameters_iq##_q(                                                 \
            (pid_ctrl_block_handle_iq##_q##_t)pid,                                           \
            (const pid_ctrl_parameter_iq##_q##_t *)params);                                  \
    }                                                                                        \
    static inline esp_err_t pid_compute_iq(pid_ctrl_block_handle_iq_t pid, _iq input_error,  \
                                           _iq *ret_result)                                  \
    {                                                                                        \
        return pid_compute_iq##_q((pid_ctrl_block_handle_iq##_q##_t)pid, input_error,        \
                                  ret_result);                                               \
    }                                                                                        \
    static inline esp_err_t pid_reset_ctrl_block_iq(pid_ctrl_block_handle_iq_t pid)          \
    {                                                                                        \
        return pid_reset_ctrl_block_iq##_q((pid_ctrl_block_handle_iq##_q##_t)pid);           \
    }

PID_IQ_GLOBAL_WRAPPERS()
#undef PID_IQ_GLOBAL_WRAPPERS
#undef PID_IQ_GLOBAL_WRAPPERS_I
#undef PID_IQ_GLOBAL_WRAPPERS_II

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

static inline esp_err_t pid_new_control_block(const pid_ctrl_config_f_t *config, pid_ctrl_block_handle_f_t *ret_pid)
{
    return pid_new_control_block_f(config, ret_pid);
}
static inline esp_err_t pid_del_control_block(pid_ctrl_block_handle_f_t pid)
{
    return pid_del_control_block_f(pid);
}
static inline esp_err_t pid_update_parameters(pid_ctrl_block_handle_f_t pid, const pid_ctrl_parameter_f_t *params)
{
    return pid_update_parameters_f(pid, params);
}
static inline esp_err_t pid_compute(pid_ctrl_block_handle_f_t pid, float input_error, float *ret_result)
{
    return pid_compute_f(pid, input_error, ret_result);
}
static inline esp_err_t pid_reset_ctrl_block(pid_ctrl_block_handle_f_t pid)
{
    return pid_reset_ctrl_block_f(pid);
}

static inline esp_err_t pid_new_control_block(const pid_ctrl_config_iq_t *config, pid_ctrl_block_handle_iq_t *ret_pid)
{
    return pid_new_control_block_iq(config, ret_pid);
}
static inline esp_err_t pid_del_control_block(pid_ctrl_block_handle_iq_t pid)
{
    return pid_del_control_block_iq(pid);
}
static inline esp_err_t pid_update_parameters(pid_ctrl_block_handle_iq_t pid, const pid_ctrl_parameter_iq_t *params)
{
    return pid_update_parameters_iq(pid, params);
}
static inline esp_err_t pid_compute(pid_ctrl_block_handle_iq_t pid, _iq input_error, _iq *ret_result)
{
    return pid_compute_iq(pid, input_error, ret_result);
}
static inline esp_err_t pid_reset_ctrl_block(pid_ctrl_block_handle_iq_t pid)
{
    return pid_reset_ctrl_block_iq(pid);
}

#define PID_IQ_OVERLOADS(_q)                                                                    \
    static inline esp_err_t pid_new_control_block(const pid_ctrl_config_iq##_q##_t *config,     \
                                                  pid_ctrl_block_handle_iq##_q##_t *ret_pid)    \
    {                                                                                           \
        return pid_new_control_block_iq##_q(config, ret_pid);                                   \
    }                                                                                           \
    static inline esp_err_t pid_del_control_block(pid_ctrl_block_handle_iq##_q##_t pid)         \
    {                                                                                           \
        return pid_del_control_block_iq##_q(pid);                                               \
    }                                                                                           \
    static inline esp_err_t pid_update_parameters(pid_ctrl_block_handle_iq##_q##_t pid,         \
                                                  const pid_ctrl_parameter_iq##_q##_t *params)  \
    {                                                                                           \
        return pid_update_parameters_iq##_q(pid, params);                                       \
    }                                                                                           \
    static inline esp_err_t pid_compute(pid_ctrl_block_handle_iq##_q##_t pid,                   \
                                        _iq##_q input_error, _iq##_q *ret_result)               \
    {                                                                                           \
        return pid_compute_iq##_q(pid, input_error, ret_result);                                \
    }                                                                                           \
    static inline esp_err_t pid_reset_ctrl_block(pid_ctrl_block_handle_iq##_q##_t pid)          \
    {                                                                                           \
        return pid_reset_ctrl_block_iq##_q(pid);                                                \
    }

PID_IQ_FORMATS(PID_IQ_OVERLOADS)
#undef PID_IQ_OVERLOADS

#else

#define PID_IQ_ASSOC_NEW(_q)                                          \
    const pid_ctrl_config_iq##_q##_t *: pid_new_control_block_iq##_q, \
    pid_ctrl_config_iq##_q##_t *: pid_new_control_block_iq##_q,
#define PID_IQ_ASSOC_DEL(_q) pid_ctrl_block_handle_iq##_q##_t: pid_del_control_block_iq##_q,
#define PID_IQ_ASSOC_UPD(_q) pid_ctrl_block_handle_iq##_q##_t: pid_update_parameters_iq##_q,
#define PID_IQ_ASSOC_CMP(_q) pid_ctrl_block_handle_iq##_q##_t: pid_compute_iq##_q,
#define PID_IQ_ASSOC_RST(_q) pid_ctrl_block_handle_iq##_q##_t: pid_reset_ctrl_block_iq##_q,

#define pid_new_control_block(config, ret_pid)                  \
    _Generic((config),                                          \
        PID_IQ_FORMATS(PID_IQ_ASSOC_NEW)                        \
        const pid_ctrl_config_iq_t *: pid_new_control_block_iq, \
        pid_ctrl_config_iq_t *: pid_new_control_block_iq,       \
        const pid_ctrl_config_f_t *: pid_new_control_block_f,   \
        pid_ctrl_config_f_t *: pid_new_control_block_f)         \
    ((config), (ret_pid))

#define pid_del_control_block(pid)                            \
    _Generic((pid),                                           \
        PID_IQ_FORMATS(PID_IQ_ASSOC_DEL)                      \
        pid_ctrl_block_handle_iq_t: pid_del_control_block_iq, \
        pid_ctrl_block_handle_f_t: pid_del_control_block_f)   \
    ((pid))

#define pid_update_parameters(pid, params)                    \
    _Generic((pid),                                           \
        PID_IQ_FORMATS(PID_IQ_ASSOC_UPD)                      \
        pid_ctrl_block_handle_iq_t: pid_update_parameters_iq, \
        pid_ctrl_block_handle_f_t: pid_update_parameters_f)   \
    ((pid), (params))

#define pid_compute(pid, input_error, ret_result)   \
    _Generic((pid),                                 \
        PID_IQ_FORMATS(PID_IQ_ASSOC_CMP)            \
        pid_ctrl_block_handle_iq_t: pid_compute_iq, \
        pid_ctrl_block_handle_f_t: pid_compute_f)   \
    ((pid), (input_error), (ret_result))

#define pid_reset_ctrl_block(pid)                            \
    _Generic((pid),                                          \
        PID_IQ_FORMATS(PID_IQ_ASSOC_RST)                     \
        pid_ctrl_block_handle_iq_t: pid_reset_ctrl_block_iq, \
        pid_ctrl_block_handle_f_t: pid_reset_ctrl_block_f)   \
    ((pid))

#endif
