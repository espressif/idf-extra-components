/*!****************************************************************************
 *  @file       _IQNdiv.c
 *  @brief      Functions to divide two values of IQN type.
 *
 *  <hr>
 ******************************************************************************/

#ifndef ti_iq_iqndiv__include
#define ti_iq_iqndiv__include
#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"

/*!
 * @brief Used to specify signed division on IQNdiv
 */
#define TYPE_DEFAULT    (0)
/*!
 * @brief Used to specify unsigned division on IQNdiv
 */
#define TYPE_UNSIGNED   (1)

/**
 * @brief Divide two values of IQN type
 *
 * @param iqNInput1       IQN type value numerator to be divided.
 * @param iqNInput2       IQN type value denominator to divide by.
 * @param type            Specify operation is signed or unsigned.
 * @param q_value         IQ format.
 *
 * @return                IQN type result of the multiplication.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNdiv)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNdiv(int_fast32_t iqNInput1, int_fast32_t iqNInput2, const uint8_t type, const int8_t q_value)
{
    uint8_t ui8Index, ui8Sign = 0;
    uint_fast32_t ui32Temp;
    uint_fast32_t uiq30Guess;
    uint_fast32_t uiqNInput1;
    uint_fast32_t uiqNInput2;
    uint_fast32_t uiqNResult;
    uint_fast64_t uiiqNInput1;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;

    if (type == TYPE_DEFAULT) {
        /* save sign of denominator */
        if (iqNInput2 <= 0) {
            /* check for divide by zero */
            if (iqNInput2 == 0) {
                return INT32_MAX;
            } else {
                ui8Sign = 1;
                iqNInput2 = -iqNInput2;
            }
        }

        /* save sign of numerator */
        if (iqNInput1 < 0) {
            ui8Sign ^= 1;
            iqNInput1 = -iqNInput1;
        }
    } else {
        /* Check for divide by zero */
        if (iqNInput2 == 0) {
            return INT32_MAX;
        }
    }

    /* Save input1 and input2 to unsigned IQN and IIQN (64-bit). */
    uiiqNInput1 = (uint_fast64_t)iqNInput1;
    uiqNInput2 = (uint_fast32_t)iqNInput2;

    /* Scale inputs so that 0.5 <= uiqNInput2 < 1.0. */
    while (uiqNInput2 < 0x40000000) {
        uiqNInput2 <<= 1;
        uiiqNInput1 <<= 1;
    }

    /*
     * Shift input1 back from iq31 to iqN but scale by 2 since we multiply
     * by result in iq30 format.
     */
    if (q_value < 31) {
        uiiqNInput1 >>= (31 - q_value - 1);
    } else {
        uiiqNInput1 <<= 1;
    }

    /* Check for saturation. */
    if (uiiqNInput1 >> 32) {
        if (ui8Sign) {
            return INT32_MIN;
        } else {
            return INT32_MAX;
        }
    } else {
        uiqNInput1 = (uint_fast32_t)uiiqNInput1;
    }

    /* use left most 7 bits as ui8Index into lookup table (range: 32-64) */
    ui8Index = uiqNInput2 >> 24;
    ui8Index -= 64;
    uiq30Guess = (uint_fast32_t)_IQ6div_lookup[ui8Index] << 24;

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpyf_start(&ui16IntState, &ui16MPYState);

    /* 1st iteration */
    ui32Temp = __mpyf_ul(uiq30Guess, uiqNInput2);
    ui32Temp = -((uint_fast32_t)ui32Temp - 0x80000000);
    ui32Temp = ui32Temp << 1;
    uiq30Guess = __mpyf_ul_reuse_arg1(uiq30Guess, ui32Temp);

    /* 2nd iteration */
    ui32Temp = __mpyf_ul(uiq30Guess, uiqNInput2);
    ui32Temp = -((uint_fast32_t)ui32Temp - 0x80000000);
    ui32Temp = ui32Temp << 1;
    uiq30Guess = __mpyf_ul_reuse_arg1(uiq30Guess, ui32Temp);

    /* 3rd iteration */
    ui32Temp = __mpyf_ul(uiq30Guess, uiqNInput2);
    ui32Temp = -((uint_fast32_t)ui32Temp - 0x80000000);
    ui32Temp = ui32Temp << 1;
    uiq30Guess = __mpyf_ul_reuse_arg1(uiq30Guess, ui32Temp);

    /* Multiply 1/uiqNInput2 and uiqNInput1. */
    uiqNResult = __mpyf_ul(uiq30Guess, uiqNInput1);

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* Saturate, add the sign and return. */
    if (type == TYPE_DEFAULT) {
        if (uiqNResult > INT32_MAX) {
            if (ui8Sign) {
                return INT32_MIN;
            } else {
                return INT32_MAX;
            }
        } else {
            if (ui8Sign) {
                return -(int_fast32_t)uiqNResult;
            } else {
                return (int_fast32_t)uiqNResult;
            }
        }
    } else {
        return uiqNResult;
    }
}
// TODO: unless we find a different use for it, or we are intending to keep same params as RTS function, I see no use for TYPE here.
#if ((defined (__IQMATH_USE_MATHACL__)) && (defined (__MSPM0_HAS_MATHACL__)))
/**
 * @brief Divide two values of IQN type, using MathACL
 *
 * @param iqNInput1       IQN type value numerator to be divided.
 * @param iqNInput2       IQN type value denominator to divide by.
 * @param q_value         IQ format.
 *
 * @return                IQN type result of the multiplication.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNdiv_MathACL)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNdiv_MathACL(int_fast32_t iqNInput1, int_fast32_t iqNInput2, const int8_t q_value)
{
    /* write control */
    MATHACL->CTL = 4 | (q_value << 8) | (1 << 5);
    /* write operands to HWA. OP2 = divisor, OP1 = dividend */
    MATHACL->OP2 = iqNInput2;
    /* trigger is write to OP1 */
    MATHACL->OP1 = iqNInput1;
    /* read quotient and remainder */
    return MATHACL->RES1;
}
#endif
#endif