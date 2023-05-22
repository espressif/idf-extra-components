/*!****************************************************************************
 *  @file       _IQNexp.c
 *  @brief      Functions to compute the exponential of the input
 *              and return the result.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"

/**
 * @brief Computes the exponential of an IQN input.
 *
 * @param iqNInput          IQN type input.
 * @param iqNLookupTable    Integer result lookup table.
 * @param ui8IntegerOffset  Integer portion offset
 * @param iqN_MIN           Minimum parameter value.
 * @param iqN_MAX           Maximum parameter value.
 * @param q_value           IQ format.
 *
 *
 * @return                  IQN type result of exponential.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNexp)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNexp(int_fast32_t iqNInput, const uint_fast32_t *iqNLookupTable, uint8_t ui8IntegerOffset, const int_fast32_t iqN_MIN, const int_fast32_t iqN_MAX, const int8_t q_value)
{
    uint8_t ui8Count;
    int_fast16_t i16Integer;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    int_fast32_t iq31Fractional;
    uint_fast32_t uiqNResult;
    uint_fast32_t uiqNIntegerResult;
    uint_fast32_t uiq30FractionalResult;
    uint_fast32_t uiq31FractionalResult;
    const uint_fast32_t *piq30Coeffs;

    /* Input is negative. */
    if (iqNInput < 0) {
        /* Check for the minimum value. */
        if (iqNInput < iqN_MIN) {
            return 0;
        }

        /* Extract the fractional portion in iq31 and set sign bit. */
        iq31Fractional = iqNInput;
        iq31Fractional <<= (31 - q_value);
        iq31Fractional |= 0x80000000;

        /* Extract the integer portion. */
        i16Integer = (int_fast16_t)(iqNInput >> q_value) + 1;

        /* Offset the integer portion and lookup the integer result. */
        i16Integer += ui8IntegerOffset;
        uiqNIntegerResult = iqNLookupTable[i16Integer];

        /* Reduce the fractional portion to -ln(2) < iq31Fractional < 0 */
        if (iq31Fractional <= -iq31_ln2) {
            iq31Fractional += iq31_ln2;
            uiqNIntegerResult >>= 1;
        }
    }
    /* Input is positive. */
    else {
        /* Check for the maximum value. */
        if (iqNInput > iqN_MAX) {
            return INT32_MAX;
        }

        /* Extract the fractional portion in iq31 and clear sign bit. */
        iq31Fractional = iqNInput;
        iq31Fractional <<= (31 - q_value);
        iq31Fractional &= 0x7fffffff;

        /* Extract the integer portion. */
        i16Integer = (int_fast16_t)(iqNInput >> q_value);

        /* Offset the integer portion and lookup the integer result. */
        i16Integer += ui8IntegerOffset;
        uiqNIntegerResult = iqNLookupTable[i16Integer];

        /* Reduce the fractional portion to 0 < iq31Fractional < ln(2) */
        if (iq31Fractional >= iq31_ln2) {
            iq31Fractional -= iq31_ln2;
            uiqNIntegerResult <<= 1;
        }
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
     * for the exponential functions. Set the iq30 result to the first
     * coefficient in the table.
     */
    piq30Coeffs = _IQ30exp_coeffs;
    uiq30FractionalResult = *piq30Coeffs++;

    /* Compute exp^(iq31Fractional). */
    for (ui8Count = _IQ30exp_order; ui8Count > 0; ui8Count--) {
        uiq30FractionalResult = __mpyf_l(iq31Fractional, uiq30FractionalResult);
        uiq30FractionalResult += *piq30Coeffs++;
    }

    /* Scale the iq30 fractional result by to iq31. */
    uiq31FractionalResult = uiq30FractionalResult << 1;

    /*
     * Multiply the integer result in iqN format and the fractional result in
     * iq31 format to obtain the result in iqN format.
     */
    uiqNResult = __mpyf_ul(uiqNIntegerResult, uiq31FractionalResult);

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* The result is scaled by 2, round the result and scale to iqN format. */
    uiqNResult++;
    uiqNResult >>= 1;

    return uiqNResult;
}

/**
 * @brief Computes the exponential of an IQ30 input.
 *
 * @param a               IQ30 type input.
 *
 * @return                IQ30 type result of exponential.
 */
int32_t _IQ30exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup30, _IQNexp_offset[30 - 1], _IQNexp_min[30 - 1], _IQNexp_max[30 - 1], 30);
}
/**
 * @brief Computes the exponential of an IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type result of exponential.
 */
int32_t _IQ29exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup29, _IQNexp_offset[29 - 1], _IQNexp_min[29 - 1], _IQNexp_max[29 - 1], 29);
}
/**
 * @brief Computes the exponential of an IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type result of exponential.
 */
int32_t _IQ28exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup28, _IQNexp_offset[28 - 1], _IQNexp_min[28 - 1], _IQNexp_max[28 - 1], 28);
}
/**
 * @brief Computes the exponential of an IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type result of exponential.
 */
int32_t _IQ27exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup27, _IQNexp_offset[27 - 1], _IQNexp_min[27 - 1], _IQNexp_max[27 - 1], 27);
}
/**
 * @brief Computes the exponential of an IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type result of exponential.
 */
int32_t _IQ26exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup26, _IQNexp_offset[26 - 1], _IQNexp_min[26 - 1], _IQNexp_max[26 - 1], 26);
}
/**
 * @brief Computes the exponential of an IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type result of exponential.
 */
int32_t _IQ25exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup25, _IQNexp_offset[25 - 1], _IQNexp_min[25 - 1], _IQNexp_max[25 - 1], 25);
}
/**
 * @brief Computes the exponential of an IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type result of exponential.
 */
int32_t _IQ24exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup24, _IQNexp_offset[24 - 1], _IQNexp_min[24 - 1], _IQNexp_max[24 - 1], 24);
}
/**
 * @brief Computes the exponential of an IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type result of exponential.
 */
int32_t _IQ23exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup23, _IQNexp_offset[23 - 1], _IQNexp_min[23 - 1], _IQNexp_max[23 - 1], 23);
}
/**
 * @brief Computes the exponential of an IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type result of exponential.
 */
int32_t _IQ22exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup22, _IQNexp_offset[22 - 1], _IQNexp_min[22 - 1], _IQNexp_max[22 - 1], 22);
}
/**
 * @brief Computes the exponential of an IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type result of exponential.
 */
int32_t _IQ21exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup21, _IQNexp_offset[21 - 1], _IQNexp_min[21 - 1], _IQNexp_max[21 - 1], 21);
}
/**
 * @brief Computes the exponential of an IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type result of exponential.
 */
int32_t _IQ20exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup20, _IQNexp_offset[20 - 1], _IQNexp_min[20 - 1], _IQNexp_max[20 - 1], 20);
}
/**
 * @brief Computes the exponential of an IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type result of exponential.
 */
int32_t _IQ19exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup19, _IQNexp_offset[19 - 1], _IQNexp_min[19 - 1], _IQNexp_max[19 - 1], 19);
}
/**
 * @brief Computes the exponential of an IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type result of exponential.
 */
int32_t _IQ18exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup18, _IQNexp_offset[18 - 1], _IQNexp_min[18 - 1], _IQNexp_max[18 - 1], 18);
}
/**
 * @brief Computes the exponential of an IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type result of exponential.
 */
int32_t _IQ17exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup17, _IQNexp_offset[17 - 1], _IQNexp_min[17 - 1], _IQNexp_max[17 - 1], 17);
}
/**
 * @brief Computes the exponential of an IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type result of exponential.
 */
int32_t _IQ16exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup16, _IQNexp_offset[16 - 1], _IQNexp_min[16 - 1], _IQNexp_max[16 - 1], 16);
}
/**
 * @brief Computes the exponential of an IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type result of exponential.
 */
int32_t _IQ15exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup15, _IQNexp_offset[15 - 1], _IQNexp_min[15 - 1], _IQNexp_max[15 - 1], 15);
}
/**
 * @brief Computes the exponential of an IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type result of exponential.
 */
int32_t _IQ14exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup14, _IQNexp_offset[14 - 1], _IQNexp_min[14 - 1], _IQNexp_max[14 - 1], 14);
}
/**
 * @brief Computes the exponential of an IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type result of exponential.
 */
int32_t _IQ13exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup13, _IQNexp_offset[13 - 1], _IQNexp_min[13 - 1], _IQNexp_max[13 - 1], 13);
}
/**
 * @brief Computes the exponential of an IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type result of exponential.
 */
int32_t _IQ12exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup12, _IQNexp_offset[12 - 1], _IQNexp_min[12 - 1], _IQNexp_max[12 - 1], 12);
}
/**
 * @brief Computes the exponential of an IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type result of exponential.
 */
int32_t _IQ11exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup11, _IQNexp_offset[11 - 1], _IQNexp_min[11 - 1], _IQNexp_max[11 - 1], 11);
}
/**
 * @brief Computes the exponential of an IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type result of exponential.
 */
int32_t _IQ10exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup10, _IQNexp_offset[10 - 1], _IQNexp_min[10 - 1], _IQNexp_max[10 - 1], 10);
}
/**
 * @brief Computes the exponential of an IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type result of exponential.
 */
int32_t _IQ9exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup9, _IQNexp_offset[9 - 1], _IQNexp_min[9 - 1], _IQNexp_max[9 - 1], 9);
}
/**
 * @brief Computes the exponential of an IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type result of exponential.
 */
int32_t _IQ8exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup8, _IQNexp_offset[8 - 1], _IQNexp_min[8 - 1], _IQNexp_max[8 - 1], 8);
}
/**
 * @brief Computes the exponential of an IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type result of exponential.
 */
int32_t _IQ7exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup7, _IQNexp_offset[7 - 1], _IQNexp_min[7 - 1], _IQNexp_max[7 - 1], 7);
}
/**
 * @brief Computes the exponential of an IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type result of exponential.
 */
int32_t _IQ6exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup6, _IQNexp_offset[6 - 1], _IQNexp_min[6 - 1], _IQNexp_max[6 - 1], 6);
}
/**
 * @brief Computes the exponential of an IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type result of exponential.
 */
int32_t _IQ5exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup5, _IQNexp_offset[5 - 1], _IQNexp_min[5 - 1], _IQNexp_max[5 - 1], 5);
}
/**
 * @brief Computes the exponential of an IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type result of exponential.
 */
int32_t _IQ4exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup4, _IQNexp_offset[4 - 1], _IQNexp_min[4 - 1], _IQNexp_max[4 - 1], 4);
}
/**
 * @brief Computes the exponential of an IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type result of exponential.
 */
int32_t _IQ3exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup3, _IQNexp_offset[3 - 1], _IQNexp_min[3 - 1], _IQNexp_max[3 - 1], 3);
}
/**
 * @brief Computes the exponential of an IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type result of exponential.
 */
int32_t _IQ2exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup2, _IQNexp_offset[2 - 1], _IQNexp_min[2 - 1], _IQNexp_max[2 - 1], 2);
}
/**
 * @brief Computes the exponential of an IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type result of exponential.
 */
int32_t _IQ1exp(int32_t a)
{
    return __IQNexp(a, _IQNexp_lookup1, _IQNexp_offset[1 - 1], _IQNexp_min[1 - 1], _IQNexp_max[1 - 1], 1);
}
