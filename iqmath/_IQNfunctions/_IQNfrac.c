/*!****************************************************************************
 *  @file       _IQNfrac.c
 *  @brief      Functions to return the fractional portion of the input.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"

/**
 * @brief Return the fractional portion of an IQN input.
 *
 * @param iqNInput        IQN type input.
 * @param q_value         IQ format.
 *
 * @return                IQN type fractional portion of input.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNfrac)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNfrac(int_fast32_t iqNInput, int8_t q_value)
{
    int_fast32_t iqNInteger;

    iqNInteger = (uint_fast32_t)iqNInput & ((uint_fast32_t)0xffffffff << q_value);

    return (iqNInput - iqNInteger);
}

/**
 * @brief Return the fractional portion of an IQ30 input.
 *
 * @param a               IQ30 type input.
 *
 * @return                IQ30 type fractional portion of input.
 */
int32_t _IQ30frac(int32_t a)
{
    return __IQNfrac(a, 30);
}
/**
 * @brief Return the fractional portion of an IQ29 input.
 *
 * @param a               IQ29 type input.
 *
 * @return                IQ29 type fractional portion of input.
 */
int32_t _IQ29frac(int32_t a)
{
    return __IQNfrac(a, 29);
}
/**
 * @brief Return the fractional portion of an IQ28 input.
 *
 * @param a               IQ28 type input.
 *
 * @return                IQ28 type fractional portion of input.
 */
int32_t _IQ28frac(int32_t a)
{
    return __IQNfrac(a, 28);
}
/**
 * @brief Return the fractional portion of an IQ27 input.
 *
 * @param a               IQ27 type input.
 *
 * @return                IQ27 type fractional portion of input.
 */
int32_t _IQ27frac(int32_t a)
{
    return __IQNfrac(a, 27);
}
/**
 * @brief Return the fractional portion of an IQ26 input.
 *
 * @param a               IQ26 type input.
 *
 * @return                IQ26 type fractional portion of input.
 */
int32_t _IQ26frac(int32_t a)
{
    return __IQNfrac(a, 26);
}
/**
 * @brief Return the fractional portion of an IQ25 input.
 *
 * @param a               IQ25 type input.
 *
 * @return                IQ25 type fractional portion of input.
 */
int32_t _IQ25frac(int32_t a)
{
    return __IQNfrac(a, 25);
}
/**
 * @brief Return the fractional portion of an IQ24 input.
 *
 * @param a               IQ24 type input.
 *
 * @return                IQ24 type fractional portion of input.
 */
int32_t _IQ24frac(int32_t a)
{
    return __IQNfrac(a, 24);
}
/**
 * @brief Return the fractional portion of an IQ23 input.
 *
 * @param a               IQ23 type input.
 *
 * @return                IQ23 type fractional portion of input.
 */
int32_t _IQ23frac(int32_t a)
{
    return __IQNfrac(a, 23);
}
/**
 * @brief Return the fractional portion of an IQ22 input.
 *
 * @param a               IQ22 type input.
 *
 * @return                IQ22 type fractional portion of input.
 */
int32_t _IQ22frac(int32_t a)
{
    return __IQNfrac(a, 22);
}
/**
 * @brief Return the fractional portion of an IQ21 input.
 *
 * @param a               IQ21 type input.
 *
 * @return                IQ21 type fractional portion of input.
 */
int32_t _IQ21frac(int32_t a)
{
    return __IQNfrac(a, 21);
}
/**
 * @brief Return the fractional portion of an IQ20 input.
 *
 * @param a               IQ20 type input.
 *
 * @return                IQ20 type fractional portion of input.
 */
int32_t _IQ20frac(int32_t a)
{
    return __IQNfrac(a, 20);
}
/**
 * @brief Return the fractional portion of an IQ19 input.
 *
 * @param a               IQ19 type input.
 *
 * @return                IQ19 type fractional portion of input.
 */
int32_t _IQ19frac(int32_t a)
{
    return __IQNfrac(a, 19);
}
/**
 * @brief Return the fractional portion of an IQ18 input.
 *
 * @param a               IQ18 type input.
 *
 * @return                IQ18 type fractional portion of input.
 */
int32_t _IQ18frac(int32_t a)
{
    return __IQNfrac(a, 18);
}
/**
 * @brief Return the fractional portion of an IQ17 input.
 *
 * @param a               IQ17 type input.
 *
 * @return                IQ17 type fractional portion of input.
 */
int32_t _IQ17frac(int32_t a)
{
    return __IQNfrac(a, 17);
}
/**
 * @brief Return the fractional portion of an IQ16 input.
 *
 * @param a               IQ16 type input.
 *
 * @return                IQ16 type fractional portion of input.
 */
int32_t _IQ16frac(int32_t a)
{
    return __IQNfrac(a, 16);
}
/**
 * @brief Return the fractional portion of an IQ15 input.
 *
 * @param a               IQ15 type input.
 *
 * @return                IQ15 type fractional portion of input.
 */
int32_t _IQ15frac(int32_t a)
{
    return __IQNfrac(a, 15);
}
/**
 * @brief Return the fractional portion of an IQ14 input.
 *
 * @param a               IQ14 type input.
 *
 * @return                IQ14 type fractional portion of input.
 */
int32_t _IQ14frac(int32_t a)
{
    return __IQNfrac(a, 14);
}
/**
 * @brief Return the fractional portion of an IQ13 input.
 *
 * @param a               IQ13 type input.
 *
 * @return                IQ13 type fractional portion of input.
 */
int32_t _IQ13frac(int32_t a)
{
    return __IQNfrac(a, 13);
}
/**
 * @brief Return the fractional portion of an IQ12 input.
 *
 * @param a               IQ12 type input.
 *
 * @return                IQ12 type fractional portion of input.
 */
int32_t _IQ12frac(int32_t a)
{
    return __IQNfrac(a, 12);
}
/**
 * @brief Return the fractional portion of an IQ11 input.
 *
 * @param a               IQ11 type input.
 *
 * @return                IQ11 type fractional portion of input.
 */
int32_t _IQ11frac(int32_t a)
{
    return __IQNfrac(a, 11);
}
/**
 * @brief Return the fractional portion of an IQ10 input.
 *
 * @param a               IQ10 type input.
 *
 * @return                IQ10 type fractional portion of input.
 */
int32_t _IQ10frac(int32_t a)
{
    return __IQNfrac(a, 10);
}
/**
 * @brief Return the fractional portion of an IQ9 input.
 *
 * @param a               IQ9 type input.
 *
 * @return                IQ9 type fractional portion of input.
 */
int32_t _IQ9frac(int32_t a)
{
    return __IQNfrac(a, 9);
}
/**
 * @brief Return the fractional portion of an IQ8 input.
 *
 * @param a               IQ8 type input.
 *
 * @return                IQ8 type fractional portion of input.
 */
int32_t _IQ8frac(int32_t a)
{
    return __IQNfrac(a, 8);
}
/**
 * @brief Return the fractional portion of an IQ7 input.
 *
 * @param a               IQ7 type input.
 *
 * @return                IQ7 type fractional portion of input.
 */
int32_t _IQ7frac(int32_t a)
{
    return __IQNfrac(a, 7);
}
/**
 * @brief Return the fractional portion of an IQ6 input.
 *
 * @param a               IQ6 type input.
 *
 * @return                IQ6 type fractional portion of input.
 */
int32_t _IQ6frac(int32_t a)
{
    return __IQNfrac(a, 6);
}
/**
 * @brief Return the fractional portion of an IQ5 input.
 *
 * @param a               IQ5 type input.
 *
 * @return                IQ5 type fractional portion of input.
 */
int32_t _IQ5frac(int32_t a)
{
    return __IQNfrac(a, 5);
}
/**
 * @brief Return the fractional portion of an IQ4 input.
 *
 * @param a               IQ4 type input.
 *
 * @return                IQ4 type fractional portion of input.
 */
int32_t _IQ4frac(int32_t a)
{
    return __IQNfrac(a, 4);
}
/**
 * @brief Return the fractional portion of an IQ3 input.
 *
 * @param a               IQ3 type input.
 *
 * @return                IQ3 type fractional portion of input.
 */
int32_t _IQ3frac(int32_t a)
{
    return __IQNfrac(a, 3);
}
/**
 * @brief Return the fractional portion of an IQ2 input.
 *
 * @param a               IQ2 type input.
 *
 * @return                IQ2 type fractional portion of input.
 */
int32_t _IQ2frac(int32_t a)
{
    return __IQNfrac(a, 2);
}
/**
 * @brief Return the fractional portion of an IQ1 input.
 *
 * @param a               IQ1 type input.
 *
 * @return                IQ1 type fractional portion of input.
 */
int32_t _IQ1frac(int32_t a)
{
    return __IQNfrac(a, 1);
}
