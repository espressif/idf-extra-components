/*!****************************************************************************
 *  @file       _IQNsin_cos.c
 *  @brief      Functions to compute the sine and cosine of the input
 *              and return the result.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"
#include "../include/IQmathLib.h"

/*!
 * @brief The value of PI
 */
#define PI (3.1415926536)

/*!
 * @brief Used to specify sine operation
 */
#define TYPE_SIN     (0)
/*!
 * @brief Used to specify cosine operation
 */
#define TYPE_COS     (1)
/*!
 * @brief Used to specify result in radians
 */
#define TYPE_RAD     (0)
/*!
 * @brief Used to specify per-unit result
 */
#define TYPE_PU      (1)


#if ((!defined (__IQMATH_USE_MATHACL__)) || (!defined (__MSPM0_HAS_MATHACL__)))
/**
 * @brief Computes the sine of an UIQ31 input.
 *
 * @param uiq31Input      UIQ31 type input.
 *
 * @return                UIQ31 type result of sine.
 */
/*
 * Perform the calculation where the input is only in the first quadrant
 * using one of the following two functions.
 *
 * This algorithm is derived from the following trig identities:
 *     sin(k + x) = sin(k)*cos(x) + cos(k)*sin(x)
 *     cos(k + x) = cos(k)*cos(x) - sin(k)*sin(x)
 *
 * First we calculate an index k and the remainder x according to the following
 * formulas:
 *
 *     k = 0x3F & int(Radian*64)
 *     x = fract(Radian*64)/64
 *
 * Two lookup tables store the values of sin(k) and cos(k) for all possible
 * indexes. The remainder, x, is calculated using second order Taylor series.
 *
 *     sin(x) = x - (x^3)/6     (~36.9 bits of accuracy)
 *     cos(x) = 1 - (x^2)/2     (~28.5 bits of accuracy)
 *
 * Combining the trig identities with the Taylor series approximiations gives
 * the following two functions:
 *
 *     cos(Radian) = C(k) + x*(-S(k) + 0.5*x*(-C(k) + 0.333*x*S(k)))
 *     sin(Radian) = S(k) + x*(C(k) + 0.5*x*(-S(k) - 0.333*x*C(k)))
 *
 *     where  S(k) = Sin table value at offset "k"
 *            C(k) = Cos table value at offset "k"
 *
 * Using a lookup table with a 64 bit index (52 indexes since the input range is
 * only 0 - 0.785398) and second order Taylor series gives 28 bits of accuracy.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNcalcSin)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNcalcSin(uint_fast32_t uiq31Input)
{
    uint_fast16_t index;
    int_fast32_t iq31X;
    int_fast32_t iq31Sin;
    int_fast32_t iq31Cos;
    int_fast32_t iq31Res;

    /* Calculate index for sin and cos lookup using bits 31:26 */
    index = (uint_fast16_t)(uiq31Input >> 25) & 0x003f;

    /* Lookup S(k) and C(k) values. */
    iq31Sin = _IQ31SinLookup[index];
    iq31Cos = _IQ31CosLookup[index];

    /*
     * Calculated x (the remainder) by subtracting the index from the unsigned
     * iq31 input. This can be accomplished by masking out the bits used for
     * the index.
     */
    iq31X = uiq31Input & 0x01ffffff;

    /* 0.333*x*C(k) */
    iq31Res = __mpyf_l(0x2aaaaaab, iq31X);
    iq31Res = __mpyf_l(iq31Cos, iq31Res);

    /* -S(k) - 0.333*x*C(k) */
    iq31Res = -(iq31Sin + iq31Res);

    /* 0.5*x*(-S(k) - 0.333*x*C(k)) */
    iq31Res = iq31Res >> 1;
    iq31Res = __mpyf_l(iq31X, iq31Res);

    /* C(k) + 0.5*x*(-S(k) - 0.333*x*C(k)) */
    iq31Res = iq31Cos + iq31Res;

    /* x*(C(k) + 0.5*x*(-S(k) - 0.333*x*C(k))) */
    iq31Res = __mpyf_l(iq31X, iq31Res);

    /* sin(Radian) = S(k) + x*(C(k) + 0.5*x*(-S(k) - 0.333*x*C(k))) */
    iq31Res = iq31Sin + iq31Res;

    return iq31Res;
}
/**
 * @brief Computes the cosine of an UIQ31 input.
 *
 * @param uiq31Input      UIQ31 type input.
 *
 * @return                UIQ31 type result of cosine.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNcalcCos)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNcalcCos(uint_fast32_t uiq31Input)
{
    uint_fast16_t index;
    int_fast32_t iq31X;
    int_fast32_t iq31Sin;
    int_fast32_t iq31Cos;
    int_fast32_t iq31Res;

    /* Calculate index for sin and cos lookup using bits 31:26 */
    index = (uint_fast16_t)(uiq31Input >> 25) & 0x003f;

    /* Lookup S(k) and C(k) values. */
    iq31Sin = _IQ31SinLookup[index];
    iq31Cos = _IQ31CosLookup[index];

    /*
     * Calculated x (the remainder) by subtracting the index from the unsigned
     * iq31 input. This can be accomplished by masking out the bits used for
     * the index.
     */
    iq31X = uiq31Input & 0x01ffffff;

    /* 0.333*x*S(k) */
    iq31Res = __mpyf_l(0x2aaaaaab, iq31X);
    iq31Res = __mpyf_l(iq31Sin, iq31Res);

    /* -C(k) + 0.333*x*S(k) */
    iq31Res = iq31Res - iq31Cos;

    /* 0.5*x*(-C(k) + 0.333*x*S(k)) */
    iq31Res = iq31Res >> 1;
    iq31Res = __mpyf_l(iq31X, iq31Res);

    /* -S(k) + 0.5*x*(-C(k) + 0.333*x*S(k)) */
    iq31Res = iq31Res - iq31Sin;

    /* x*(-S(k) + 0.5*x*(-C(k) + 0.333*x*S(k))) */
    iq31Res = __mpyf_l(iq31X, iq31Res);

    /* cos(Radian) = C(k) + x*(-S(k) + 0.5*x*(-C(k) + 0.333*x*S(k))) */
    iq31Res = iq31Cos + iq31Res;

    return iq31Res;
}

/**
 * @brief Computes the sine or cosine of an IQN input.
 *
 * @param iqNInput        IQN type input.
 * @param q_value         IQ format.
 * @param type            Specifies sine or cosine operation.
 * @param format          Specifies radians or per-unit operation.
 *
 * @return                IQN type result of sin or cosine operation.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNsin_cos)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNsin_cos(int_fast32_t iqNInput, const int8_t q_value,
        const int8_t type, const int8_t format)
{
    uint8_t ui8Sign = 0;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    uint_fast32_t uiq29Input;
    uint_fast32_t uiq30Input;
    uint_fast32_t uiq31Input;
    uint_fast32_t uiq32Input;
    uint_fast32_t uiq31Result = 0;

    /* Remove sign from input */
    if (iqNInput < 0) {
        iqNInput = -iqNInput;

        /* Flip sign only for sin */
        if (type == TYPE_SIN) {
            ui8Sign = 1;
        }
    }

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpyf_start(&ui16IntState, &ui16MPYState);

    /* Per unit API */
    if (format == TYPE_PU) {
        /*
         * Scale input to unsigned iq32 to allow for maximum range. This removes
         * the integer component of the per unit input.
         */
        uiq32Input = (uint_fast32_t)iqNInput << (32 - q_value);

        /* Reduce the input to the first two quadrants. */
        if (uiq32Input >= 0x80000000) {
            uiq32Input -= 0x80000000;
            ui8Sign ^= 1;
        }

        /*
         * Multiply unsigned iq32 input by 2*pi and scale to unsigned iq30:
         *     iq32 * iq30 = iq30 * 2
         */
        uiq30Input = __mpyf_ul(uiq32Input, iq30_pi);

    }
    /* Radians API */
    else {
        /* Calculate the exponent difference from input format to iq29. */
        int_fast16_t exp = 29 - q_value;

        /* Save input as unsigned iq29 format. */
        uiq29Input = (uint_fast32_t)iqNInput;

        /* Reduce the input exponent to zero by scaling by 2*pi. */
        while (exp) {
            if (uiq29Input >= iq29_pi) {
                uiq29Input -= iq29_pi;
            }
            uiq29Input <<= 1;
            exp--;
        }

        /* Reduce the range to the first two quadrants. */
        if (uiq29Input >= iq29_pi) {
            uiq29Input -= iq29_pi;
            ui8Sign ^= 1;
        }

        /* Scale the unsigned iq29 input to unsigned iq30. */
        uiq30Input = uiq29Input << 1;
    }

    /* Reduce the iq30 input range to the first quadrant. */
    if (uiq30Input >= iq30_halfPi) {
        uiq30Input = iq30_pi - uiq30Input;

        /* flip sign for cos calculations */
        if (type == TYPE_COS) {
            ui8Sign ^= 1;
        }
    }

    /* Convert the unsigned iq30 input to unsigned iq31 */
    uiq31Input = uiq30Input << 1;

    /* Only one of these cases will be compiled per function. */
    if (type == TYPE_COS) {
        /* If input is greater than pi/4 use sin for calculations */
        if (uiq31Input > iq31_quarterPi) {
            uiq31Input = iq31_halfPi - uiq31Input;
            uiq31Result = __IQNcalcSin(uiq31Input);
        } else {
            uiq31Result = __IQNcalcCos(uiq31Input);
        }
    } else if (type == TYPE_SIN) {
        /* If input is greater than pi/4 use cos for calculations */
        if (uiq31Input > iq31_quarterPi) {
            uiq31Input = iq31_halfPi - uiq31Input;
            uiq31Result = __IQNcalcCos(uiq31Input);
        } else {
            uiq31Result = __IQNcalcSin(uiq31Input);
        }
    }

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* Shift to Q type */
    uiq31Result >>= (31 - q_value);

    /* set sign */
    if (ui8Sign) {
        uiq31Result = -uiq31Result;
    }

    return uiq31Result;
}
#else
/**
 * @brief Computes the sine or cosine of an IQN input, using MathACL.
 *
 * @param iqNInput        IQN type input.
 * @param q_value         IQ format.
 * @param type            Specifies sine or cosine operation.
 * @param format          Specifies radians or per-unit operation.
 *
 * @return                IQN type result of sin or cosine operation.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNsin_cos)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNsin_cos(int_fast32_t iqNInput, const int8_t q_value,
        const int8_t type, const int8_t format)
{
    int_fast32_t res, res1, resMult, resDiv;
    int_fast32_t iq31input;
    /* Per unit API */
    if (format == TYPE_PU) {
        /* multiply by 2 for MathACL scaling. */
        resMult = (uint_fast32_t)iqNInput << (1);
        /* shift to IQ31 for sin/cos calculation */
        iq31input = (uint_fast32_t)resMult << (31 - q_value);
    }
    /* Radians API */
    else {
        /* divide by PI for MathACL scaling
            * write control
            */
        MATHACL->CTL = 4 | (q_value << 8) | (1 << 5);
        /* write operands to HWA. OP2 = divisor, OP1 = dividend */
        MATHACL->OP2 = ((uint_fast32_t)((PI) * ((uint_fast32_t)1 << q_value)));
        /* trigger is write to OP1 */
        MATHACL->OP1 = iqNInput;
        /* read quotient and remainder */
        resDiv = MATHACL->RES1;
        /* shift from q_value to IQ31 for sin/cos calculation */
        iq31input = (uint_fast32_t)resDiv << (31 - q_value);
    }
    /*
     * write control
     * operation = sincos, iterations = 31
     */
    MATHACL->CTL = 1 | (31 << 24);
    /* write operand to HWA */
    MATHACL->OP1 = iq31input;
    if (type == TYPE_COS) {
        /* read cosine */
        res1 = MATHACL->RES1;
    } else if (type == TYPE_SIN) {
        /* read sine */
        res1 = MATHACL->RES2;
    }
    /* Shift to q_value type */
    res = res1 >> (31 - q_value);
    return res;
}
#endif

/* IQ sin functions */

/**
 * @brief Computes the cosine of an IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type result of cosine operation, in radians.
 */
int32_t _IQ29sin(int32_t a)
{
    return __IQNsin_cos(a, 29, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type result of sine operation, in radians.
 */
int32_t _IQ28sin(int32_t a)
{
    return __IQNsin_cos(a, 28, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type result of sine operation, in radians.
 */
int32_t _IQ27sin(int32_t a)
{
    return __IQNsin_cos(a, 27, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type result of sine operation, in radians.
 */
int32_t _IQ26sin(int32_t a)
{
    return __IQNsin_cos(a, 26, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type result of sine operation, in radians.
 */
int32_t _IQ25sin(int32_t a)
{
    return __IQNsin_cos(a, 25, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type result of sine operation, in radians.
 */
int32_t _IQ24sin(int32_t a)
{
    return __IQNsin_cos(a, 24, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type result of sine operation, in radians.
 */
int32_t _IQ23sin(int32_t a)
{
    return __IQNsin_cos(a, 23, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type result of sine operation, in radians.
 */
int32_t _IQ22sin(int32_t a)
{
    return __IQNsin_cos(a, 22, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type result of sine operation, in radians.
 */
int32_t _IQ21sin(int32_t a)
{
    return __IQNsin_cos(a, 21, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type result of sine operation, in radians.
 */
int32_t _IQ20sin(int32_t a)
{
    return __IQNsin_cos(a, 20, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type result of sine operation, in radians.
 */
int32_t _IQ19sin(int32_t a)
{
    return __IQNsin_cos(a, 19, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type result of sine operation, in radians.
 */
int32_t _IQ18sin(int32_t a)
{
    return __IQNsin_cos(a, 18, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type result of sine operation, in radians.
 */
int32_t _IQ17sin(int32_t a)
{
    return __IQNsin_cos(a, 17, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type result of sine operation, in radians.
 */
int32_t _IQ16sin(int32_t a)
{
    return __IQNsin_cos(a, 16, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type result of sine operation, in radians.
 */
int32_t _IQ15sin(int32_t a)
{
    return __IQNsin_cos(a, 15, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type result of sine operation, in radians.
 */
int32_t _IQ14sin(int32_t a)
{
    return __IQNsin_cos(a, 14, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type result of sine operation, in radians.
 */
int32_t _IQ13sin(int32_t a)
{
    return __IQNsin_cos(a, 13, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type result of sine operation, in radians.
 */
int32_t _IQ12sin(int32_t a)
{
    return __IQNsin_cos(a, 12, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type result of sine operation, in radians.
 */
int32_t _IQ11sin(int32_t a)
{
    return __IQNsin_cos(a, 11, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type result of sine operation, in radians.
 */
int32_t _IQ10sin(int32_t a)
{
    return __IQNsin_cos(a, 10, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type result of sine operation, in radians.
 */
int32_t _IQ9sin(int32_t a)
{
    return __IQNsin_cos(a, 9, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type result of sine operation, in radians.
 */
int32_t _IQ8sin(int32_t a)
{
    return __IQNsin_cos(a, 8, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type result of sine operation, in radians.
 */
int32_t _IQ7sin(int32_t a)
{
    return __IQNsin_cos(a, 7, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type result of sine operation, in radians.
 */
int32_t _IQ6sin(int32_t a)
{
    return __IQNsin_cos(a, 6, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type result of sine operation, in radians.
 */
int32_t _IQ5sin(int32_t a)
{
    return __IQNsin_cos(a, 5, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type result of sine operation, in radians.
 */
int32_t _IQ4sin(int32_t a)
{
    return __IQNsin_cos(a, 4, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type result of sine operation, in radians.
 */
int32_t _IQ3sin(int32_t a)
{
    return __IQNsin_cos(a, 3, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type result of sine operation, in radians.
 */
int32_t _IQ2sin(int32_t a)
{
    return __IQNsin_cos(a, 2, TYPE_SIN, TYPE_RAD);
}
/**
 * @brief Computes the sine of an IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type result of sine operation, in radians.
 */
int32_t _IQ1sin(int32_t a)
{
    return __IQNsin_cos(a, 1, TYPE_SIN, TYPE_RAD);
}

/* IQ cos functions */
/**
 * @brief Computes the cosine of an IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type result of cosine operation, in radians.
 */
int32_t _IQ29cos(int32_t a)
{
    return __IQNsin_cos(a, 29, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type result of cosine operation, in radians.
 */
int32_t _IQ28cos(int32_t a)
{
    return __IQNsin_cos(a, 28, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type result of cosine operation, in radians.
 */
int32_t _IQ27cos(int32_t a)
{
    return __IQNsin_cos(a, 27, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type result of cosine operation, in radians.
 */
int32_t _IQ26cos(int32_t a)
{
    return __IQNsin_cos(a, 26, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type result of cosine operation, in radians.
 */
int32_t _IQ25cos(int32_t a)
{
    return __IQNsin_cos(a, 25, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type result of cosine operation, in radians.
 */
int32_t _IQ24cos(int32_t a)
{
    return __IQNsin_cos(a, 24, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type result of cosine operation, in radians.
 */
int32_t _IQ23cos(int32_t a)
{
    return __IQNsin_cos(a, 23, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type result of cosine operation, in radians.
 */
int32_t _IQ22cos(int32_t a)
{
    return __IQNsin_cos(a, 22, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type result of cosine operation, in radians.
 */
int32_t _IQ21cos(int32_t a)
{
    return __IQNsin_cos(a, 21, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type result of cosine operation, in radians.
 */
int32_t _IQ20cos(int32_t a)
{
    return __IQNsin_cos(a, 20, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type result of cosine operation, in radians.
 */
int32_t _IQ19cos(int32_t a)
{
    return __IQNsin_cos(a, 19, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type result of cosine operation, in radians.
 */
int32_t _IQ18cos(int32_t a)
{
    return __IQNsin_cos(a, 18, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type result of cosine operation, in radians.
 */
int32_t _IQ17cos(int32_t a)
{
    return __IQNsin_cos(a, 17, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type result of cosine operation, in radians.
 */
int32_t _IQ16cos(int32_t a)
{
    return __IQNsin_cos(a, 16, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type result of cosine operation, in radians.
 */
int32_t _IQ15cos(int32_t a)
{
    return __IQNsin_cos(a, 15, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type result of cosine operation, in radians.
 */
int32_t _IQ14cos(int32_t a)
{
    return __IQNsin_cos(a, 14, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type result of cosine operation, in radians.
 */
int32_t _IQ13cos(int32_t a)
{
    return __IQNsin_cos(a, 13, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type result of cosine operation, in radians.
 */
int32_t _IQ12cos(int32_t a)
{
    return __IQNsin_cos(a, 12, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type result of cosine operation, in radians.
 */
int32_t _IQ11cos(int32_t a)
{
    return __IQNsin_cos(a, 11, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type result of cosine operation, in radians.
 */
int32_t _IQ10cos(int32_t a)
{
    return __IQNsin_cos(a, 10, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type result of cosine operation, in radians.
 */
int32_t _IQ9cos(int32_t a)
{
    return __IQNsin_cos(a, 9, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type result of cosine operation, in radians.
 */
int32_t _IQ8cos(int32_t a)
{
    return __IQNsin_cos(a, 8, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type result of cosine operation, in radians.
 */
int32_t _IQ7cos(int32_t a)
{
    return __IQNsin_cos(a, 7, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type result of cosine operation, in radians.
 */
int32_t _IQ6cos(int32_t a)
{
    return __IQNsin_cos(a, 6, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type result of cosine operation, in radians.
 */
int32_t _IQ5cos(int32_t a)
{
    return __IQNsin_cos(a, 5, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type result of cosine operation, in radians.
 */
int32_t _IQ4cos(int32_t a)
{
    return __IQNsin_cos(a, 4, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type result of cosine operation, in radians.
 */
int32_t _IQ3cos(int32_t a)
{
    return __IQNsin_cos(a, 3, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type result of cosine operation, in radians.
 */
int32_t _IQ2cos(int32_t a)
{
    return __IQNsin_cos(a, 2, TYPE_COS, TYPE_RAD);
}
/**
 * @brief Computes the cosine of an IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type result of cosine operation, in radians.
 */
int32_t _IQ1cos(int32_t a)
{
    return __IQNsin_cos(a, 1, TYPE_COS, TYPE_RAD);
}

/* IQ sinPU functions */
/**
 * @brief Computes the sine of an IQ31 input.
 *
 * @param a               IQ31 type input.
 *
 * @return                IQ31 type per-unit result of sine operation.
 */
int32_t _IQ31sinPU(int32_t a)
{
    return __IQNsin_cos(a, 31, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ30 input.
 *
 * @param a               IQ30 type input.
 *
 * @return                IQ30 type per-unit result of sine operation.
 */
int32_t _IQ30sinPU(int32_t a)
{
    return __IQNsin_cos(a, 30, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type per-unit result of sine operation.
 */
int32_t _IQ29sinPU(int32_t a)
{
    return __IQNsin_cos(a, 29, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type per-unit result of sine operation.
 */
int32_t _IQ28sinPU(int32_t a)
{
    return __IQNsin_cos(a, 28, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type per-unit result of sine operation.
 */
int32_t _IQ27sinPU(int32_t a)
{
    return __IQNsin_cos(a, 27, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type per-unit result of sine operation.
 */
int32_t _IQ26sinPU(int32_t a)
{
    return __IQNsin_cos(a, 26, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type per-unit result of sine operation.
 */
int32_t _IQ25sinPU(int32_t a)
{
    return __IQNsin_cos(a, 25, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type per-unit result of sine operation.
 */
int32_t _IQ24sinPU(int32_t a)
{
    return __IQNsin_cos(a, 24, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type per-unit result of sine operation.
 */
int32_t _IQ23sinPU(int32_t a)
{
    return __IQNsin_cos(a, 23, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type per-unit result of sine operation.
 */
int32_t _IQ22sinPU(int32_t a)
{
    return __IQNsin_cos(a, 22, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type per-unit result of sine operation.
 */
int32_t _IQ21sinPU(int32_t a)
{
    return __IQNsin_cos(a, 21, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type per-unit result of sine operation.
 */
int32_t _IQ20sinPU(int32_t a)
{
    return __IQNsin_cos(a, 20, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type per-unit result of sine operation.
 */
int32_t _IQ19sinPU(int32_t a)
{
    return __IQNsin_cos(a, 19, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type per-unit result of sine operation.
 */
int32_t _IQ18sinPU(int32_t a)
{
    return __IQNsin_cos(a, 18, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type per-unit result of sine operation.
 */
int32_t _IQ17sinPU(int32_t a)
{
    return __IQNsin_cos(a, 17, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type per-unit result of sine operation.
 */
int32_t _IQ16sinPU(int32_t a)
{
    return __IQNsin_cos(a, 16, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type per-unit result of sine operation.
 */
int32_t _IQ15sinPU(int32_t a)
{
    return __IQNsin_cos(a, 15, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type per-unit result of sine operation.
 */
int32_t _IQ14sinPU(int32_t a)
{
    return __IQNsin_cos(a, 14, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type per-unit result of sine operation.
 */
int32_t _IQ13sinPU(int32_t a)
{
    return __IQNsin_cos(a, 13, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type per-unit result of sine operation.
 */
int32_t _IQ12sinPU(int32_t a)
{
    return __IQNsin_cos(a, 12, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type per-unit result of sine operation.
 */
int32_t _IQ11sinPU(int32_t a)
{
    return __IQNsin_cos(a, 11, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type per-unit result of sine operation.
 */
int32_t _IQ10sinPU(int32_t a)
{
    return __IQNsin_cos(a, 10, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type per-unit result of sine operation.
 */
int32_t _IQ9sinPU(int32_t a)
{
    return __IQNsin_cos(a, 9, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type per-unit result of sine operation.
 */
int32_t _IQ8sinPU(int32_t a)
{
    return __IQNsin_cos(a, 8, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type per-unit result of sine operation.
 */
int32_t _IQ7sinPU(int32_t a)
{
    return __IQNsin_cos(a, 7, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type per-unit result of sine operation.
 */
int32_t _IQ6sinPU(int32_t a)
{
    return __IQNsin_cos(a, 6, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type per-unit result of sine operation.
 */
int32_t _IQ5sinPU(int32_t a)
{
    return __IQNsin_cos(a, 5, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type per-unit result of sine operation.
 */
int32_t _IQ4sinPU(int32_t a)
{
    return __IQNsin_cos(a, 4, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type per-unit result of sine operation.
 */
int32_t _IQ3sinPU(int32_t a)
{
    return __IQNsin_cos(a, 3, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type per-unit result of sine operation.
 */
int32_t _IQ2sinPU(int32_t a)
{
    return __IQNsin_cos(a, 2, TYPE_SIN, TYPE_PU);
}
/**
 * @brief Computes the sine of an IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type per-unit result of sine operation.
 */
int32_t _IQ1sinPU(int32_t a)
{
    return __IQNsin_cos(a, 1, TYPE_SIN, TYPE_PU);
}

/* IQ cosPU functions */
/**
 * @brief Computes the cosine of an IQ31 input.
 *
 * @param a               IQ31 type input.
 *
 * @return                IQ31 type per-unit result of cosine operation.
 */
int32_t _IQ31cosPU(int32_t a)
{
    return __IQNsin_cos(a, 31, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ30 input.
 *
 * @param a               IQ30 type input.
 *
 * @return                IQ30 type per-unit result of cosine operation.
 */
int32_t _IQ30cosPU(int32_t a)
{
    return __IQNsin_cos(a, 30, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type per-unit result of cosine operation.
 */
int32_t _IQ29cosPU(int32_t a)
{
    return __IQNsin_cos(a, 29, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type per-unit result of cosine operation.
 */
int32_t _IQ28cosPU(int32_t a)
{
    return __IQNsin_cos(a, 28, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type per-unit result of cosine operation.
 */
int32_t _IQ27cosPU(int32_t a)
{
    return __IQNsin_cos(a, 27, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type per-unit result of cosine operation.
 */
int32_t _IQ26cosPU(int32_t a)
{
    return __IQNsin_cos(a, 26, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type per-unit result of cosine operation.
 */
int32_t _IQ25cosPU(int32_t a)
{
    return __IQNsin_cos(a, 25, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type per-unit result of cosine operation.
 */
int32_t _IQ24cosPU(int32_t a)
{
    return __IQNsin_cos(a, 24, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type per-unit result of cosine operation.
 */
int32_t _IQ23cosPU(int32_t a)
{
    return __IQNsin_cos(a, 23, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type per-unit result of cosine operation.
 */
int32_t _IQ22cosPU(int32_t a)
{
    return __IQNsin_cos(a, 22, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type per-unit result of cosine operation.
 */
int32_t _IQ21cosPU(int32_t a)
{
    return __IQNsin_cos(a, 21, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type per-unit result of cosine operation.
 */
int32_t _IQ20cosPU(int32_t a)
{
    return __IQNsin_cos(a, 20, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type per-unit result of cosine operation.
 */
int32_t _IQ19cosPU(int32_t a)
{
    return __IQNsin_cos(a, 19, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type per-unit result of cosine operation.
 */
int32_t _IQ18cosPU(int32_t a)
{
    return __IQNsin_cos(a, 18, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type per-unit result of cosine operation.
 */
int32_t _IQ17cosPU(int32_t a)
{
    return __IQNsin_cos(a, 17, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type per-unit result of cosine operation.
 */
int32_t _IQ16cosPU(int32_t a)
{
    return __IQNsin_cos(a, 16, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type per-unit result of cosine operation.
 */
int32_t _IQ15cosPU(int32_t a)
{
    return __IQNsin_cos(a, 15, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type per-unit result of cosine operation.
 */
int32_t _IQ14cosPU(int32_t a)
{
    return __IQNsin_cos(a, 14, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type per-unit result of cosine operation.
 */
int32_t _IQ13cosPU(int32_t a)
{
    return __IQNsin_cos(a, 13, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type per-unit result of cosine operation.
 */
int32_t _IQ12cosPU(int32_t a)
{
    return __IQNsin_cos(a, 12, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type per-unit result of cosine operation.
 */
int32_t _IQ11cosPU(int32_t a)
{
    return __IQNsin_cos(a, 11, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type per-unit result of cosine operation.
 */
int32_t _IQ10cosPU(int32_t a)
{
    return __IQNsin_cos(a, 10, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type per-unit result of cosine operation.
 */
int32_t _IQ9cosPU(int32_t a)
{
    return __IQNsin_cos(a, 9, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type per-unit result of cosine operation.
 */
int32_t _IQ8cosPU(int32_t a)
{
    return __IQNsin_cos(a, 8, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type per-unit result of cosine operation.
 */
int32_t _IQ7cosPU(int32_t a)
{
    return __IQNsin_cos(a, 7, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type per-unit result of cosine operation.
 */
int32_t _IQ6cosPU(int32_t a)
{
    return __IQNsin_cos(a, 6, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type per-unit result of cosine operation.
 */
int32_t _IQ5cosPU(int32_t a)
{
    return __IQNsin_cos(a, 5, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type per-unit result of cosine operation.
 */
int32_t _IQ4cosPU(int32_t a)
{
    return __IQNsin_cos(a, 4, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type per-unit result of cosine operation.
 */
int32_t _IQ3cosPU(int32_t a)
{
    return __IQNsin_cos(a, 3, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type per-unit result of cosine operation.
 */
int32_t _IQ2cosPU(int32_t a)
{
    return __IQNsin_cos(a, 2, TYPE_COS, TYPE_PU);
}
/**
 * @brief Computes the cosine of an IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type per-unit result of cosine operation.
 */
int32_t _IQ1cosPU(int32_t a)
{
    return __IQNsin_cos(a, 1, TYPE_COS, TYPE_PU);
}
