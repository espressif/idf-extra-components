/*!****************************************************************************
 *  @file       _IQNrsmpy.c
 *  @brief      Functions to multiply two IQ numbers, returning the product in
 *  IQ format. The result is rounded and saturated, so if the product is
 * greater than the minimum or maximum values for the given IQ format, the
 * return value is saturated to the minimum or maximum value for the given IQ
 * format (as appropriate).
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"

/**
 * @brief Multiplies two IQN numbers, with rounding and saturation.
 *
 * @param iqNInput1       IQN type value input to be multiplied.
 * @param iqNInput2       IQN type value input to be multiplied.
 * @param q_value         IQ format for result.
 *
 * @return                IQN type result of the multiplication.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNrsmpy)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNrsmpy(int_fast32_t iqNInput1, int_fast32_t iqNInput2, const int8_t q_value)
{
    int_fast64_t iqNResult;

    iqNResult = (int_fast64_t)iqNInput1 * (int_fast64_t)iqNInput2;
    iqNResult = iqNResult + ((uint_fast32_t)1 << (q_value - 1));
    iqNResult = iqNResult >> q_value;

    if (iqNResult > INT32_MAX) {
        return INT32_MAX;
    } else if (iqNResult < INT32_MIN) {
        return INT32_MIN;
    } else {
        return (int_fast32_t)iqNResult;
    }
}

/**
 * @brief Multiplies two IQ31 numbers, with rounding and saturation.
 *
 * @param a               IQ31 type value input to be multiplied.
 * @param b               IQ31 type value input to be multiplied.
 *
 * @return                IQ31 type result of the multiplication.
 */
int32_t _IQ31rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 31);
}
/**
 * @brief Multiplies two IQ30 numbers, with rounding and saturation.
 *
 * @param a               IQ30 type value input to be multiplied.
 * @param b               IQ30 type value input to be multiplied.
 *
 * @return                IQ30 type result of the multiplication.
 */
int32_t _IQ30rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 30);
}
/**
 * @brief Multiplies two IQ29 numbers, with rounding and saturation.
 *
 * @param a               IQ29 type value input to be multiplied.
 * @param b               IQ29 type value input to be multiplied.
 *
 * @return                IQ29 type result of the multiplication.
 */
int32_t _IQ29rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 29);
}
/**
 * @brief Multiplies two IQ28 numbers, with rounding and saturation.
 *
 * @param a               IQ28 type value input to be multiplied.
 * @param b               IQ28 type value input to be multiplied.
 *
 * @return                IQ28 type result of the multiplication.
 */
int32_t _IQ28rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 28);
}
/**
 * @brief Multiplies two IQ27 numbers, with rounding and saturation.
 *
 * @param a               IQ27 type value input to be multiplied.
 * @param b               IQ27 type value input to be multiplied.
 *
 * @return                IQ27 type result of the multiplication.
 */
int32_t _IQ27rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 27);
}
/**
 * @brief Multiplies two IQ26 numbers, with rounding and saturation.
 *
 * @param a               IQ26 type value input to be multiplied.
 * @param b               IQ26 type value input to be multiplied.
 *
 * @return                IQ26 type result of the multiplication.
 */
int32_t _IQ26rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 26);
}
/**
 * @brief Multiplies two IQ25 numbers, with rounding and saturation.
 *
 * @param a               IQ25 type value input to be multiplied.
 * @param b               IQ25 type value input to be multiplied.
 *
 * @return                IQ25 type result of the multiplication.
 */
int32_t _IQ25rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 25);
}
/**
 * @brief Multiplies two IQ24 numbers, with rounding and saturation.
 *
 * @param a               IQ24 type value input to be multiplied.
 * @param b               IQ24 type value input to be multiplied.
 *
 * @return                IQ24 type result of the multiplication.
 */
int32_t _IQ24rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 24);
}
/**
 * @brief Multiplies two IQ23 numbers, with rounding and saturation.
 *
 * @param a               IQ23 type value input to be multiplied.
 * @param b               IQ23 type value input to be multiplied.
 *
 * @return                IQ23 type result of the multiplication.
 */
int32_t _IQ23rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 23);
}
/**
 * @brief Multiplies two IQ22 numbers, with rounding and saturation.
 *
 * @param a               IQ22 type value input to be multiplied.
 * @param b               IQ22 type value input to be multiplied.
 *
 * @return                IQ22 type result of the multiplication.
 */
int32_t _IQ22rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 22);
}
/**
 * @brief Multiplies two IQ21 numbers, with rounding and saturation.
 *
 * @param a               IQ21 type value input to be multiplied.
 * @param b               IQ21 type value input to be multiplied.
 *
 * @return                IQ21 type result of the multiplication.
 */
int32_t _IQ21rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 21);
}
/**
 * @brief Multiplies two IQ20 numbers, with rounding and saturation.
 *
 * @param a               IQ20 type value input to be multiplied.
 * @param b               IQ20 type value input to be multiplied.
 *
 * @return                IQ20 type result of the multiplication.
 */
int32_t _IQ20rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 20);
}
/**
 * @brief Multiplies two IQ19 numbers, with rounding and saturation.
 *
 * @param a               IQ19 type value input to be multiplied.
 * @param b               IQ19 type value input to be multiplied.
 *
 * @return                IQ19 type result of the multiplication.
 */
int32_t _IQ19rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 19);
}
/**
 * @brief Multiplies two IQ18 numbers, with rounding and saturation.
 *
 * @param a               IQ18 type value input to be multiplied.
 * @param b               IQ18 type value input to be multiplied.
 *
 * @return                IQ18 type result of the multiplication.
 */
int32_t _IQ18rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 18);
}
/**
 * @brief Multiplies two IQ17 numbers, with rounding and saturation.
 *
 * @param a               IQ17 type value input to be multiplied.
 * @param b               IQ17 type value input to be multiplied.
 *
 * @return                IQ17 type result of the multiplication.
 */
int32_t _IQ17rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 17);
}
/**
 * @brief Multiplies two IQ16 numbers, with rounding and saturation.
 *
 * @param a               IQ16 type value input to be multiplied.
 * @param b               IQ16 type value input to be multiplied.
 *
 * @return                IQ16 type result of the multiplication.
 */
int32_t _IQ16rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 16);
}
/**
 * @brief Multiplies two IQ15 numbers, with rounding and saturation.
 *
 * @param a               IQ15 type value input to be multiplied.
 * @param b               IQ15 type value input to be multiplied.
 *
 * @return                IQ15 type result of the multiplication.
 */
int32_t _IQ15rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 15);
}
/**
 * @brief Multiplies two IQ14 numbers, with rounding and saturation.
 *
 * @param a               IQ14 type value input to be multiplied.
 * @param b               IQ14 type value input to be multiplied.
 *
 * @return                IQ14 type result of the multiplication.
 */
int32_t _IQ14rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 14);
}
/**
 * @brief Multiplies two IQ13 numbers, with rounding and saturation.
 *
 * @param a               IQ13 type value input to be multiplied.
 * @param b               IQ13 type value input to be multiplied.
 *
 * @return                IQ13 type result of the multiplication.
 */
int32_t _IQ13rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 13);
}
/**
 * @brief Multiplies two IQ12 numbers, with rounding and saturation.
 *
 * @param a               IQ12 type value input to be multiplied.
 * @param b               IQ12 type value input to be multiplied.
 *
 * @return                IQ12 type result of the multiplication.
 */
int32_t _IQ12rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 12);
}
/**
 * @brief Multiplies two IQ11 numbers, with rounding and saturation.
 *
 * @param a               IQ11 type value input to be multiplied.
 * @param b               IQ11 type value input to be multiplied.
 *
 * @return                IQ11 type result of the multiplication.
 */
int32_t _IQ11rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 11);
}
/**
 * @brief Multiplies two IQ10 numbers, with rounding and saturation.
 *
 * @param a               IQ10 type value input to be multiplied.
 * @param b               IQ10 type value input to be multiplied.
 *
 * @return                IQ10 type result of the multiplication.
 */
int32_t _IQ10rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 10);
}
/**
 * @brief Multiplies two IQ9 numbers, with rounding and saturation.
 *
 * @param a               IQ9 type value input to be multiplied.
 * @param b               IQ9 type value input to be multiplied.
 *
 * @return                IQ9 type result of the multiplication.
 */
int32_t _IQ9rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 9);
}
/**
 * @brief Multiplies two IQ8 numbers, with rounding and saturation.
 *
 * @param a               IQ8 type value input to be multiplied.
 * @param b               IQ8 type value input to be multiplied.
 *
 * @return                IQ8 type result of the multiplication.
 */
int32_t _IQ8rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 8);
}
/**
 * @brief Multiplies two IQ7 numbers, with rounding and saturation.
 *
 * @param a               IQ7 type value input to be multiplied.
 * @param b               IQ7 type value input to be multiplied.
 *
 * @return                IQ7 type result of the multiplication.
 */
int32_t _IQ7rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 7);
}
/**
 * @brief Multiplies two IQ6 numbers, with rounding and saturation.
 *
 * @param a               IQ6 type value input to be multiplied.
 * @param b               IQ6 type value input to be multiplied.
 *
 * @return                IQ6 type result of the multiplication.
 */
int32_t _IQ6rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 6);
}
/**
 * @brief Multiplies two IQ5 numbers, with rounding and saturation.
 *
 * @param a               IQ5 type value input to be multiplied.
 * @param b               IQ5 type value input to be multiplied.
 *
 * @return                IQ5 type result of the multiplication.
 */
int32_t _IQ5rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 5);
}
/**
 * @brief Multiplies two IQ4 numbers, with rounding and saturation.
 *
 * @param a               IQ4 type value input to be multiplied.
 * @param b               IQ4 type value input to be multiplied.
 *
 * @return                IQ4 type result of the multiplication.
 */
int32_t _IQ4rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 4);
}
/**
 * @brief Multiplies two IQ3 numbers, with rounding and saturation.
 *
 * @param a               IQ3 type value input to be multiplied.
 * @param b               IQ3 type value input to be multiplied.
 *
 * @return                IQ3 type result of the multiplication.
 */
int32_t _IQ3rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 3);
}
/**
 * @brief Multiplies two IQ2 numbers, with rounding and saturation.
 *
 * @param a               IQ2 type value input to be multiplied.
 * @param b               IQ2 type value input to be multiplied.
 *
 * @return                IQ2 type result of the multiplication.
 */
int32_t _IQ2rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 2);
}
/**
 * @brief Multiplies two IQ1 numbers, with rounding and saturation.
 *
 * @param a               IQ1 type value input to be multiplied.
 * @param b               IQ1 type value input to be multiplied.
 *
 * @return                IQ1 type result of the multiplication.
 */
int32_t _IQ1rsmpy(int32_t a, int32_t b)
{
    return __IQNrsmpy(a, b, 1);
}
