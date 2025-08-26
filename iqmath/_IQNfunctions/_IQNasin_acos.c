/*!****************************************************************************
 *  @file       _IQNasin_acos.c
 *  @brief      Functions to compute the inverse sine of the input
 *              and return the result, in radians.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"

/* Hidden Q31 sqrt function. */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @brief Computes the square root of the IQ31 input.
 *
 * @param iq31Input       IQ31 type input.
 *
 * @return                IQ31 type result of square root.
 */
extern int_fast32_t _IQ31sqrt(int_fast32_t iq31Input);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the inverse sine of the IQN input.
 *
 * @param iqNInput        IQN type input.
 * @param q_value         IQ format.
 *
 * @return                IQN type result of inverse sine.
 */
/*
 * Calculate asin using a 4th order Taylor series for inputs in the range of
 * zero to 0.5. The coefficients are stored in a lookup table with 17 ranges
 * to give an accuracy of 26 bits.
 *
 * For inputs greater than 0.5 we apply the following transformation:
 *
 *     asin(x) = PI/2 - 2*asin(sqrt((1 - x)/2))
 *
 * This transformation is derived from the following trig identities:
 *
 *     (1) asin(x) = PI/2 - acos(x)
 *     (2) sin(t/2)^2 = (1 - cos(t))/2
 *     (3) cos(t) = x
 *     (4) t = acos(x)
 *
 * Identity (2) can be simplified to give equation (5):
 *
 *     (5) t = 2*asin(sqrt((1 - cos(t))/2))
 *
 * Substituting identities (3) and (4) into equation (5) gives equation (6):
 *
 *     (6) acos(x) = 2*asin(sqrt((1 - x)/2))
 *
 * The final step is substituting equation (6) into identity (1):
 *
 *     asin(x) = PI/2 - 2*asin(sqrt((1 - x)/2))
 *
 * Acos is implemented using asin and identity (1).
 */

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNasin)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNasin(int_fast32_t iqNInput, const int8_t q_value)
{
    uint8_t ui8Status = 0;
    uint_fast16_t index;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    int_fast32_t iq29Result;
    const int_fast32_t *piq29Coeffs;
    uint_fast32_t uiq31Input;
    uint_fast32_t uiq31InputTemp;

    /*
     * Extract the sign from the input and set the following status bits:
     *
     *      ui8Status = xxxxxxTS
     *      x = unused
     *      T = transform was applied
     *      S = sign bit needs to be set (-y)
     */
    if (iqNInput < 0) {
        ui8Status |= 1;
        iqNInput = -iqNInput;
    }

    /*
     * Check if input is within the valid input range:
     *
     *     0 <= iqNInput <= 1
     */
    if (iqNInput > ((uint_fast32_t)1 << q_value)) {
        return 0;
    }

    /* Convert input to unsigned iq31. */
    uiq31Input = (uint_fast32_t)iqNInput << (31 - q_value);

    /*
     * Apply the transformation from asin to acos if input is greater than 0.5.
     * The first step is to calculate the following:
     *
     *     (sqrt((1 - uiq31Input)/2))
     */
    uiq31InputTemp = 0x80000000 - uiq31Input;
    if (uiq31InputTemp < 0x40000000) {
        /* Halve the result. */
        uiq31Input = uiq31InputTemp >> 1;

        /* Calculate sqrt((1 - uiq31Input)/2) */
        uiq31Input = _IQ31sqrt(uiq31Input);

        /* Flag that the transformation was used. */
        ui8Status |= 2;
    }

    /* Calculate the index using the left 6 most bits of the input. */
    index = (int_fast16_t)(uiq31Input >> 26) & 0x003f;

    /* Set the coefficient pointer. */
    piq29Coeffs = _IQ29Asin_coeffs[index];

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpyf_start(&ui16IntState, &ui16MPYState);

    /*
     * Calculate asin(x) using the following Taylor series:
     *
     *     asin(x) = (((c4*x + c3)*x + c2)*x + c1)*x + c0
     */

    /* c4*x */
    iq29Result = __mpyf_l(uiq31Input, *piq29Coeffs++);

    /* c4*x + c3 */
    iq29Result = iq29Result + *piq29Coeffs++;

    /* (c4*x + c3)*x */
    iq29Result = __mpyf_l(uiq31Input, iq29Result);

    /* (c4*x + c3)*x + c2 */
    iq29Result = iq29Result + *piq29Coeffs++;

    /* ((c4*x + c3)*x + c2)*x */
    iq29Result = __mpyf_l(uiq31Input, iq29Result);

    /* ((c4*x + c3)*x + c2)*x + c1 */
    iq29Result = iq29Result + *piq29Coeffs++;

    /* (((c4*x + c3)*x + c2)*x + c1)*x */
    iq29Result = __mpyf_l(uiq31Input, iq29Result);

    /* (((c4*x + c3)*x + c2)*x + c1)*x + c0 */
    iq29Result = iq29Result + *piq29Coeffs++;

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* check if we switched to acos */
    if (ui8Status & 2) {
        /* asin(x) = pi/2 - 2*iq29Result */
        iq29Result = iq29Result << 1;
        iq29Result -= iq29_halfPi;      // this is equivalent to the above
        iq29Result = -iq29Result;       // but avoids using temporary registers
    }

    /* Shift iq29 result to specified q_value. */
    iq29Result >>= 29 - q_value;

    /* Add sign to the result. */
    if (ui8Status & 1) {
        iq29Result = -iq29Result;
    }

    return iq29Result;
}

/* ASIN */
/**
 * @brief Computes the inverse sine of the IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type result of inverse sine.
 */
int32_t _IQ29asin(int32_t a)
{
    return __IQNasin(a, 29);
}
/**
 * @brief Computes the inverse sine of the IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type result of inverse sine.
 */
int32_t _IQ28asin(int32_t a)
{
    return __IQNasin(a, 28);
}
/**
 * @brief Computes the inverse sine of the IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type result of inverse sine.
 */
int32_t _IQ27asin(int32_t a)
{
    return __IQNasin(a, 27);
}
/**
 * @brief Computes the inverse sine of the IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type result of inverse sine.
 */
int32_t _IQ26asin(int32_t a)
{
    return __IQNasin(a, 26);
}
/**
 * @brief Computes the inverse sine of the IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type result of inverse sine.
 */
int32_t _IQ25asin(int32_t a)
{
    return __IQNasin(a, 25);
}
/**
 * @brief Computes the inverse sine of the IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type result of inverse sine.
 */
int32_t _IQ24asin(int32_t a)
{
    return __IQNasin(a, 24);
}
/**
 * @brief Computes the inverse sine of the IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type result of inverse sine.
 */
int32_t _IQ23asin(int32_t a)
{
    return __IQNasin(a, 23);
}
/**
 * @brief Computes the inverse sine of the IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type result of inverse sine.
 */
int32_t _IQ22asin(int32_t a)
{
    return __IQNasin(a, 22);
}
/**
 * @brief Computes the inverse sine of the IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type result of inverse sine.
 */
int32_t _IQ21asin(int32_t a)
{
    return __IQNasin(a, 21);
}
/**
 * @brief Computes the inverse sine of the IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type result of inverse sine.
 */
int32_t _IQ20asin(int32_t a)
{
    return __IQNasin(a, 20);
}
/**
 * @brief Computes the inverse sine of the IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type result of inverse sine.
 */
int32_t _IQ19asin(int32_t a)
{
    return __IQNasin(a, 19);
}
/**
 * @brief Computes the inverse sine of the IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type result of inverse sine.
 */
int32_t _IQ18asin(int32_t a)
{
    return __IQNasin(a, 18);
}
/**
 * @brief Computes the inverse sine of the IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type result of inverse sine.
 */
int32_t _IQ17asin(int32_t a)
{
    return __IQNasin(a, 17);
}
/**
 * @brief Computes the inverse sine of the IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type result of inverse sine.
 */
int32_t _IQ16asin(int32_t a)
{
    return __IQNasin(a, 16);
}
/**
 * @brief Computes the inverse sine of the IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type result of inverse sine.
 */
int32_t _IQ15asin(int32_t a)
{
    return __IQNasin(a, 15);
}
/**
 * @brief Computes the inverse sine of the IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type result of inverse sine.
 */
int32_t _IQ14asin(int32_t a)
{
    return __IQNasin(a, 14);
}
/**
 * @brief Computes the inverse sine of the IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type result of inverse sine.
 */
int32_t _IQ13asin(int32_t a)
{
    return __IQNasin(a, 13);
}
/**
 * @brief Computes the inverse sine of the IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type result of inverse sine.
 */
int32_t _IQ12asin(int32_t a)
{
    return __IQNasin(a, 12);
}
/**
 * @brief Computes the inverse sine of the IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type result of inverse sine.
 */
int32_t _IQ11asin(int32_t a)
{
    return __IQNasin(a, 11);
}
/**
 * @brief Computes the inverse sine of the IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type result of inverse sine.
 */
int32_t _IQ10asin(int32_t a)
{
    return __IQNasin(a, 10);
}
/**
 * @brief Computes the inverse sine of the IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type result of inverse sine.
 */
int32_t _IQ9asin(int32_t a)
{
    return __IQNasin(a, 9);
}
/**
 * @brief Computes the inverse sine of the IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type result of inverse sine.
 */
int32_t _IQ8asin(int32_t a)
{
    return __IQNasin(a, 8);
}
/**
 * @brief Computes the inverse sine of the IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type result of inverse sine.
 */
int32_t _IQ7asin(int32_t a)
{
    return __IQNasin(a, 7);
}
/**
 * @brief Computes the inverse sine of the IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type result of inverse sine.
 */
int32_t _IQ6asin(int32_t a)
{
    return __IQNasin(a, 6);
}
/**
 * @brief Computes the inverse sine of the IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type result of inverse sine.
 */
int32_t _IQ5asin(int32_t a)
{
    return __IQNasin(a, 5);
}
/**
 * @brief Computes the inverse sine of the IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type result of inverse sine.
 */
int32_t _IQ4asin(int32_t a)
{
    return __IQNasin(a, 4);
}
/**
 * @brief Computes the inverse sine of the IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type result of inverse sine.
 */
int32_t _IQ3asin(int32_t a)
{
    return __IQNasin(a, 3);
}
/**
 * @brief Computes the inverse sine of the IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type result of inverse sine.
 */
int32_t _IQ2asin(int32_t a)
{
    return __IQNasin(a, 2);
}
/**
 * @brief Computes the inverse sine of the IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type result of inverse sine.
 */
int32_t _IQ1asin(int32_t a)
{
    return __IQNasin(a, 1);
}
