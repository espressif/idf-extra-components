/*!****************************************************************************
 *  @file       _atoIQN.c
 *  @brief      Functions to convert a string to an IQN number
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"

/**
 * @brief Converts string to an IQN number.
 *
 * @param string          Pointer to the string to be converted.
 * @param q_value         IQ format.
 *
 * @return                Conversion of string to IQN type.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__atoIQN)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
__STATIC_INLINE int_fast32_t __atoIQN(const char *string, int_fast32_t q_value)
{
    uint8_t sgn;
    uint_fast16_t ui16IntState;
    uint_fast16_t ui16MPYState;
    uint_fast32_t iqNResult;
    uint_fast32_t uiq0Integer = 0;
    uint_fast32_t uiq31Fractional = 0;
    uint_fast32_t max_int = 0x7fffffff >> q_value;

    /* Check for sign */
    if (*string == '-') {
        string++;
        sgn = 1;
    } else {
        sgn = 0;
    }

    /* Setup the device multiplier. */
    __mpy_start(&ui16IntState, &ui16MPYState);

    /* Process integer portion of string starting from first character. */
    while ((*string != '.') && (*string != 0)) {
        /* Check for invalid character */
        if (*string < '0' || *string > '9') {
            return 0;
        }

        /* Check that multiplying by 10 won't cause overflow */
        if (uiq0Integer > iq31_pointOne) {
            if (sgn) {
                return 0x80000000;
            } else {
                return 0x7fffffff;
            }
        }

        /* Multiply by 10 */
        uiq0Integer = __mpy_l(uiq0Integer, 10);

        /* Add next integer to result */
        uiq0Integer += *string++ - '0';

        /* Check to see if integer portion has overflowed */
        if (uiq0Integer > max_int) {
            if (sgn) {
                return 0x80000000;
            } else {
                return 0x7fffffff;
            }
        }
    }

    /* Restore multiplier context. */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* Check if previous loop ended with null character and return. */
    if (*string == 0) {
        /* Shift integer portion up */
        iqNResult = uiq0Integer << q_value;

        /* Return the result. */
        return iqNResult;
    }

    /* Increment to the null terminating character and back up one character. */
    while (*++string);
    string--;

    /* Setup the device multiplier. */
    __mpy_start(&ui16IntState, &ui16MPYState);

    /* Process fractional portion of string starting at the last character. */
    while (*string != '.') {
        /* Check for invalid character */
        if (*string < '0' || *string > '9') {
            return 0;
        }

        /* Multiply fractional piece by 0.1 to setup the next decimal place. */
        __mpy_set_frac();
        uiq31Fractional = __mpyf_ul(uiq31Fractional, iq31_pointOne);
        __mpy_clear_ctl0();

        /*
         * Add the current decimal place converted to iq31 to the sum and
         * decrement pointer.
         */
        uiq31Fractional += __mpy_ul((*string - '0'), iq31_pointOne);
        string--;
    }

    /* Restore multiplier context. */
    __mpy_stop(&ui16IntState, &ui16MPYState);

    /* Shift integer portion up */
    uiq0Integer <<= q_value;

    /* Shift fractional portion to match Q type with rounding. */
    if (q_value != 31) {
        uiq31Fractional += ((uint_fast32_t)1 << (30 - q_value));
    }
    uiq31Fractional >>= (31 - q_value);

    /* Construct the iqN result. */
    iqNResult = uiq0Integer + uiq31Fractional;
    if (sgn) {
        iqNResult = -iqNResult;
    }

    /* Finished, return the result */
    return iqNResult;
}

/**
 * @brief Converts string to IQ31 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ31 type.
 */
int32_t _atoIQ31(const char *string)
{
    return __atoIQN(string, 31);
}
/**
 * @brief Converts string to IQ30 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ30 type.
 */
int32_t _atoIQ30(const char *string)
{
    return __atoIQN(string, 30);
}
/**
 * @brief Converts string to IQ29 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ29 type.
 */
int32_t _atoIQ29(const char *string)
{
    return __atoIQN(string, 29);
}
/**
 * @brief Converts string to IQ28 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ28 type.
 */
int32_t _atoIQ28(const char *string)
{
    return __atoIQN(string, 28);
}
/**
 * @brief Converts string to IQ27 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ27 type.
 */
int32_t _atoIQ27(const char *string)
{
    return __atoIQN(string, 27);
}
/**
 * @brief Converts string to IQ26 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ26 type.
 */
int32_t _atoIQ26(const char *string)
{
    return __atoIQN(string, 26);
}
/**
 * @brief Converts string to IQ25 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ25 type.
 */
int32_t _atoIQ25(const char *string)
{
    return __atoIQN(string, 25);
}
/**
 * @brief Converts string to IQ24 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ24 type.
 */
int32_t _atoIQ24(const char *string)
{
    return __atoIQN(string, 24);
}
/**
 * @brief Converts string to IQ23 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ23 type.
 */
int32_t _atoIQ23(const char *string)
{
    return __atoIQN(string, 23);
}
/**
 * @brief Converts string to IQ22 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ22 type.
 */
int32_t _atoIQ22(const char *string)
{
    return __atoIQN(string, 22);
}
/**
 * @brief Converts string to IQ21 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ21 type.
 */
int32_t _atoIQ21(const char *string)
{
    return __atoIQN(string, 21);
}
/**
 * @brief Converts string to IQ20 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ20 type.
 */
int32_t _atoIQ20(const char *string)
{
    return __atoIQN(string, 20);
}
/**
 * @brief Converts string to IQ19 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ19 type.
 */
int32_t _atoIQ19(const char *string)
{
    return __atoIQN(string, 19);
}
/**
 * @brief Converts string to IQ18 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ18 type.
 */
int32_t _atoIQ18(const char *string)
{
    return __atoIQN(string, 18);
}
/**
 * @brief Converts string to IQ17 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ17 type.
 */
int32_t _atoIQ17(const char *string)
{
    return __atoIQN(string, 17);
}
/**
 * @brief Converts string to IQ16 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ16 type.
 */
int32_t _atoIQ16(const char *string)
{
    return __atoIQN(string, 16);
}
/**
 * @brief Converts string to IQ15 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ15 type.
 */
int32_t _atoIQ15(const char *string)
{
    return __atoIQN(string, 15);
}
/**
 * @brief Converts string to IQ14 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ14 type.
 */
int32_t _atoIQ14(const char *string)
{
    return __atoIQN(string, 14);
}
/**
 * @brief Converts string to IQ13 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ13 type.
 */
int32_t _atoIQ13(const char *string)
{
    return __atoIQN(string, 13);
}
/**
 * @brief Converts string to IQ12 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ12 type.
 */
int32_t _atoIQ12(const char *string)
{
    return __atoIQN(string, 12);
}
/**
 * @brief Converts string to IQ11 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ11 type.
 */
int32_t _atoIQ11(const char *string)
{
    return __atoIQN(string, 11);
}
/**
 * @brief Converts string to IQ10 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ10 type.
 */
int32_t _atoIQ10(const char *string)
{
    return __atoIQN(string, 10);
}
/**
 * @brief Converts string to IQ9 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ9 type.
 */
int32_t _atoIQ9(const char *string)
{
    return __atoIQN(string, 9);
}
/**
 * @brief Converts string to IQ8 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ8 type.
 */
int32_t _atoIQ8(const char *string)
{
    return __atoIQN(string, 8);
}
/**
 * @brief Converts string to IQ7 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ7 type.
 */
int32_t _atoIQ7(const char *string)
{
    return __atoIQN(string, 7);
}
/**
 * @brief Converts string to IQ6 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ6 type.
 */
int32_t _atoIQ6(const char *string)
{
    return __atoIQN(string, 6);
}
/**
 * @brief Converts string to IQ5 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ5 type.
 */
int32_t _atoIQ5(const char *string)
{
    return __atoIQN(string, 5);
}
/**
 * @brief Converts string to IQ4 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ4 type.
 */
int32_t _atoIQ4(const char *string)
{
    return __atoIQN(string, 4);
}
/**
 * @brief Converts string to IQ3 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ3 type.
 */
int32_t _atoIQ3(const char *string)
{
    return __atoIQN(string, 3);
}
/**
 * @brief Converts string to IQ2 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ2 type.
 */
int32_t _atoIQ2(const char *string)
{
    return __atoIQN(string, 2);
}
/**
 * @brief Converts string to IQ1 number.
 *
 * @param string        Pointer to the string to be converted.
 *
 * @return              Conversion of string to IQ1 type.
 */
int32_t _atoIQ1(const char *string)
{
    return __atoIQN(string, 1);
}
