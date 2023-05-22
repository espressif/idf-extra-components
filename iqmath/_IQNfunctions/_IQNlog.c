/*!****************************************************************************
 *  @file       _IQNlog.c
 *  @brief      Functions to compute the base-e logarithm of an IQN number.
 *
 *  <hr>
 ******************************************************************************/


#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"

/**
 * @brief Computes the base-e logarithm of an IQN input.
 *
 * @param iqNInput          IQN type input.
 * @param iqNMin            Minimum parameter value.
 * @param q_value           IQ format.
 *
 * @return                  IQN type result of exponential.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNlog)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNlog(int_fast32_t iqNInput, const int_fast32_t iqNMin, const int8_t q_value)
{
    uint8_t ui8Counter;
    int_fast16_t i16Exp;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    int_fast32_t iqNResult;
    int_fast32_t iq30Result;
    uint_fast32_t uiq31Input;
    const uint_fast32_t *piq30Coeffs;

    /*
     * Check the sign of the input and for negative saturation for q_values
     * larger than iq26.
     */
    if (q_value > 26) {
        if (iqNInput <= 0) {
            return 0;
        } else if (iqNInput <= iqNMin) {
            return INT32_MIN;
        }
    }
    /*
     * Only check the sign of the input and that it is not equal to zero for
     * q_values less than or equal to iq26.
     */
    else {
        if (iqNInput <= 0) {
            return 0;
        }
    }

    /* Initialize the exponent value. */
    i16Exp = (31 - q_value);

    /*
     * Scale the input so it is within the following range in iq31:
     *
     *     0.666666 < uiq31Input < 1.333333.
     */
    uiq31Input = (uint_fast32_t)iqNInput;
    while (uiq31Input < iq31_twoThird) {
        uiq31Input <<= 1;
        i16Exp--;
    }

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpyf_start(&ui16IntState, &ui16MPYState);

    /*
     * Initialize the coefficient pointer to the Taylor Series iq30 coefficients
     * for the logarithm functions. Set the iq30 result to the first
     * coefficient in the table. Subtract one from the iq31 input.
     */
    piq30Coeffs = _IQ30log_coeffs;
    iq30Result = *piq30Coeffs++;
    uiq31Input -= iq31_one;

    /* Calculate log(uiq31Input) using the iq30 Taylor Series coefficients. */
    for (ui8Counter = _IQ30log_order; ui8Counter > 0; ui8Counter--) {
        iq30Result = __mpyf_l(uiq31Input, iq30Result);
        iq30Result += *piq30Coeffs++;
    }

    /* Scale the iq30 result to match the function iq type. */
    iqNResult = iq30Result >> (30 - q_value);

    /*
     * Add i16Exp * ln(2) to the iqN result. This will never saturate since we
     * check for the minimum value at the start of the function. Negative
     * exponents require seperate handling to allow for an extra bit with the
     * unsigned data type.
     */
    if (i16Exp > 0) {
        iqNResult += __mpyf_ul(iq31_ln2, ((int_fast32_t)i16Exp << q_value));
    } else {
        iqNResult -= __mpyf_ul(iq31_ln2, (((uint_fast32_t) - i16Exp) << q_value));
    }

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    return iqNResult;
}

/**
 * @brief Computes the base-e logarithm of an IQ30 input.
 *
 * @param a                 IQ30 type input.
 *
 * @return                  IQ30 type result of exponential.
 */
int32_t _IQ30log(int32_t a)
{
    return __IQNlog(a, _IQNlog_min[30 - 27], 30);
}
/**
 * @brief Computes the base-e logarithm of an IQ29 input.
 *
 * @param a                 IQ29 type input.
 *
 * @return                  IQ29 type result of exponential.
 */
int32_t _IQ29log(int32_t a)
{
    return __IQNlog(a, _IQNlog_min[29 - 27], 29);
}
/**
 * @brief Computes the base-e logarithm of an IQ28 input.
 *
 * @param a                 IQ28 type input.
 *
 * @return                  IQ28 type result of exponential.
 */
int32_t _IQ28log(int32_t a)
{
    return __IQNlog(a, _IQNlog_min[28 - 27], 28);
}
/**
 * @brief Computes the base-e logarithm of an IQ27 input.
 *
 * @param a                 IQ27 type input.
 *
 * @return                  IQ27 type result of exponential.
 */
int32_t _IQ27log(int32_t a)
{
    return __IQNlog(a, _IQNlog_min[27 - 27], 27);
}
/**
 * @brief Computes the base-e logarithm of an IQ26 input.
 *
 * @param a                 IQ26 type input.
 *
 * @return                  IQ26 type result of exponential.
 */
int32_t _IQ26log(int32_t a)
{
    return __IQNlog(a, 1, 26);
}
/**
 * @brief Computes the base-e logarithm of an IQ25 input.
 *
 * @param a                 IQ25 type input.
 *
 * @return                  IQ25 type result of exponential.
 */
int32_t _IQ25log(int32_t a)
{
    return __IQNlog(a, 1, 25);
}
/**
 * @brief Computes the base-e logarithm of an IQ24 input.
 *
 * @param a                 IQ24 type input.
 *
 * @return                  IQ24 type result of exponential.
 */
int32_t _IQ24log(int32_t a)
{
    return __IQNlog(a, 1, 24);
}
/**
 * @brief Computes the base-e logarithm of an IQ23 input.
 *
 * @param a                 IQ23 type input.
 *
 * @return                  IQ23 type result of exponential.
 */
int32_t _IQ23log(int32_t a)
{
    return __IQNlog(a, 1, 23);
}
/**
 * @brief Computes the base-e logarithm of an IQ22 input.
 *
 * @param a                 IQ22 type input.
 *
 * @return                  IQ22 type result of exponential.
 */
int32_t _IQ22log(int32_t a)
{
    return __IQNlog(a, 1, 22);
}
/**
 * @brief Computes the base-e logarithm of an IQ21 input.
 *
 * @param a                 IQ21 type input.
 *
 * @return                  IQ21 type result of exponential.
 */
int32_t _IQ21log(int32_t a)
{
    return __IQNlog(a, 1, 21);
}
/**
 * @brief Computes the base-e logarithm of an IQ20 input.
 *
 * @param a                 IQ20 type input.
 *
 * @return                  IQ20 type result of exponential.
 */
int32_t _IQ20log(int32_t a)
{
    return __IQNlog(a, 1, 20);
}
/**
 * @brief Computes the base-e logarithm of an IQ19 input.
 *
 * @param a                 IQ19 type input.
 *
 * @return                  IQ19 type result of exponential.
 */
int32_t _IQ19log(int32_t a)
{
    return __IQNlog(a, 1, 19);
}
/**
 * @brief Computes the base-e logarithm of an IQ18 input.
 *
 * @param a                 IQ18 type input.
 *
 * @return                  IQ18 type result of exponential.
 */
int32_t _IQ18log(int32_t a)
{
    return __IQNlog(a, 1, 18);
}
/**
 * @brief Computes the base-e logarithm of an IQ17 input.
 *
 * @param a                 IQ17 type input.
 *
 * @return                  IQ17 type result of exponential.
 */
int32_t _IQ17log(int32_t a)
{
    return __IQNlog(a, 1, 17);
}
/**
 * @brief Computes the base-e logarithm of an IQ16 input.
 *
 * @param a                 IQ16 type input.
 *
 * @return                  IQ16 type result of exponential.
 */
int32_t _IQ16log(int32_t a)
{
    return __IQNlog(a, 1, 16);
}
/**
 * @brief Computes the base-e logarithm of an IQ15 input.
 *
 * @param a                 IQ15 type input.
 *
 * @return                  IQ15 type result of exponential.
 */
int32_t _IQ15log(int32_t a)
{
    return __IQNlog(a, 1, 15);
}
/**
 * @brief Computes the base-e logarithm of an IQ14 input.
 *
 * @param a                 IQ14 type input.
 *
 * @return                  IQ14 type result of exponential.
 */
int32_t _IQ14log(int32_t a)
{
    return __IQNlog(a, 1, 14);
}
/**
 * @brief Computes the base-e logarithm of an IQ13 input.
 *
 * @param a                 IQ13 type input.
 *
 * @return                  IQ13 type result of exponential.
 */
int32_t _IQ13log(int32_t a)
{
    return __IQNlog(a, 1, 13);
}
/**
 * @brief Computes the base-e logarithm of an IQ12 input.
 *
 * @param a                 IQ12 type input.
 *
 * @return                  IQ12 type result of exponential.
 */
int32_t _IQ12log(int32_t a)
{
    return __IQNlog(a, 1, 12);
}
/**
 * @brief Computes the base-e logarithm of an IQ11 input.
 *
 * @param a                 IQ11 type input.
 *
 * @return                  IQ11 type result of exponential.
 */
int32_t _IQ11log(int32_t a)
{
    return __IQNlog(a, 1, 11);
}
/**
 * @brief Computes the base-e logarithm of an IQ10 input.
 *
 * @param a                 IQ10 type input.
 *
 * @return                  IQ10 type result of exponential.
 */
int32_t _IQ10log(int32_t a)
{
    return __IQNlog(a, 1, 10);
}
/**
 * @brief Computes the base-e logarithm of an IQ9 input.
 *
 * @param a                 IQ9 type input.
 *
 * @return                  IQ9 type result of exponential.
 */
int32_t _IQ9log(int32_t a)
{
    return __IQNlog(a, 1, 9);
}
/**
 * @brief Computes the base-e logarithm of an IQ8 input.
 *
 * @param a                 IQ8 type input.
 *
 * @return                  IQ8 type result of exponential.
 */
int32_t _IQ8log(int32_t a)
{
    return __IQNlog(a, 1, 8);
}
/**
 * @brief Computes the base-e logarithm of an IQ7 input.
 *
 * @param a                 IQ7 type input.
 *
 * @return                  IQ7 type result of exponential.
 */
int32_t _IQ7log(int32_t a)
{
    return __IQNlog(a, 1, 7);
}
/**
 * @brief Computes the base-e logarithm of an IQ6 input.
 *
 * @param a                 IQ6 type input.
 *
 * @return                  IQ6 type result of exponential.
 */
int32_t _IQ6log(int32_t a)
{
    return __IQNlog(a, 1, 6);
}
/**
 * @brief Computes the base-e logarithm of an IQ5 input.
 *
 * @param a                 IQ5 type input.
 *
 * @return                  IQ5 type result of exponential.
 */
int32_t _IQ5log(int32_t a)
{
    return __IQNlog(a, 1, 5);
}
/**
 * @brief Computes the base-e logarithm of an IQ4 input.
 *
 * @param a                 IQ4 type input.
 *
 * @return                  IQ4 type result of exponential.
 */
int32_t _IQ4log(int32_t a)
{
    return __IQNlog(a, 1, 4);
}
/**
 * @brief Computes the base-e logarithm of an IQ3 input.
 *
 * @param a                 IQ3 type input.
 *
 * @return                  IQ3 type result of exponential.
 */
int32_t _IQ3log(int32_t a)
{
    return __IQNlog(a, 1, 3);
}
/**
 * @brief Computes the base-e logarithm of an IQ2 input.
 *
 * @param a                 IQ2 type input.
 *
 * @return                  IQ2 type result of exponential.
 */
int32_t _IQ2log(int32_t a)
{
    return __IQNlog(a, 1, 2);
}
/**
 * @brief Computes the base-e logarithm of an IQ1 input.
 *
 * @param a                 IQ1 type input.
 *
 * @return                  IQ1 type result of exponential.
 */
int32_t _IQ1log(int32_t a)
{
    return __IQNlog(a, 1, 1);
}
