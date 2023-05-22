/*!****************************************************************************
 *  @file       _IQNrmpy.c
 *  @brief      Functions to multiply two IQ numbers, returning the product
 *  in IQ format. The result is rounded but not saturated, so if the product
 *  is greater than the minimum or maximum values for the given IQ format,
 *  the return value wraps around and produces inaccurate results.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"

/**
 * @brief Multiply two values of IQN type, with rounding.
 *
 * @param iqNInput1       IQN type value input to be multiplied.
 * @param iqNInput2       IQN type value input to be multiplied.
 * @param q_value         IQ format for result.
 *
 * @return                IQN type result of the multiplication.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNrmpy)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNrmpy(int_fast32_t iqNInput1, int_fast32_t iqNInput2, const int8_t q_value)
{
    int_fast64_t iqNResult;

    iqNResult = (int_fast64_t)iqNInput1 * (int_fast64_t)iqNInput2;
    iqNResult = iqNResult + ((uint_fast32_t)1 << (q_value - 1));
    iqNResult = iqNResult >> q_value;

    return (int_fast32_t)iqNResult;
}

/**
 * @brief Multiply two values of IQ31 type, with rounding.
 *
 * @param a               IQ31 type value input to be multiplied.
 * @param b               IQ31 type value input to be multiplied.
 *
 * @return                IQ31 type result of the multiplication.
 */
int32_t _IQ31rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 31);
}
/**
 * @brief Multiply two values of IQ30 type, with rounding.
 *
 * @param a               IQ30 type value input to be multiplied.
 * @param b               IQ30 type value input to be multiplied.
 *
 * @return                IQ30 type result of the multiplication.
 */
int32_t _IQ30rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 30);
}
/**
 * @brief Multiply two values of IQ29 type, with rounding.
 *
 * @param a               IQ29 type value input to be multiplied.
 * @param b               IQ29 type value input to be multiplied.
 *
 * @return                IQ29 type result of the multiplication.
 */
int32_t _IQ29rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 29);
}
/**
 * @brief Multiply two values of IQ28 type, with rounding.
 *
 * @param a               IQ28 type value input to be multiplied.
 * @param b               IQ28 type value input to be multiplied.
 *
 * @return                IQ28 type result of the multiplication.
 */
int32_t _IQ28rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 28);
}
/**
 * @brief Multiply two values of IQ27 type, with rounding.
 *
 * @param a               IQ27 type value input to be multiplied.
 * @param b               IQ27 type value input to be multiplied.
 *
 * @return                IQ27 type result of the multiplication.
 */
int32_t _IQ27rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 27);
}
/**
 * @brief Multiply two values of IQ26 type, with rounding.
 *
 * @param a               IQ26 type value input to be multiplied.
 * @param b               IQ26 type value input to be multiplied.
 *
 * @return                IQ26 type result of the multiplication.
 */
int32_t _IQ26rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 26);
}
/**
 * @brief Multiply two values of IQ25 type, with rounding.
 *
 * @param a               IQ25 type value input to be multiplied.
 * @param b               IQ25 type value input to be multiplied.
 *
 * @return                IQ25 type result of the multiplication.
 */
int32_t _IQ25rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 25);
}
/**
 * @brief Multiply two values of IQ24 type, with rounding.
 *
 * @param a               IQ24 type value input to be multiplied.
 * @param b               IQ24 type value input to be multiplied.
 *
 * @return                IQ24 type result of the multiplication.
 */
int32_t _IQ24rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 24);
}
/**
 * @brief Multiply two values of IQ23 type, with rounding.
 *
 * @param a               IQ23 type value input to be multiplied.
 * @param b               IQ23 type value input to be multiplied.
 *
 * @return                IQ23 type result of the multiplication.
 */
int32_t _IQ23rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 23);
}
/**
 * @brief Multiply two values of IQ22 type, with rounding.
 *
 * @param a               IQ22 type value input to be multiplied.
 * @param b               IQ22 type value input to be multiplied.
 *
 * @return                IQ22 type result of the multiplication.
 */
int32_t _IQ22rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 22);
}
/**
 * @brief Multiply two values of IQ21 type, with rounding.
 *
 * @param a               IQ21 type value input to be multiplied.
 * @param b               IQ21 type value input to be multiplied.
 *
 * @return                IQ21 type result of the multiplication.
 */
int32_t _IQ21rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 21);
}
/**
 * @brief Multiply two values of IQ20 type, with rounding.
 *
 * @param a               IQ20 type value input to be multiplied.
 * @param b               IQ20 type value input to be multiplied.
 *
 * @return                IQ20 type result of the multiplication.
 */
int32_t _IQ20rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 20);
}
/**
 * @brief Multiply two values of IQ19 type, with rounding.
 *
 * @param a               IQ19 type value input to be multiplied.
 * @param b               IQ19 type value input to be multiplied.
 *
 * @return                IQ19 type result of the multiplication.
 */
int32_t _IQ19rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 19);
}
/**
 * @brief Multiply two values of IQ18 type, with rounding.
 *
 * @param a               IQ18 type value input to be multiplied.
 * @param b               IQ18 type value input to be multiplied.
 *
 * @return                IQ18 type result of the multiplication.
 */
int32_t _IQ18rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 18);
}
/**
 * @brief Multiply two values of IQ17 type, with rounding.
 *
 * @param a               IQ17 type value input to be multiplied.
 * @param b               IQ17 type value input to be multiplied.
 *
 * @return                IQ17 type result of the multiplication.
 */
int32_t _IQ17rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 17);
}
/**
 * @brief Multiply two values of IQ16 type, with rounding.
 *
 * @param a               IQ16 type value input to be multiplied.
 * @param b               IQ16 type value input to be multiplied.
 *
 * @return                IQ16 type result of the multiplication.
 */
int32_t _IQ16rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 16);
}
/**
 * @brief Multiply two values of IQ15 type, with rounding.
 *
 * @param a               IQ15 type value input to be multiplied.
 * @param b               IQ15 type value input to be multiplied.
 *
 * @return                IQ15 type result of the multiplication.
 */
int32_t _IQ15rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 15);
}
/**
 * @brief Multiply two values of IQ14 type, with rounding.
 *
 * @param a               IQ14 type value input to be multiplied.
 * @param b               IQ14 type value input to be multiplied.
 *
 * @return                IQ14 type result of the multiplication.
 */
int32_t _IQ14rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 14);
}
/**
 * @brief Multiply two values of IQ13 type, with rounding.
 *
 * @param a               IQ13 type value input to be multiplied.
 * @param b               IQ13 type value input to be multiplied.
 *
 * @return                IQ13 type result of the multiplication.
 */
int32_t _IQ13rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 13);
}
/**
 * @brief Multiply two values of IQ12 type, with rounding.
 *
 * @param a               IQ12 type value input to be multiplied.
 * @param b               IQ12 type value input to be multiplied.
 *
 * @return                IQ12 type result of the multiplication.
 */
int32_t _IQ12rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 12);
}
/**
 * @brief Multiply two values of IQ11 type, with rounding.
 *
 * @param a               IQ11 type value input to be multiplied.
 * @param b               IQ11 type value input to be multiplied.
 *
 * @return                IQ11 type result of the multiplication.
 */
int32_t _IQ11rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 11);
}
/**
 * @brief Multiply two values of IQ10 type, with rounding.
 *
 * @param a               IQ10 type value input to be multiplied.
 * @param b               IQ10 type value input to be multiplied.
 *
 * @return                IQ10 type result of the multiplication.
 */
int32_t _IQ10rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 10);
}
/**
 * @brief Multiply two values of IQ9 type, with rounding.
 *
 * @param a               IQ9 type value input to be multiplied.
 * @param b               IQ9 type value input to be multiplied.
 *
 * @return                IQ9 type result of the multiplication.
 */
int32_t _IQ9rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 9);
}
/**
 * @brief Multiply two values of IQ8 type, with rounding.
 *
 * @param a               IQ8 type value input to be multiplied.
 * @param b               IQ8 type value input to be multiplied.
 *
 * @return                IQ8 type result of the multiplication.
 */
int32_t _IQ8rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 8);
}
/**
 * @brief Multiply two values of IQ7 type, with rounding.
 *
 * @param a               IQ7 type value input to be multiplied.
 * @param b               IQ7 type value input to be multiplied.
 *
 * @return                IQ7 type result of the multiplication.
 */
int32_t _IQ7rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 7);
}
/**
 * @brief Multiply two values of IQ6 type, with rounding.
 *
 * @param a               IQ6 type value input to be multiplied.
 * @param b               IQ6 type value input to be multiplied.
 *
 * @return                IQ6 type result of the multiplication.
 */
int32_t _IQ6rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 6);
}
/**
 * @brief Multiply two values of IQ5 type, with rounding.
 *
 * @param a               IQ5 type value input to be multiplied.
 * @param b               IQ5 type value input to be multiplied.
 *
 * @return                IQ5 type result of the multiplication.
 */
int32_t _IQ5rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 5);
}
/**
 * @brief Multiply two values of IQ4 type, with rounding.
 *
 * @param a               IQ4 type value input to be multiplied.
 * @param b               IQ4 type value input to be multiplied.
 *
 * @return                IQ4 type result of the multiplication.
 */
int32_t _IQ4rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 4);
}
/**
 * @brief Multiply two values of IQ3 type, with rounding.
 *
 * @param a               IQ3 type value input to be multiplied.
 * @param b               IQ3 type value input to be multiplied.
 *
 * @return                IQ3 type result of the multiplication.
 */
int32_t _IQ3rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 3);
}
/**
 * @brief Multiply two values of IQ2 type, with rounding.
 *
 * @param a               IQ2 type value input to be multiplied.
 * @param b               IQ2 type value input to be multiplied.
 *
 * @return                IQ2 type result of the multiplication.
 */
int32_t _IQ2rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 2);
}
/**
 * @brief Multiply two values of IQ1 type, with rounding.
 *
 * @param a               IQ1 type value input to be multiplied.
 * @param b               IQ1 type value input to be multiplied.
 *
 * @return                IQ1 type result of the multiplication.
 */
int32_t _IQ1rmpy(int32_t a, int32_t b)
{
    return __IQNrmpy(a, b, 1);
}
