/*!****************************************************************************
 *  @file       _IQNmpyIQX.c
 *  @brief      Functions to multiply two IQ numbers in different IQ formats,
 * returning the product in a third IQ format.  The result is neither rounded
 * nor saturated, so if the product is greater than the minimum or maximum
 * values for the given output IQ format, the return value will wrap around
 * and produce inaccurate results.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"
#include "_IQNtables.h"

/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in a third IQ format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 * @param q_value         IQ format for result.
 *
 * @return                IQN type result of the multiplication.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNmpyIQX)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __IQNmpyIQX(int_fast32_t a, int n1, int_fast32_t b, int n2, int8_t q_value)
{
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    int_fast32_t i32Shift;
    int_fast64_t i64Result;

    /*
     * Mark the start of any multiplies. This will disable interrupts and set
     * the multiplier to fractional mode. This is designed to reduce overhead
     * of constantly switching states when using repeated multiplies (MSP430
     * only).
     */
    __mpy_start(&ui16IntState, &ui16MPYState);

    i64Result = __mpyx(a, b);

    /*
     * Mark the end of all multiplies. This restores MPY and interrupt states
     * (MSP430 only).
     */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    i32Shift = (n1 + n2) - q_value;

    if (i32Shift > 0) {
        i64Result >>= i32Shift;
    } else {
        i64Result <<= -i32Shift;
    }

    return (int_fast32_t)i64Result;
}

/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ30 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ30 type result of the multiplication.
 */
int32_t _IQ30mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 30);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ29 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ29 type result of the multiplication.
 */
int32_t _IQ29mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 29);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ28 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ28 type result of the multiplication.
 */
int32_t _IQ28mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 28);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ27 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ27 type result of the multiplication.
 */
int32_t _IQ27mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 27);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ26 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ26 type result of the multiplication.
 */
int32_t _IQ26mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 26);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ25 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ25 type result of the multiplication.
 */
int32_t _IQ25mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 25);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ24 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ24 type result of the multiplication.
 */
int32_t _IQ24mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 24);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ23 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ23 type result of the multiplication.
 */
int32_t _IQ23mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 23);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ22 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ22 type result of the multiplication.
 */
int32_t _IQ22mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 22);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ21 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ21 type result of the multiplication.
 */
int32_t _IQ21mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 21);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ20 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ20 type result of the multiplication.
 */
int32_t _IQ20mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 20);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ19 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ19 type result of the multiplication.
 */
int32_t _IQ19mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 19);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ18 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ18 type result of the multiplication.
 */
int32_t _IQ18mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 18);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ17 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ17 type result of the multiplication.
 */
int32_t _IQ17mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 17);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ16 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ16 type result of the multiplication.
 */
int32_t _IQ16mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 16);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ15 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ15 type result of the multiplication.
 */
int32_t _IQ15mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 15);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ14 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ14 type result of the multiplication.
 */
int32_t _IQ14mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 14);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ13 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ13 type result of the multiplication.
 */
int32_t _IQ13mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 13);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ12 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ12 type result of the multiplication.
 */
int32_t _IQ12mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 12);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ11 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ11 type result of the multiplication.
 */
int32_t _IQ11mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 11);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ10 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ10 type result of the multiplication.
 */
int32_t _IQ10mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 10);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ9 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ9 type result of the multiplication.
 */
int32_t _IQ9mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 9);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ8 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ8 type result of the multiplication.
 */
int32_t _IQ8mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 8);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ7 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ7 type result of the multiplication.
 */
int32_t _IQ7mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 7);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ6 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ6 type result of the multiplication.
 */
int32_t _IQ6mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 6);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ5 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ5 type result of the multiplication.
 */
int32_t _IQ5mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 5);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ4 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ4 type result of the multiplication.
 */
int32_t _IQ4mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 4);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ3 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ3 type result of the multiplication.
 */
int32_t _IQ3mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 3);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ2 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ2 type result of the multiplication.
 */
int32_t _IQ2mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 2);
}
/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in  IQ1 format.
 *
 * @param a               IQN1 type value input to be multiplied.
 * @param n1              IQ format for first value.
 * @param b               IQN2 type value input to be multiplied.
 * @param n2              IQ format for second value.
 *
 * @return                IQ1 type result of the multiplication.
 */
int32_t _IQ1mpyIQX(int32_t a, int n1, int32_t b, int n2)
{
    return __IQNmpyIQX(a, n1, b, n2, 1);
}
