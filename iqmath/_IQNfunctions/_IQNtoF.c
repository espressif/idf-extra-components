/*!****************************************************************************
 *  @file       _IQNtoF.c
 *  @brief      Functions to convert IQN type to floating point.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>
#include <string.h>

#include "../support/support.h"

/**
 * @brief Converts IQN type to floating point.
 *
 * @param iqNInput        IQN type value input to be converted.
 * @param q_value         IQ format.
 *
 * @return                Conversion of iqNInput to floating point.
 */
#if defined(__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNtoF)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#endif
__STATIC_INLINE float __IQNtoF(int_fast32_t iqNInput, int8_t q_value)
{
    uint_fast16_t ui16Exp;
    uint_fast32_t uiq23Result;
    uint_fast32_t uiq31Input;

    /* Initialize exponent to the offset iq value. */
    ui16Exp = 0x3f80 + ((31 - q_value) * ((uint_fast32_t) 1 << (23 - 16)));

    /* Save the sign of the iqN input to the exponent construction. */
    if (iqNInput < 0) {
        ui16Exp |= 0x8000;
        uiq31Input = -iqNInput;
    } else if (iqNInput == 0) {
        return (0);
    } else {
        uiq31Input = iqNInput;
    }

    /* Scale the iqN input to uiq31 by keeping track of the exponent. */
    while ((uint_fast16_t)(uiq31Input >> 16) < 0x8000) {
        uiq31Input <<= 1;
        ui16Exp -= 0x0080;
    }

    /* Round the uiq31 result and and shift to uiq23 */
    uiq23Result = (uiq31Input + 0x0080) >> 8;

    /* Remove the implied MSB bit of the mantissa. */
    uiq23Result &= ~0x00800000;

    /*
     * Add the constructed exponent and sign bit to the mantissa. We must use
     * an add in the case where rounding would cause the mantissa to overflow.
     * When this happens the mantissa result is two where the MSB is zero and
     * the LSB of the exp is set to 1 instead. Adding one to the exponent is the
     * correct handling for a mantissa of two. It is not required to scale the
     * mantissa since it will always be equal to zero in this scenario.
     */
    uiq23Result += (uint_fast32_t) ui16Exp << 16;

    /* Return the mantissa + exp + sign result as a floating point type. */
    /* Use memcpy to avoid strict-aliasing violation */
    float result;
    memcpy(&result, &uiq23Result, sizeof(float));
    return result;
}

/**
 * @brief Converts input to floating point using IQ30 format.
 *
 * @param a             IQ30 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ30toF(int32_t a)
{
    return __IQNtoF(a, 30);
}
/**
 * @brief Converts input to floating point using IQ29 format.
 *
 * @param a             IQ29 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ29toF(int32_t a)
{
    return __IQNtoF(a, 29);
}
/**
 * @brief Converts input to floating point using IQ28 format.
 *
 * @param a             IQ28 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ28toF(int32_t a)
{
    return __IQNtoF(a, 28);
}
/**
 * @brief Converts input to floating point using IQ27 format.
 *
 * @param a             IQ27 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ27toF(int32_t a)
{
    return __IQNtoF(a, 27);
}
/**
 * @brief Converts input to floating point using IQ26 format.
 *
 * @param a             IQ26 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ26toF(int32_t a)
{
    return __IQNtoF(a, 26);
}
/**
 * @brief Converts input to floating point using IQ25 format.
 *
 * @param a             IQ25 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ25toF(int32_t a)
{
    return __IQNtoF(a, 25);
}
/**
 * @brief Converts input to floating point using IQ24 format.
 *
 * @param a             IQ24 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ24toF(int32_t a)
{
    return __IQNtoF(a, 24);
}
/**
 * @brief Converts input to floating point using IQ23 format.
 *
 * @param a             IQ23 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ23toF(int32_t a)
{
    return __IQNtoF(a, 23);
}
/**
 * @brief Converts input to floating point using IQ22 format.
 *
 * @param a             IQ22 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ22toF(int32_t a)
{
    return __IQNtoF(a, 22);
}
/**
 * @brief Converts input to floating point using IQ21 format.
 *
 * @param a             IQ21 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ21toF(int32_t a)
{
    return __IQNtoF(a, 21);
}
/**
 * @brief Converts input to floating point using IQ20 format.
 *
 * @param a             IQ20 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ20toF(int32_t a)
{
    return __IQNtoF(a, 20);
}
/**
 * @brief Converts input to floating point using IQ19 format.
 *
 * @param a             IQ19 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ19toF(int32_t a)
{
    return __IQNtoF(a, 19);
}
/**
 * @brief Converts input to floating point using IQ18 format.
 *
 * @param a             IQ18 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ18toF(int32_t a)
{
    return __IQNtoF(a, 18);
}
/**
 * @brief Converts input to floating point using IQ17 format.
 *
 * @param a             IQ17 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ17toF(int32_t a)
{
    return __IQNtoF(a, 17);
}
/**
 * @brief Converts input to floating point using IQ16 format.
 *
 * @param a             IQ16 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ16toF(int32_t a)
{
    return __IQNtoF(a, 16);
}
/**
 * @brief Converts input to floating point using IQ15 format.
 *
 * @param a             IQ15 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ15toF(int32_t a)
{
    return __IQNtoF(a, 15);
}
/**
 * @brief Converts input to floating point using IQ14 format.
 *
 * @param a             IQ14 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ14toF(int32_t a)
{
    return __IQNtoF(a, 14);
}
/**
 * @brief Converts input to floating point using IQ13 format.
 *
 * @param a             IQ13 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ13toF(int32_t a)
{
    return __IQNtoF(a, 13);
}
/**
 * @brief Converts input to floating point using IQ12 format.
 *
 * @param a             IQ12 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ12toF(int32_t a)
{
    return __IQNtoF(a, 12);
}
/**
 * @brief Converts input to floating point using IQ11 format.
 *
 * @param a             IQ11 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ11toF(int32_t a)
{
    return __IQNtoF(a, 11);
}
/**
 * @brief Converts input to floating point using IQ10 format.
 *
 * @param a             IQ10 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ10toF(int32_t a)
{
    return __IQNtoF(a, 10);
}
/**
 * @brief Converts input to floating point using IQ9 format.
 *
 * @param a             IQ9 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ9toF(int32_t a)
{
    return __IQNtoF(a, 9);
}
/**
 * @brief Converts input to floating point using IQ8 format.
 *
 * @param a             IQ8 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ8toF(int32_t a)
{
    return __IQNtoF(a, 8);
}
/**
 * @brief Converts input to floating point using IQ7 format.
 *
 * @param a             IQ7 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ7toF(int32_t a)
{
    return __IQNtoF(a, 7);
}
/**
 * @brief Converts input to floating point using IQ6 format.
 *
 * @param a             IQ6 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ6toF(int32_t a)
{
    return __IQNtoF(a, 6);
}
/**
 * @brief Converts input to floating point using IQ5 format.
 *
 * @param a             IQ5 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ5toF(int32_t a)
{
    return __IQNtoF(a, 5);
}
/**
 * @brief Converts input to floating point using IQ4 format.
 *
 * @param a             IQ4 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ4toF(int32_t a)
{
    return __IQNtoF(a, 4);
}
/**
 * @brief Converts input to floating point using IQ3 format.
 *
 * @param a             IQ3 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ3toF(int32_t a)
{
    return __IQNtoF(a, 3);
}
/**
 * @brief Converts input to floating point using IQ2 format.
 *
 * @param a             IQ2 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ2toF(int32_t a)
{
    return __IQNtoF(a, 2);
}
/**
 * @brief Converts input to floating point using IQ1 format.
 *
 * @param a             IQ1 type value to be converted.
 *
 * @return              Conversion of input to floating point.
 */
float _IQ1toF(int32_t a)
{
    return __IQNtoF(a, 1);
}
