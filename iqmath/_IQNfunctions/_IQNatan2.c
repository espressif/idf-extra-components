/*!****************************************************************************
 *  @file       _IQNatan2.c
 *  @brief      Functions to compute the 4-quadrant arctangent of the input
 *              and return the result.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"
#include "../include/IQmathLib.h"
#include "_IQNmpy.h"
#include "_IQNdiv.h"

/*!
 * @brief The value of PI
 */
#define PI (3.1415926536)

/*!
 * @brief Used to specify per-unit result
 */
#define TYPE_PU         (0)
/*!
 * @brief Used to specify result in radians
 */
#define TYPE_RAD        (1)

#if ((!defined (__IQMATH_USE_MATHACL__)) || (!defined (__MSPM0_HAS_MATHACL__)))
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/* Hidden _UIQ31div function. */
/**
 * @brief Computes the division of the IQ31 inputs.
 *
 * @param uiq31Input1     IQ31 type value numerator to be divided.
 * @param uiq31Input2     IQ31 type value denominator to divide by.
 *
 * @return                IQ31 type result of division.
 */
extern uint_fast32_t _UIQ31div(uint_fast32_t uiq31Input1, uint_fast32_t uiq31Input2);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Compute the 4-quadrant arctangent of the IQN input
 *        and return the result.
 *
 * @param iqNInputY       IQN type input y.
 * @param iqNInputX       IQN type input x.
 * @param type            Specifies radians or per-unit operation.
 * @param q_value         IQ format.
 *
 * @return                IQN type result of 4-quadrant arctangent.
 */
/*
 * Calculate atan2 using a 3rd order Taylor series. The coefficients are stored
 * in a lookup table with 17 ranges to give an accuracy of XX bits.
 *
 * The input to the Taylor series is the ratio of the two inputs and must be
 * in the range of 0 <= input <= 1. If the y argument is larger than the x
 * argument we must apply the following transformation:
 *
 *     atan(y/x) = pi/2 - atan(x/y)
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNatan2)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNatan2(int_fast32_t iqNInputY, int_fast32_t iqNInputX, const uint8_t type, const int8_t q_value)
{
    uint8_t ui8Status = 0;
    uint8_t ui8Index;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    uint_fast32_t uiqNInputX;
    uint_fast32_t uiqNInputY;
    uint_fast32_t uiq32ResultPU;
    int_fast32_t iqNResult;
    int_fast32_t iq29Result;
    const int_fast32_t *piq32Coeffs;
    uint_fast32_t uiq31Input;

    /*
     * Extract the sign from the inputs and set the following status bits:
     *
     *      ui8Status = xxxxxTQS
     *      x = unused
     *      T = transform was applied
     *      Q = 2nd or 3rd quadrant (-x)
     *      S = sign bit needs to be set (-y)
     */
    if (iqNInputY < 0) {
        ui8Status |= 1;
        iqNInputY = -iqNInputY;
    }
    if (iqNInputX < 0) {
        ui8Status |= 2;
        iqNInputX = -iqNInputX;
    }

    /* Save inputs to unsigned iqN formats. */
    uiqNInputX = (uint_fast32_t)iqNInputX;
    uiqNInputY = (uint_fast32_t)iqNInputY;

    /*
     * Calcualte the ratio of the inputs in iq31. When using the iq31 div
     * fucntions with inputs of matching type the result will be iq31:
     *
     *     iq31 = _IQ31div(iqN, iqN);
     */
    if (uiqNInputX < uiqNInputY) {
        ui8Status |= 4;
        uiq31Input = _UIQ31div(uiqNInputX, uiqNInputY);
    } else {
        uiq31Input = _UIQ31div(uiqNInputY, uiqNInputX);
    }

    /* Calculate the index using the left 8 most bits of the input. */
    ui8Index = (uint_fast16_t)(uiq31Input >> 24);
    ui8Index = ui8Index & 0x00fc;

    /* Set the coefficient pointer. */
    piq32Coeffs = &_IQ32atan_coeffs[ui8Index];

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpyf_start(&ui16IntState, &ui16MPYState);

    /*
     * Calculate atan(x) using the following Taylor series:
     *
     *     atan(x) = ((c3*x + c2)*x + c1)*x + c0
     */

    /* c3*x */
    uiq32ResultPU = __mpyf_l(uiq31Input, *piq32Coeffs++);

    /* c3*x + c2 */
    uiq32ResultPU = uiq32ResultPU + *piq32Coeffs++;

    /* (c3*x + c2)*x */
    uiq32ResultPU = __mpyf_l(uiq31Input, uiq32ResultPU);

    /* (c3*x + c2)*x + c1 */
    uiq32ResultPU = uiq32ResultPU + *piq32Coeffs++;

    /* ((c3*x + c2)*x + c1)*x */
    uiq32ResultPU = __mpyf_l(uiq31Input, uiq32ResultPU);

    /* ((c3*x + c2)*x + c1)*x + c0 */
    uiq32ResultPU = uiq32ResultPU + *piq32Coeffs++;

    /* Check if we applied the transformation. */
    if (ui8Status & 4) {
        /* atan(y/x) = pi/2 - uiq32ResultPU */
        uiq32ResultPU = (uint32_t)(0x40000000 - uiq32ResultPU);
    }

    /* Check if the result needs to be mirrored to the 2nd/3rd quadrants. */
    if (ui8Status & 2) {
        /* atan(y/x) = pi - uiq32ResultPU */
        uiq32ResultPU = (uint32_t)(0x80000000 - uiq32ResultPU);
    }

    /* Round and convert result to correct format (radians/PU and iqN type). */
    if (type == TYPE_PU) {
        uiq32ResultPU += (uint_fast32_t)1 << (31 - q_value);
        iqNResult = uiq32ResultPU >> (32 - q_value);
    } else {
        /*
         * Multiply the per-unit result by 2*pi:
         *
         *     iq31mpy(iq32, iq28) = iq29
         */
        iq29Result = __mpyf_l(uiq32ResultPU, iq28_twoPi);

        /* Only round IQ formats < 29 */
        if (q_value < 29) {
            iq29Result += (uint_fast32_t)1 << (28 - q_value);
        }
        iqNResult = iq29Result >> (29 - q_value);
    }

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* Set the sign bit and result to correct quadrant. */
    if (ui8Status & 1) {
        return -iqNResult;
    } else {
        return iqNResult;
    }
}
#else
/**
 * @brief Compute the 4-quadrant arctangent of the IQN input
 *        and return the result, using MathACL.
 *
 * @param iqNInputY       IQN type input y.
 * @param iqNInputX       IQN type input x.
 * @param type            Specifies radians or per-unit operation.
 * @param q_value         IQ format.
 *
 * @return                IQN type result of 4-quadrant arctangent.
 */
/* Calculate atan2 using MATHACL */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNatan2)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif

__STATIC_INLINE int_fast32_t __IQNatan2(int_fast32_t iqNInputY, int_fast32_t iqNInputX, const uint8_t type, const int8_t q_value)
{
    int_fast32_t res, res1, abs_max, temp;
    int_fast32_t iqNnormX, iqNnormY, iq31normX, iq31normY;
    /* ATAN2 Operation with MatchACL requires X,Y input values to be IQ31.
     * Therefore, we need to normalize the input values.
     */

    /* Normalize input values by using the largest abs max input value */
    /* Code for _IQXabs is the same for all Q values */
    if (_IQabs(iqNInputY) > _IQabs(iqNInputX)) {
        abs_max = _IQabs(iqNInputY);
    } else {
        abs_max = _IQabs(iqNInputX);
    }

    /* Both inputs are 0 */
    if (abs_max == 0) {
        return 0;
    }

    /* IQ31 doesn't support 1.0. Therefore, we need to ensure the normalized
     * values are not equal to 1 but rather slightly below 1.0.
     */

    /* Check to see if abs_max is equal to the max value
     * represented by the specified Q value (0x7FFFFFFFF) being represented.
     * This will determine which approach is taken to ensure abs_max is > than
     * the abs value of either inputs when normalization is performed.
     */
    if (abs_max == 0x7FFFFFFF) {
        /* We want to represent as close as .999999 that we can for a given
         * q value. Therefore we want to go to the number slightly lower than
         * 1 for the given q value.
         */
        temp = (1 << q_value) - 1;

        /* Scale down the inputs slightly so we ensure that abs_max is slightly
         * larger than the abs value of the inputs.
         */
        iqNInputX = __IQNmpy(iqNInputX, temp, q_value);
        iqNInputY = __IQNmpy(iqNInputY, temp, q_value);
    } else {
        /* The decimal value 1 is the smallest positive value for a given IQ.
         * So by adding this to our variable we are using to normalize we
         * ensure the resulting normalized values are < 1.0.
         */
        abs_max += 1;
    }

    /* Normalize Inputs */
    iqNnormX = __IQNdiv_MathACL(iqNInputX, abs_max, q_value);
    iqNnormY = __IQNdiv_MathACL(iqNInputY, abs_max, q_value);

    /* Shift from IQX to IQ31 which is required for MathACL ATAN2 operation */
    iq31normX = (uint_fast32_t)iqNnormX << (31 - q_value);
    iq31normY = (uint_fast32_t)iqNnormY << (31 - q_value);

    /* MathACL ATAN2 Operation */
    MATHACL->CTL = 2 | (31 << 24);
    /* write operands to HWA */
    MATHACL->OP2 = iq31normY;
    /* write to OP1 is the trigger */
    MATHACL->OP1 = iq31normX;

    /* read atan2 */
    res1 = MATHACL->RES1;
    /* Shift from IQ31 to IQ28 for result scaling */
    res = res1 >> (3);
    /* Round and convert result to correct format (radians/PU and iqN type). */
    if (type == TYPE_PU) {
        /* IQ28(2.0) = 0x20000000 in int32 */
        res = _IQ28div(res, 0x20000000);
    } else {
        /* IQ28(PI) = 0x3243F6A8 in int32 */
        res = _IQ28mpy(0x3243F6A8, res);
    }
    /* Shift to q_value type */
    if (q_value < 28) {
        res = res >> (28 - q_value);
    } else {
        res = res << (q_value - 28);
    }

    return res;
}
#endif

/* ATAN2 */
/**
 * @brief Compute the 4-quadrant arctangent of the IQ29 input
 *        and return the result, in radians.
 *
 * @param y               IQ29 type input y.
 * @param x               IQ29 type input x.
 *
 * @return                IQ29 type result of 4-quadrant arctangent.
 */
int32_t _IQ29atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 29);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ28 input
 *        and return the result, in radians.
 *
 * @param y               IQ28 type input y.
 * @param x               IQ28 type input x.
 *
 * @return                IQ28 type result of 4-quadrant arctangent.
 */
int32_t _IQ28atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 28);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ27 input
 *        and return the result, in radians.
 *
 * @param y               IQ27 type input y.
 * @param x               IQ27 type input x.
 *
 * @return                IQ27 type result of 4-quadrant arctangent.
 */
int32_t _IQ27atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 27);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ26 input
 *        and return the result, in radians.
 *
 * @param y               IQ26 type input y.
 * @param x               IQ26 type input x.
 *
 * @return                IQ26 type result of 4-quadrant arctangent.
 */
int32_t _IQ26atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 26);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ25 input
 *        and return the result, in radians.
 *
 * @param y               IQ25 type input y.
 * @param x               IQ25 type input x.
 *
 * @return                IQ25 type result of 4-quadrant arctangent.
 */
int32_t _IQ25atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 25);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ24 input
 *        and return the result, in radians.
 *
 * @param y               IQ24 type input y.
 * @param x               IQ24 type input x.
 *
 * @return                IQ24 type result of 4-quadrant arctangent.
 */
int32_t _IQ24atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 24);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ23 input
 *        and return the result, in radians.
 *
 * @param y               IQ23 type input y.
 * @param x               IQ23 type input x.
 *
 * @return                IQ23 type result of 4-quadrant arctangent.
 */
int32_t _IQ23atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 23);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ22 input
 *        and return the result, in radians.
 *
 * @param y               IQ22 type input y.
 * @param x               IQ22 type input x.
 *
 * @return                IQ22 type result of 4-quadrant arctangent.
 */
int32_t _IQ22atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 22);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ21 input
 *        and return the result, in radians.
 *
 * @param y               IQ21 type input y.
 * @param x               IQ21 type input x.
 *
 * @return                IQ21 type result of 4-quadrant arctangent.
 */
int32_t _IQ21atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 21);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ20 input
 *        and return the result, in radians.
 *
 * @param y               IQ20 type input y.
 * @param x               IQ20 type input x.
 *
 * @return                IQ20 type result of 4-quadrant arctangent.
 */
int32_t _IQ20atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 20);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ19 input
 *        and return the result, in radians.
 *
 * @param y               IQ19 type input y.
 * @param x               IQ19 type input x.
 *
 * @return                IQ19 type result of 4-quadrant arctangent.
 */
int32_t _IQ19atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 19);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ18 input
 *        and return the result, in radians.
 *
 * @param y               IQ18 type input y.
 * @param x               IQ18 type input x.
 *
 * @return                IQ18 type result of 4-quadrant arctangent.
 */
int32_t _IQ18atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 18);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ17 input
 *        and return the result, in radians.
 *
 * @param y               IQ17 type input y.
 * @param x               IQ17 type input x.
 *
 * @return                IQ17 type result of 4-quadrant arctangent.
 */
int32_t _IQ17atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 17);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ16 input
 *        and return the result, in radians.
 *
 * @param y               IQ16 type input y.
 * @param x               IQ16 type input x.
 *
 * @return                IQ16 type result of 4-quadrant arctangent.
 */
int32_t _IQ16atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 16);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ15 input
 *        and return the result, in radians.
 *
 * @param y               IQ15 type input y.
 * @param x               IQ15 type input x.
 *
 * @return                IQ15 type result of 4-quadrant arctangent.
 */
int32_t _IQ15atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 15);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ14 input
 *        and return the result, in radians.
 *
 * @param y               IQ14 type input y.
 * @param x               IQ14 type input x.
 *
 * @return                IQ14 type result of 4-quadrant arctangent.
 */
int32_t _IQ14atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 14);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ13 input
 *        and return the result, in radians.
 *
 * @param y               IQ13 type input y.
 * @param x               IQ13 type input x.
 *
 * @return                IQ13 type result of 4-quadrant arctangent.
 */
int32_t _IQ13atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 13);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ12 input
 *        and return the result, in radians.
 *
 * @param y               IQ12 type input y.
 * @param x               IQ12 type input x.
 *
 * @return                IQ12 type result of 4-quadrant arctangent.
 */
int32_t _IQ12atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 12);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ11 input
 *        and return the result, in radians.
 *
 * @param y               IQ11 type input y.
 * @param x               IQ11 type input x.
 *
 * @return                IQ11 type result of 4-quadrant arctangent.
 */
int32_t _IQ11atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 11);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ10 input
 *        and return the result, in radians.
 *
 * @param y               IQ10 type input y.
 * @param x               IQ10 type input x.
 *
 * @return                IQ10 type result of 4-quadrant arctangent.
 */
int32_t _IQ10atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 10);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ9 input
 *        and return the result, in radians.
 *
 * @param y               IQ9 type input y.
 * @param x               IQ9 type input x.
 *
 * @return                IQ9 type result of 4-quadrant arctangent.
 */
int32_t _IQ9atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 9);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ8 input
 *        and return the result, in radians.
 *
 * @param y               IQ8 type input y.
 * @param x               IQ8 type input x.
 *
 * @return                IQ8 type result of 4-quadrant arctangent.
 */
int32_t _IQ8atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 8);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ7 input
 *        and return the result, in radians.
 *
 * @param y               IQ7 type input y.
 * @param x               IQ7 type input x.
 *
 * @return                IQ7 type result of 4-quadrant arctangent.
 */
int32_t _IQ7atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 7);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ6 input
 *        and return the result, in radians.
 *
 * @param y               IQ6 type input y.
 * @param x               IQ6 type input x.
 *
 * @return                IQ6 type result of 4-quadrant arctangent.
 */
int32_t _IQ6atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 6);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ5 input
 *        and return the result, in radians.
 *
 * @param y               IQ5 type input y.
 * @param x               IQ5 type input x.
 *
 * @return                IQ5 type result of 4-quadrant arctangent.
 */
int32_t _IQ5atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 5);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ4 input
 *        and return the result, in radians.
 *
 * @param y               IQ4 type input y.
 * @param x               IQ4 type input x.
 *
 * @return                IQ4 type result of 4-quadrant arctangent.
 */
int32_t _IQ4atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 4);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ3 input
 *        and return the result, in radians.
 *
 * @param y               IQ3 type input y.
 * @param x               IQ3 type input x.
 *
 * @return                IQ3 type result of 4-quadrant arctangent.
 */
int32_t _IQ3atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 3);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ2 input
 *        and return the result, in radians.
 *
 * @param y               IQ2 type input y.
 * @param x               IQ2 type input x.
 *
 * @return                IQ2 type result of 4-quadrant arctangent.
 */
int32_t _IQ2atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 2);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ1 input
 *        and return the result, in radians.
 *
 * @param y               IQ1 type input y.
 * @param x               IQ1 type input x.
 *
 * @return                IQ1 type result of 4-quadrant arctangent.
 */
int32_t _IQ1atan2(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_RAD, 1);
}

/* ATAN2PU */
/**
 * @brief Compute the 4-quadrant arctangent of the IQ31 input
 *        and return the result.
 *
 * @param y               IQ31 type input y.
 * @param x               IQ31 type input x.
 *
 * @return                IQ31 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ31atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 31);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ30 input
 *        and return the result.
 *
 * @param y               IQ30 type input y.
 * @param x               IQ30 type input x.
 *
 * @return                IQ30 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ30atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 30);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ29 input
 *        and return the result.
 *
 * @param y               IQ29 type input y.
 * @param x               IQ29 type input x.
 *
 * @return                IQ29 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ29atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 29);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ28 input
 *        and return the result.
 *
 * @param y               IQ28 type input y.
 * @param x               IQ28 type input x.
 *
 * @return                IQ28 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ28atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 28);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ27 input
 *        and return the result.
 *
 * @param y               IQ27 type input y.
 * @param x               IQ27 type input x.
 *
 * @return                IQ27 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ27atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 27);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ26 input
 *        and return the result.
 *
 * @param y               IQ26 type input y.
 * @param x               IQ26 type input x.
 *
 * @return                IQ26 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ26atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 26);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ25 input
 *        and return the result.
 *
 * @param y               IQ25 type input y.
 * @param x               IQ25 type input x.
 *
 * @return                IQ25 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ25atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 25);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ24 input
 *        and return the result.
 *
 * @param y               IQ24 type input y.
 * @param x               IQ24 type input x.
 *
 * @return                IQ24 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ24atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 24);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ23 input
 *        and return the result.
 *
 * @param y               IQ23 type input y.
 * @param x               IQ23 type input x.
 *
 * @return                IQ23 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ23atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 23);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ22 input
 *        and return the result.
 *
 * @param y               IQ22 type input y.
 * @param x               IQ22 type input x.
 *
 * @return                IQ22 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ22atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 22);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ21 input
 *        and return the result.
 *
 * @param y               IQ21 type input y.
 * @param x               IQ21 type input x.
 *
 * @return                IQ21 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ21atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 21);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ20 input
 *        and return the result.
 *
 * @param y               IQ20 type input y.
 * @param x               IQ20 type input x.
 *
 * @return                IQ20 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ20atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 20);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ19 input
 *        and return the result.
 *
 * @param y               IQ19 type input y.
 * @param x               IQ19 type input x.
 *
 * @return                IQ19 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ19atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 19);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ18 input
 *        and return the result.
 *
 * @param y               IQ18 type input y.
 * @param x               IQ18 type input x.
 *
 * @return                IQ18 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ18atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 18);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ17 input
 *        and return the result.
 *
 * @param y               IQ17 type input y.
 * @param x               IQ17 type input x.
 *
 * @return                IQ17 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ17atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 17);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ16 input
 *        and return the result.
 *
 * @param y               IQ16 type input y.
 * @param x               IQ16 type input x.
 *
 * @return                IQ16 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ16atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 16);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ15 input
 *        and return the result.
 *
 * @param y               IQ15 type input y.
 * @param x               IQ15 type input x.
 *
 * @return                IQ15 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ15atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 15);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ14 input
 *        and return the result.
 *
 * @param y               IQ14 type input y.
 * @param x               IQ14 type input x.
 *
 * @return                IQ14 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ14atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 14);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ13 input
 *        and return the result.
 *
 * @param y               IQ13 type input y.
 * @param x               IQ13 type input x.
 *
 * @return                IQ13 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ13atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 13);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ12 input
 *        and return the result.
 *
 * @param y               IQ12 type input y.
 * @param x               IQ12 type input x.
 *
 * @return                IQ12 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ12atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 12);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ11 input
 *        and return the result.
 *
 * @param y               IQ11 type input y.
 * @param x               IQ11 type input x.
 *
 * @return                IQ11 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ11atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 11);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ10 input
 *        and return the result.
 *
 * @param y               IQ10 type input y.
 * @param x               IQ10 type input x.
 *
 * @return                IQ10 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ10atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 10);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ9 input
 *        and return the result.
 *
 * @param y               IQ9 type input y.
 * @param x               IQ9 type input x.
 *
 * @return                IQ9 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ9atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 9);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ8 input
 *        and return the result.
 *
 * @param y               IQ8 type input y.
 * @param x               IQ8 type input x.
 *
 * @return                IQ8 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ8atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 8);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ7 input
 *        and return the result.
 *
 * @param y               IQ7 type input y.
 * @param x               IQ7 type input x.
 *
 * @return                IQ7 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ7atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 7);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ6 input
 *        and return the result.
 *
 * @param y               IQ6 type input y.
 * @param x               IQ6 type input x.
 *
 * @return                IQ6 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ6atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 6);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ5 input
 *        and return the result.
 *
 * @param y               IQ5 type input y.
 * @param x               IQ5 type input x.
 *
 * @return                IQ5 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ5atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 5);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ4 input
 *        and return the result.
 *
 * @param y               IQ4 type input y.
 * @param x               IQ4 type input x.
 *
 * @return                IQ4 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ4atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 4);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ3 input
 *        and return the result.
 *
 * @param y               IQ3 type input y.
 * @param x               IQ3 type input x.
 *
 * @return                IQ3 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ3atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 3);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ2 input
 *        and return the result.
 *
 * @param y               IQ2 type input y.
 * @param x               IQ2 type input x.
 *
 * @return                IQ2 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ2atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 2);
}
/**
 * @brief Compute the 4-quadrant arctangent of the IQ1 input
 *        and return the result.
 *
 * @param y               IQ1 type input y.
 * @param x               IQ1 type input x.
 *
 * @return                IQ1 type per-unit result of 4-quadrant arctangent.
 */
int32_t _IQ1atan2PU(int32_t y, int32_t x)
{
    return __IQNatan2(y, x, TYPE_PU, 1);
}
