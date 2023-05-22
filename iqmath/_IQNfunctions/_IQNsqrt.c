/*!****************************************************************************
 *  @file       _IQNsqrt.c
 *  @brief      Functions to compute square root, inverse square root and the
 *              magnitude of two IQN inputs.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"
#include "../include/IQmathLib.h"

/*!
 * @brief Specifies inverse square root operation type.
 */
#define TYPE_ISQRT   (0)
/*!
 * @brief Specifies square root operation type.
 */
#define TYPE_SQRT    (1)
/*!
 * @brief Specifies magnitude operation type.
 */
#define TYPE_MAG     (2)
/*!
 * @brief Specifies inverse magnitude operation type.
 */
#define TYPE_IMAG    (3)

/**
 * @brief Calculate square root, inverse square root and the magnitude of two inputs.
 *
 * @param iqNInputX         IQN type input x.
 * @param iqNInputY         IQN type input y.
 * @param type              Operation type.
 * @param q_value           IQ format.
 *
 * @return                  IQN type result of the square root or magnitude operation.
 */
/*
 * Calculate square root, inverse square root and the magnitude of two inputs
 * using a Newton-Raphson iterative method. This method takes an initial guess
 * and performs an error correction with each iteration. The equation is:
 *
 *     x1 = x0 - f(x0)/f'(x0)
 *
 * Where f' is the derivative of f. The approximation for inverse square root
 * is:
 *
 *     g' = g * (1.5 - (x/2) * g * g)
 *
 *     g' = new guess approximation
 *     g = best guess approximation
 *     x = input
 *
 * The inverse square root is multiplied by the initial input x to get the
 * square root result for square root and magnitude functions.
 *
 *     root(x) = x * 1/root(x)
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNsqrt)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNsqrt(int_fast32_t iqNInputX, int_fast32_t iqNInputY, const int8_t q_value, const int8_t type)
{
    uint8_t ui8Index;
    uint8_t ui8Loops;
    int_fast16_t i16Exponent;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    uint_fast32_t uiq30Guess;
    uint_fast32_t uiq30Result;
    uint_fast32_t uiq31Result;
    uint_fast32_t uiq32Input;

    /* If the type is (inverse) magnitude we need to calculate x^2 + y^2 first. */
    if (type == TYPE_MAG || type == TYPE_IMAG) {
        uint_fast64_t ui64Sum;

        __mpy_start(&ui16IntState, &ui16MPYState);

        /* Calculate x^2 */
        ui64Sum = __mpyx(iqNInputX, iqNInputX);

        /* Calculate y^2 and add to x^2 */
        ui64Sum += __mpyx(iqNInputY, iqNInputY);

        __mpy_stop(&ui16IntState, &ui16MPYState);

        /* Return if the magnitude is simply zero. */
        if (ui64Sum == 0) {
            return 0;
        }

        /*
         * Initialize the exponent to positive for magnitude, negative for
         * inverse magnitude.
         */
        if (type == TYPE_MAG) {
            i16Exponent = (32 - q_value);
        } else {
            i16Exponent = -(32 - q_value);
        }

        /* Shift to iq64 by keeping track of exponent. */
        while ((uint_fast16_t)(ui64Sum >> 48) < 0x4000) {
            ui64Sum <<= 2;
            /* Decrement exponent for mag */
            if (type == TYPE_MAG) {
                i16Exponent--;
            }
            /* Increment exponent for imag */
            else {
                i16Exponent++;
            }
        }

        /* Shift ui64Sum to unsigned iq32 and set as uiq32Input */
        uiq32Input = (uint_fast32_t)(ui64Sum >> 32);
    } else {
        /* check sign of input */
        if (iqNInputX <= 0) {
            return 0;
        }

        /* If the q_value gives an odd starting exponent make it even. */
        if ((32 - q_value) % 2 == 1) {
            iqNInputX <<= 1;
            /* Start with positive exponent for sqrt */
            if (type == TYPE_SQRT) {
                i16Exponent = ((32 - q_value) - 1) >> 1;
            }
            /* start with negative exponent for isqrt */
            else {
                i16Exponent = -(((32 - q_value) - 1) >> 1);
            }
        } else {
            /* start with positive exponent for sqrt */
            if (type == TYPE_SQRT) {
                i16Exponent = (32 - q_value) >> 1;
            }
            /* start with negative exponent for isqrt */
            else {
                i16Exponent = -((32 - q_value) >> 1);
            }
        }

        /* Save input as unsigned iq32. */
        uiq32Input = (uint_fast32_t)iqNInputX;

        /* Shift to iq32 by keeping track of exponent */
        while ((uint_fast16_t)(uiq32Input >> 16) < 0x4000) {
            uiq32Input <<= 2;
            /* Decrement exponent for sqrt and mag */
            if (type) {
                i16Exponent--;
            }
            /* Increment exponent for isqrt */
            else {
                i16Exponent++;
            }
        }
    }


    /* Use left most byte as index into lookup table (range: 32-128) */
    ui8Index = uiq32Input >> 25;
    ui8Index -= 32;
    uiq30Guess = (uint_fast32_t)_IQ14sqrt_lookup[ui8Index] << 16;

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpyf_start(&ui16IntState, &ui16MPYState);

    /*
     * Set the loop counter:
     *
     *     iq1 <= q_value < 24 - 2 loops
     *     iq22 <= q_value <= 31 - 3 loops
     */
    if (q_value < 24) {
        ui8Loops = 2;
    } else {
        ui8Loops = 3;
    }

    /* Iterate through Newton-Raphson algorithm. */
    while (ui8Loops--) {
        /* x*g */
        uiq31Result = __mpyf_ul(uiq32Input, uiq30Guess);

        /* x*g*g */
        uiq30Result = __mpyf_ul(uiq31Result, uiq30Guess);

        /* 3 - x*g*g */
        uiq30Result = -(uiq30Result - 0xC0000000);

        /*
         * g/2*(3 - x*g*g)
         * uiq30Guess = uiq31Guess/2
         */
        uiq30Guess = __mpyf_ul(uiq30Guess, uiq30Result);
    }

    /* Calculate sqrt(x) for both sqrt and mag */
    if (type == TYPE_SQRT || type == TYPE_MAG) {
        /*
         * uiq30Guess contains the inverse square root approximation, multiply
         * by uiq32Input to get square root result.
         */
        uiq31Result = __mpyf_ul(uiq30Guess, uiq32Input);

        __mpy_stop(&ui16IntState, &ui16MPYState);

        /*
         * Shift the result right by 31 - q_value.
         */
        i16Exponent -= (31 - q_value);

        /* Saturate value for any shift larger than 1 (only need this for mag) */
        if (type == TYPE_MAG) {
            if (i16Exponent > 0) {
                return 0x7fffffff;
            }
        }

        /* Shift left by 1 check only needed for iq30 and iq31 mag/sqrt */
        if (q_value >= 30) {
            if (i16Exponent > 0) {
                uiq31Result <<= 1;
                return uiq31Result;
            }
        }
    }
    /* Separate handling for isqrt and imag. */
    else {
        __mpy_stop(&ui16IntState, &ui16MPYState);

        /*
         * Shift the result right by 31 - q_value, add one since we use the uiq30
         * result without shifting.
         */
        i16Exponent = i16Exponent - (31 - q_value) + 1;
        uiq31Result = uiq30Guess;

        /* Saturate any positive non-zero exponent for isqrt. */
        if (i16Exponent > 0) {
            return 0x7fffffff;
        }
    }

    /* Shift uiq31Result right by -exponent */
    if (i16Exponent <= -32) {
        return 0;
    }
    if (i16Exponent <= -16) {
        uiq31Result >>= 16;
        i16Exponent += 16;
    }
    if (i16Exponent <= -8) {
        uiq31Result >>= 8;
        i16Exponent += 8;
    }
    while (i16Exponent < -1) {
        uiq31Result >>= 1;
        i16Exponent++;
    }
    if (i16Exponent) {
        uiq31Result++;
        uiq31Result >>= 1;
    }

    return uiq31Result;
}

#if ((defined (__IQMATH_USE_MATHACL__)) && (defined (__MSPM0_HAS_MATHACL__)))
/**
 * @brief Calculate square root of an  IQN input, using MathACL.
 *
 * @param iqNInputX         IQN type input.
 * @param q_value           IQ format.
 *
 * @return                  IQN type result of the square root operation.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNsqrt_MathACL)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNsqrt_MathACL(int_fast32_t iqNInputX, const int8_t q_value)
{
    /* check sign of input */
    if (iqNInputX <= 0) {
        return 0;
    }

    /* Scale factor computation:
     * output: IQ30 format value whose square root is to be computed by MATHACL
     * scale_factor: (n) where the input has been divided by 2^(2n) to render IQ30
     * value in the range (1,2).
     */
    uint32_t input, output;
    uint8_t scale_factor;
    uint8_t n = 0;

    input = iqNInputX;
    /* check input is withing 32-bit boundaries */
    if (input & 0x80000000) {
        scale_factor = 0;
        output = input;
    } else {
        n = 0;
        /* check while input != IQ30(1.0) */
        while ((input & 0x40000000) == 0) {
            n++;
            /* multiply by 2 until reaching IQ30 [1.0,2.0 range] */
            input <<= 1;
        }
        /*
         * Scale factor: take into account the shift from q_value to IQ30, the remaining value
         * is the scale factor such that scaled number = (nonscaled number)^(2^scale_factor)
         */
        scale_factor = (30 - q_value) - n;
        output = input;
    }
    /* SQRT MATHACL Operation
     * write control
     * CTL parameters are: sqrt operation | number of iterations | scale factor
     */
    MATHACL->CTL = 5 | (31 << 24) | (scale_factor << 16);
    /* write operands to HWA
     * write to OP1 is the trigger
     */
    MATHACL->OP1 = output;
    /* read sqrt
     * shift output from IQ16 to q_value
     */
    if (q_value > 16) {
        return (uint_fast32_t)MATHACL->RES1 << (q_value - 16);
    } else {
        return (uint_fast32_t)MATHACL->RES1 >> (16 - q_value);
    }
}
#endif

#if ((!defined (__IQMATH_USE_MATHACL__)) || (!defined (__MSPM0_HAS_MATHACL__)))
/* RTS SQRT */
/**
 * @brief Calculate square root of an IQ31 input.
 *
 * @param a                 IQ31 type input.
 *
 * @return                  IQ31 type result of the square root operation.
 */
int32_t _IQ31sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 31, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ30 input.
 *
 * @param a                 IQ30 type input.
 *
 * @return                  IQ30 type result of the square root operation.
 */
int32_t _IQ30sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 30, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ29 input.
 *
 * @param a                 IQ29 type input.
 *
 * @return                  IQ29 type result of the square root operation.
 */
int32_t _IQ29sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 29, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ28 input.
 *
 * @param a                 IQ28 type input.
 *
 * @return                  IQ28 type result of the square root operation.
 */
int32_t _IQ28sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 28, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ27 input.
 *
 * @param a                 IQ27 type input.
 *
 * @return                  IQ27 type result of the square root operation.
 */
int32_t _IQ27sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 27, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ26 input.
 *
 * @param a                 IQ26 type input.
 *
 * @return                  IQ26 type result of the square root operation.
 */
int32_t _IQ26sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 26, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ25 input.
 *
 * @param a                 IQ25 type input.
 *
 * @return                  IQ25 type result of the square root operation.
 */
int32_t _IQ25sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 25, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ24 input.
 *
 * @param a                 IQ24 type input.
 *
 * @return                  IQ24 type result of the square root operation.
 */
int32_t _IQ24sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 24, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ23 input.
 *
 * @param a                 IQ23 type input.
 *
 * @return                  IQ23 type result of the square root operation.
 */
int32_t _IQ23sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 23, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ22 input.
 *
 * @param a                 IQ22 type input.
 *
 * @return                  IQ22 type result of the square root operation.
 */
int32_t _IQ22sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 22, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ21 input.
 *
 * @param a                 IQ21 type input.
 *
 * @return                  IQ21 type result of the square root operation.
 */
int32_t _IQ21sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 21, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ20 input.
 *
 * @param a                 IQ20 type input.
 *
 * @return                  IQ20 type result of the square root operation.
 */
int32_t _IQ20sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 20, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ19 input.
 *
 * @param a                 IQ19 type input.
 *
 * @return                  IQ19 type result of the square root operation.
 */
int32_t _IQ19sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 19, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ18 input.
 *
 * @param a                 IQ18 type input.
 *
 * @return                  IQ18 type result of the square root operation.
 */
int32_t _IQ18sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 18, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ17 input.
 *
 * @param a                 IQ17 type input.
 *
 * @return                  IQ17 type result of the square root operation.
 */
int32_t _IQ17sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 17, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ16 input.
 *
 * @param a                 IQ16 type input.
 *
 * @return                  IQ16 type result of the square root operation.
 */
int32_t _IQ16sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 16, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ15 input.
 *
 * @param a                 IQ15 type input.
 *
 * @return                  IQ15 type result of the square root operation.
 */
int32_t _IQ15sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 15, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ14 input.
 *
 * @param a                 IQ14 type input.
 *
 * @return                  IQ14 type result of the square root operation.
 */
int32_t _IQ14sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 14, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ13 input.
 *
 * @param a                 IQ13 type input.
 *
 * @return                  IQ13 type result of the square root operation.
 */
int32_t _IQ13sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 13, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ12 input.
 *
 * @param a                 IQ12 type input.
 *
 * @return                  IQ12 type result of the square root operation.
 */
int32_t _IQ12sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 12, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ11 input.
 *
 * @param a                 IQ11 type input.
 *
 * @return                  IQ11 type result of the square root operation.
 */
int32_t _IQ11sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 11, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ10 input.
 *
 * @param a                 IQ10 type input.
 *
 * @return                  IQ10 type result of the square root operation.
 */
int32_t _IQ10sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 10, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ9 input.
 *
 * @param a                 IQ9 type input.
 *
 * @return                  IQ9 type result of the square root operation.
 */
int32_t _IQ9sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 9, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ8 input.
 *
 * @param a                 IQ8 type input.
 *
 * @return                  IQ8 type result of the square root operation.
 */
int32_t _IQ8sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 8, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ7 input.
 *
 * @param a                 IQ7 type input.
 *
 * @return                  IQ7 type result of the square root operation.
 */
int32_t _IQ7sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 7, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ6 input.
 *
 * @param a                 IQ6 type input.
 *
 * @return                  IQ6 type result of the square root operation.
 */
int32_t _IQ6sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 6, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ5 input.
 *
 * @param a                 IQ5 type input.
 *
 * @return                  IQ5 type result of the square root operation.
 */
int32_t _IQ5sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 5, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ4 input.
 *
 * @param a                 IQ4 type input.
 *
 * @return                  IQ4 type result of the square root operation.
 */
int32_t _IQ4sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 4, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ3 input.
 *
 * @param a                 IQ3 type input.
 *
 * @return                  IQ3 type result of the square root operation.
 */
int32_t _IQ3sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 3, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ2 input.
 *
 * @param a                 IQ2 type input.
 *
 * @return                  IQ2 type result of the square root operation.
 */
int32_t _IQ2sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 2, TYPE_SQRT);
}
/**
 * @brief Calculate square root of an IQ1 input.
 *
 * @param a                 IQ1 type input.
 *
 * @return                  IQ1 type result of the square root operation.
 */
int32_t _IQ1sqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 1, TYPE_SQRT);
}
#else
/* MATHACL SQRT */
/**
 * @brief Calculate square root of an IQ31 input, using MathACL.
 *
 * @param a                 IQ31 type input.
 *
 * @return                  IQ31 type result of the square root operation.
 */
int32_t _IQ31sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 31);
}
/**
 * @brief Calculate square root of an IQ30 input, using MathACL.
 *
 * @param a                 IQ30 type input.
 *
 * @return                  IQ30 type result of the square root operation.
 */
int32_t _IQ30sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 30);
}
/**
 * @brief Calculate square root of an IQ29 input, using MathACL.
 *
 * @param a                 IQ29 type input.
 *
 * @return                  IQ29 type result of the square root operation.
 */
int32_t _IQ29sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 29);
}
/**
 * @brief Calculate square root of an IQ28 input, using MathACL.
 *
 * @param a                 IQ28 type input.
 *
 * @return                  IQ28 type result of the square root operation.
 */
int32_t _IQ28sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 28);
}
/**
 * @brief Calculate square root of an IQ27 input, using MathACL.
 *
 * @param a                 IQ27 type input.
 *
 * @return                  IQ27 type result of the square root operation.
 */
int32_t _IQ27sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 27);
}
/**
 * @brief Calculate square root of an IQ26 input, using MathACL.
 *
 * @param a                 IQ26 type input.
 *
 * @return                  IQ26 type result of the square root operation.
 */
int32_t _IQ26sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 26);
}
/**
 * @brief Calculate square root of an IQ25 input, using MathACL.
 *
 * @param a                 IQ25 type input.
 *
 * @return                  IQ25 type result of the square root operation.
 */
int32_t _IQ25sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 25);
}
/**
 * @brief Calculate square root of an IQ24 input, using MathACL.
 *
 * @param a                 IQ24 type input.
 *
 * @return                  IQ24 type result of the square root operation.
 */
int32_t _IQ24sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 24);
}
/**
 * @brief Calculate square root of an IQ23 input, using MathACL.
 *
 * @param a                 IQ23 type input.
 *
 * @return                  IQ23 type result of the square root operation.
 */
int32_t _IQ23sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 23);
}
/**
 * @brief Calculate square root of an IQ22 input, using MathACL.
 *
 * @param a                 IQ22 type input.
 *
 * @return                  IQ22 type result of the square root operation.
 */
int32_t _IQ22sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 22);
}
/**
 * @brief Calculate square root of an IQ21 input, using MathACL.
 *
 * @param a                 IQ21 type input.
 *
 * @return                  IQ21 type result of the square root operation.
 */
int32_t _IQ21sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 21);
}
/**
 * @brief Calculate square root of an IQ20 input, using MathACL.
 *
 * @param a                 IQ20 type input.
 *
 * @return                  IQ20 type result of the square root operation.
 */
int32_t _IQ20sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 20);
}
/**
 * @brief Calculate square root of an IQ19 input, using MathACL.
 *
 * @param a                 IQ19 type input.
 *
 * @return                  IQ19 type result of the square root operation.
 */
int32_t _IQ19sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 19);
}
/**
 * @brief Calculate square root of an IQ18 input, using MathACL.
 *
 * @param a                 IQ18 type input.
 *
 * @return                  IQ18 type result of the square root operation.
 */
int32_t _IQ18sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 18);
}
/**
 * @brief Calculate square root of an IQ17 input, using MathACL.
 *
 * @param a                 IQ17 type input.
 *
 * @return                  IQ17 type result of the square root operation.
 */
int32_t _IQ17sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 17);
}
/**
 * @brief Calculate square root of an IQ16 input, using MathACL.
 *
 * @param a                 IQ16 type input.
 *
 * @return                  IQ16 type result of the square root operation.
 */
int32_t _IQ16sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 16);
}
/**
 * @brief Calculate square root of an IQ15 input, using MathACL.
 *
 * @param a                 IQ15 type input.
 *
 * @return                  IQ15 type result of the square root operation.
 */
int32_t _IQ15sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 15);
}
/**
 * @brief Calculate square root of an IQ14 input, using MathACL.
 *
 * @param a                 IQ14 type input.
 *
 * @return                  IQ14 type result of the square root operation.
 */
int32_t _IQ14sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 14);
}
/**
 * @brief Calculate square root of an IQ13 input, using MathACL.
 *
 * @param a                 IQ13 type input.
 *
 * @return                  IQ13 type result of the square root operation.
 */
int32_t _IQ13sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 13);
}
/**
 * @brief Calculate square root of an IQ12 input, using MathACL.
 *
 * @param a                 IQ12 type input.
 *
 * @return                  IQ12 type result of the square root operation.
 */
int32_t _IQ12sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 12);
}
/**
 * @brief Calculate square root of an IQ11 input, using MathACL.
 *
 * @param a                 IQ11 type input.
 *
 * @return                  IQ11 type result of the square root operation.
 */
int32_t _IQ11sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 11);
}
/**
 * @brief Calculate square root of an IQ10 input, using MathACL.
 *
 * @param a                 IQ10 type input.
 *
 * @return                  IQ10 type result of the square root operation.
 */
int32_t _IQ10sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 10);
}
/**
 * @brief Calculate square root of an IQ9 input, using MathACL.
 *
 * @param a                 IQ9 type input.
 *
 * @return                  IQ9 type result of the square root operation.
 */
int32_t _IQ9sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 9);
}
/**
 * @brief Calculate square root of an IQ8 input, using MathACL.
 *
 * @param a                 IQ8 type input.
 *
 * @return                  IQ8 type result of the square root operation.
 */
int32_t _IQ8sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 8);
}
/**
 * @brief Calculate square root of an IQ7 input, using MathACL.
 *
 * @param a                 IQ7 type input.
 *
 * @return                  IQ7 type result of the square root operation.
 */
int32_t _IQ7sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 7);
}
/**
 * @brief Calculate square root of an IQ6 input, using MathACL.
 *
 * @param a                 IQ6 type input.
 *
 * @return                  IQ6 type result of the square root operation.
 */
int32_t _IQ6sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 6);
}
/**
 * @brief Calculate square root of an IQ5 input, using MathACL.
 *
 * @param a                 IQ5 type input.
 *
 * @return                  IQ5 type result of the square root operation.
 */
int32_t _IQ5sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 5);
}
/**
 * @brief Calculate square root of an IQ4 input, using MathACL.
 *
 * @param a                 IQ4 type input.
 *
 * @return                  IQ4 type result of the square root operation.
 */
int32_t _IQ4sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 4);
}
/**
 * @brief Calculate square root of an IQ3 input, using MathACL.
 *
 * @param a                 IQ3 type input.
 *
 * @return                  IQ3 type result of the square root operation.
 */
int32_t _IQ3sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 3);
}
/**
 * @brief Calculate square root of an IQ2 input, using MathACL.
 *
 * @param a                 IQ2 type input.
 *
 * @return                  IQ2 type result of the square root operation.
 */
int32_t _IQ2sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 2);
}
/**
 * @brief Calculate square root of an IQ1 input, using MathACL.
 *
 * @param a                 IQ1 type input.
 *
 * @return                  IQ1 type result of the square root operation.
 */
int32_t _IQ1sqrt(int32_t a)
{
    return __IQNsqrt_MathACL(a, 1);
}
#endif

/* INVERSE SQRT */
/**
 * @brief Calculate inverse square root of an IQ30 input.
 *
 * @param a                 IQ30 type input.
 *
 * @return                  IQ30 type result of the inverse square root operation.
 */
int32_t _IQ30isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 30, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ29 input.
 *
 * @param a                 IQ29 type input.
 *
 * @return                  IQ29 type result of the inverse square root operation.
 */
int32_t _IQ29isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 29, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ28 input.
 *
 * @param a                 IQ28 type input.
 *
 * @return                  IQ28 type result of the inverse square root operation.
 */
int32_t _IQ28isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 28, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ27 input.
 *
 * @param a                 IQ27 type input.
 *
 * @return                  IQ27 type result of the inverse square root operation.
 */
int32_t _IQ27isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 27, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ26 input.
 *
 * @param a                 IQ26 type input.
 *
 * @return                  IQ26 type result of the inverse square root operation.
 */
int32_t _IQ26isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 26, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ25 input.
 *
 * @param a                 IQ25 type input.
 *
 * @return                  IQ25 type result of the inverse square root operation.
 */
int32_t _IQ25isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 25, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ24 input.
 *
 * @param a                 IQ24 type input.
 *
 * @return                  IQ24 type result of the inverse square root operation.
 */
int32_t _IQ24isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 24, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ23 input.
 *
 * @param a                 IQ23 type input.
 *
 * @return                  IQ23 type result of the inverse square root operation.
 */
int32_t _IQ23isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 23, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ22 input.
 *
 * @param a                 IQ22 type input.
 *
 * @return                  IQ22 type result of the inverse square root operation.
 */
int32_t _IQ22isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 22, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ21 input.
 *
 * @param a                 IQ21 type input.
 *
 * @return                  IQ21 type result of the inverse square root operation.
 */
int32_t _IQ21isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 21, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ20 input.
 *
 * @param a                 IQ20 type input.
 *
 * @return                  IQ20 type result of the inverse square root operation.
 */
int32_t _IQ20isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 20, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ19 input.
 *
 * @param a                 IQ19 type input.
 *
 * @return                  IQ19 type result of the inverse square root operation.
 */
int32_t _IQ19isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 19, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ18 input.
 *
 * @param a                 IQ18 type input.
 *
 * @return                  IQ18 type result of the inverse square root operation.
 */
int32_t _IQ18isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 18, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ17 input.
 *
 * @param a                 IQ17 type input.
 *
 * @return                  IQ17 type result of the inverse square root operation.
 */
int32_t _IQ17isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 17, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ16 input.
 *
 * @param a                 IQ16 type input.
 *
 * @return                  IQ16 type result of the inverse square root operation.
 */
int32_t _IQ16isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 16, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ15 input.
 *
 * @param a                 IQ15 type input.
 *
 * @return                  IQ15 type result of the inverse square root operation.
 */
int32_t _IQ15isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 15, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ14 input.
 *
 * @param a                 IQ14 type input.
 *
 * @return                  IQ14 type result of the inverse square root operation.
 */
int32_t _IQ14isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 14, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ13 input.
 *
 * @param a                 IQ13 type input.
 *
 * @return                  IQ13 type result of the inverse square root operation.
 */
int32_t _IQ13isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 13, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ12 input.
 *
 * @param a                 IQ12 type input.
 *
 * @return                  IQ12 type result of the inverse square root operation.
 */
int32_t _IQ12isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 12, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ11 input.
 *
 * @param a                 IQ11 type input.
 *
 * @return                  IQ11 type result of the inverse square root operation.
 */
int32_t _IQ11isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 11, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ10 input.
 *
 * @param a                 IQ10 type input.
 *
 * @return                  IQ10 type result of the inverse square root operation.
 */
int32_t _IQ10isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 10, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ9 input.
 *
 * @param a                 IQ9 type input.
 *
 * @return                  IQ9 type result of the inverse square root operation.
 */
int32_t _IQ9isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 9, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ8 input.
 *
 * @param a                 IQ8 type input.
 *
 * @return                  IQ8 type result of the inverse square root operation.
 */
int32_t _IQ8isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 8, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ7 input.
 *
 * @param a                 IQ7 type input.
 *
 * @return                  IQ7 type result of the inverse square root operation.
 */
int32_t _IQ7isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 7, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ6 input.
 *
 * @param a                 IQ6 type input.
 *
 * @return                  IQ6 type result of the inverse square root operation.
 */
int32_t _IQ6isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 6, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ5 input.
 *
 * @param a                 IQ5 type input.
 *
 * @return                  IQ5 type result of the inverse square root operation.
 */
int32_t _IQ5isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 5, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ4 input.
 *
 * @param a                 IQ4 type input.
 *
 * @return                  IQ4 type result of the inverse square root operation.
 */
int32_t _IQ4isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 4, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ3 input.
 *
 * @param a                 IQ3 type input.
 *
 * @return                  IQ3 type result of the inverse square root operation.
 */
int32_t _IQ3isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 3, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ2 input.
 *
 * @param a                 IQ2 type input.
 *
 * @return                  IQ2 type result of the inverse square root operation.
 */
int32_t _IQ2isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 2, TYPE_ISQRT);
}
/**
 * @brief Calculate inverse square root of an IQ1 input.
 *
 * @param a                 IQ1 type input.
 *
 * @return                  IQ1 type result of the inverse square root operation.
 */
int32_t _IQ1isqrt(int32_t a)
{
    return __IQNsqrt(a, 0, 1, TYPE_ISQRT);
}

/* MAGNITUDE */
/**
 * @brief Calculate the magnitude of two  IQ31 inputs.
 *
 * @param a                 IQ31 type input.
 * @param b                 IQ31 type input.
 *
 * @return                  IQ31 type result of the magnitude operation.
 */
int32_t _IQmag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 31, TYPE_MAG);
}

/* INVERSE MAGNITUDE */
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ30 type input.
 * @param b                 IQ30 type input.
 *
 * @return                  IQ30 type result of the inverse magnitude operation.
 */
int32_t _IQ30imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 30, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ29 type input.
 * @param b                 IQ29 type input.
 *
 * @return                  IQ29 type result of the inverse magnitude operation.
 */
int32_t _IQ29imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 29, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ28 type input.
 * @param b                 IQ28 type input.
 *
 * @return                  IQ28 type result of the inverse magnitude operation.
 */
int32_t _IQ28imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 28, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ27 type input.
 * @param b                 IQ27 type input.
 *
 * @return                  IQ27 type result of the inverse magnitude operation.
 */
int32_t _IQ27imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 27, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ26 type input.
 * @param b                 IQ26 type input.
 *
 * @return                  IQ26 type result of the inverse magnitude operation.
 */
int32_t _IQ26imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 26, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ25 type input.
 * @param b                 IQ25 type input.
 *
 * @return                  IQ25 type result of the inverse magnitude operation.
 */
int32_t _IQ25imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 25, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ24 type input.
 * @param b                 IQ24 type input.
 *
 * @return                  IQ24 type result of the inverse magnitude operation.
 */
int32_t _IQ24imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 24, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ23 type input.
 * @param b                 IQ23 type input.
 *
 * @return                  IQ23 type result of the inverse magnitude operation.
 */
int32_t _IQ23imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 23, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ22 type input.
 * @param b                 IQ22 type input.
 *
 * @return                  IQ22 type result of the inverse magnitude operation.
 */
int32_t _IQ22imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 22, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ21 type input.
 * @param b                 IQ21 type input.
 *
 * @return                  IQ21 type result of the inverse magnitude operation.
 */
int32_t _IQ21imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 21, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ20 type input.
 * @param b                 IQ20 type input.
 *
 * @return                  IQ20 type result of the inverse magnitude operation.
 */
int32_t _IQ20imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 20, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ19 type input.
 * @param b                 IQ19 type input.
 *
 * @return                  IQ19 type result of the inverse magnitude operation.
 */
int32_t _IQ19imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 19, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ18 type input.
 * @param b                 IQ18 type input.
 *
 * @return                  IQ18 type result of the inverse magnitude operation.
 */
int32_t _IQ18imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 18, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ17 type input.
 * @param b                 IQ17 type input.
 *
 * @return                  IQ17 type result of the inverse magnitude operation.
 */
int32_t _IQ17imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 17, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ16 type input.
 * @param b                 IQ16 type input.
 *
 * @return                  IQ16 type result of the inverse magnitude operation.
 */
int32_t _IQ16imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 16, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ15 type input.
 * @param b                 IQ15 type input.
 *
 * @return                  IQ15 type result of the inverse magnitude operation.
 */
int32_t _IQ15imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 15, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ14 type input.
 * @param b                 IQ14 type input.
 *
 * @return                  IQ14 type result of the inverse magnitude operation.
 */
int32_t _IQ14imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 14, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ13 type input.
 * @param b                 IQ13 type input.
 *
 * @return                  IQ13 type result of the inverse magnitude operation.
 */
int32_t _IQ13imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 13, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ12 type input.
 * @param b                 IQ12 type input.
 *
 * @return                  IQ12 type result of the inverse magnitude operation.
 */
int32_t _IQ12imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 12, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ11 type input.
 * @param b                 IQ11 type input.
 *
 * @return                  IQ11 type result of the inverse magnitude operation.
 */
int32_t _IQ11imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 11, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ10 type input.
 * @param b                 IQ10 type input.
 *
 * @return                  IQ10 type result of the inverse magnitude operation.
 */
int32_t _IQ10imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 10, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ9 type input.
 * @param b                 IQ9 type input.
 *
 * @return                  IQ9 type result of the inverse magnitude operation.
 */
int32_t _IQ9imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 9, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ8 type input.
 * @param b                 IQ8 type input.
 *
 * @return                  IQ8 type result of the inverse magnitude operation.
 */
int32_t _IQ8imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 8, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ7 type input.
 * @param b                 IQ7 type input.
 *
 * @return                  IQ7 type result of the inverse magnitude operation.
 */
int32_t _IQ7imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 7, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ6 type input.
 * @param b                 IQ6 type input.
 *
 * @return                  IQ6 type result of the inverse magnitude operation.
 */
int32_t _IQ6imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 6, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ5 type input.
 * @param b                 IQ5 type input.
 *
 * @return                  IQ5 type result of the inverse magnitude operation.
 */
int32_t _IQ5imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 5, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ4 type input.
 * @param b                 IQ4 type input.
 *
 * @return                  IQ4 type result of the inverse magnitude operation.
 */
int32_t _IQ4imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 4, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ3 type input.
 * @param b                 IQ3 type input.
 *
 * @return                  IQ3 type result of the inverse magnitude operation.
 */
int32_t _IQ3imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 3, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ2 type input.
 * @param b                 IQ2 type input.
 *
 * @return                  IQ2 type result of the inverse magnitude operation.
 */
int32_t _IQ2imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 2, TYPE_IMAG);
}
/**
 * @brief Calculate inverse magnitude of two inputs.
 *
 * @param a                 IQ1 type input.
 * @param b                 IQ1 type input.
 *
 * @return                  IQ1 type result of the inverse magnitude operation.
 */
int32_t _IQ1imag(int32_t a, int32_t b)
{
    return __IQNsqrt(a, b, 1, TYPE_IMAG);
}
