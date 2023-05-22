/*!****************************************************************************
 *  @file       _IQNtoa.c
 *  @brief      Functions to convert an IQ number to a string.
 *
 *  <hr>
 ******************************************************************************/

#include <stdint.h>

#include "../support/support.h"

/*
 * Convert IQN values to string.
 */
/**
 * @brief Convert an IQ number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQN type input.
 * @param q_value         IQ format.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNtoa)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
int_fast16_t __IQNtoa(char *string, const char *format, int_fast32_t iqNInput, int_fast16_t q_value)
{
    char *pcBuf;                    // buffer pointer
    int_fast16_t count;                  // conversion character counter
    uint_fast16_t ui16IntWidth;          // integer format width
    uint_fast16_t ui16FracWidth;         // fractional format width
    uint_fast16_t ui16IntState;          // save interrupt state
    uint_fast16_t ui16MPYState;          // save multiplier state
    uint_fast32_t uiqNInput;             // unsigned input
    uint_fast32_t uiq32Fractional;       // working variable
    uint_fast32_t ui32Integer;           // working variable
    uint_fast32_t ui32IntTemp;           // temp variable
    uint_fast32_t ui32IntegerTenth;      // uval scaled by 10
    uint_fast64_t uiiq32FractionalTen;   // fractional input scaled by 10

    /* Check that 1st character is a '%' character. */
    if (*format++ != '%') {
        /* Error: format not preceded with '%' character. */
        return (2);
    }

    /* Check that an integer is provided and extract the integer width. */
    if (*format < '0' || *format > '9') {
        /* Error: integer width is non integer. */
        return (2);
    }

    /* Initialize local variables and extract the integer width. */
    count = 0;
    ui16IntWidth = 0;
    while (*format >= '0' && *format <= '9') {
        __mpy_start(&ui16IntState, &ui16MPYState);
        ui16IntWidth = __mpy_uw(ui16IntWidth, 10);
        __mpy_stop(&ui16IntState, &ui16MPYState);
        ui16IntWidth = ui16IntWidth + (*format++ - '0');

        /* If we don't find the '.' after 2 counts, something is wrong. */
        if (++count > 2) {
            /* Error: integer width field too many characters */
            return (2);
        }
    }

    /* Check integer width for errors. */
    if (ui16IntWidth > 11) {
        /* Error: integer width too large */
        return (2);
    }

    /* Check the next character for '.' and increment over. */
    if (*format++ != '.') {
        /* Error: format missing the '.' character. */
        return (2);
    }

    /* Re-initialize the local variables and extract the fractional width. */
    count = 0;
    ui16FracWidth = 0;
    while (*format >= '0' && *format <= '9') {
        __mpy_start(&ui16IntState, &ui16MPYState);
        ui16FracWidth = __mpy_uw(ui16FracWidth, 10);
        __mpy_stop(&ui16IntState, &ui16MPYState);
        ui16FracWidth = ui16FracWidth + (*format++ - '0');

        /* If we don't exit after 2 counts, something is wrong. */
        if (++count > 2) {
            /* Error: fractional width field too many characters */
            return (2);
        }
    }

    /* Check the next character for 'f' or 'F'. */
    if (*format != 'f' && *format != 'F') {
        /* Error: format missing the format specifying character. */
        return (2);
    }

    /* Check that the next character is the NULL string terminator. */
    if (*++format != 0) {
        /* Error: missing null terminator. */
        return (2);
    }

    /*
     * Begin constructing the string.
     */
    pcBuf = string;

    /* Check for negative value. */
    if (iqNInput < 0) {
        iqNInput = -iqNInput;
        *pcBuf++ = '-';
    }
    uiqNInput = (uint_fast32_t)iqNInput;

    /* Construct the integer string in reverse. */
    pcBuf += ui16IntWidth;
    ui32Integer = uiqNInput >> q_value;

    for (count = ui16IntWidth; count > 0; count--) {
        /* Integer position n = ui32Integer - floor(ui32Integer/(10^n))*(10^n) */
        __mpyf_start(&ui16IntState, &ui16MPYState);
        ui32IntegerTenth = __mpyf_l(ui32Integer, iq31_oneTenth);
        __mpy_clear_ctl0();
        ui32IntTemp = __mpy_ul(ui32IntegerTenth, 10);
        /* Handle any possible rounding. */
        if (ui32IntTemp > ui32Integer) {
            ui32IntTemp -= 10;
            ui32IntegerTenth -= 1;
        }
        ui32Integer -= ui32IntTemp;
        __mpy_stop(&ui16IntState, &ui16MPYState);

        *--pcBuf = ui32Integer + '0';
        ui32Integer = ui32IntegerTenth;
    }

    /* Check if there is any remaining input. */
    if (ui32Integer) {
        /* Error: integer format too small. */
        return (1);
    }

    /* Construct the fractional string if specified using unsigned iq32. */
    pcBuf += ui16IntWidth;
    uiq32Fractional = uiqNInput << (32 - q_value);

    if (ui16FracWidth > 0) {
        *pcBuf++ = '.';

        while (ui16FracWidth--) {
            __mpy_start(&ui16IntState, &ui16MPYState);
            uiiq32FractionalTen = __mpyx_u(uiq32Fractional, 10);
            __mpy_stop(&ui16IntState, &ui16MPYState);

            uiq32Fractional = (uint_fast32_t)uiiq32FractionalTen;
            *pcBuf++ = (uint8_t)(uiiq32FractionalTen >> 32) + '0';
        }
    }

    /* Add null terminating character and return. */
    *pcBuf = 0;
    return (0);
}

/**
 * @brief Convert an IQ31 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ31 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ31toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 31);
}
/**
 * @brief Convert an IQ30 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ30 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ30toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 30);
}
/**
 * @brief Convert an IQ29 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ29 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ29toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 29);
}
/**
 * @brief Convert an IQ28 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ28 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ28toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 28);
}
/**
 * @brief Convert an IQ27 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ27 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ27toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 27);
}
/**
 * @brief Convert an IQ26 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ26 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ26toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 26);
}
/**
 * @brief Convert an IQ25 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ25 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ25toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 25);
}
/**
 * @brief Convert an IQ24 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ24 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ24toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 24);
}
/**
 * @brief Convert an IQ23 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ23 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ23toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 23);
}
/**
 * @brief Convert an IQ22 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ22 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ22toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 22);
}
/**
 * @brief Convert an IQ21 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ21 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ21toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 21);
}
/**
 * @brief Convert an IQ20 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ20 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ20toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 20);
}
/**
 * @brief Convert an IQ19 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ19 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ19toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 19);
}
/**
 * @brief Convert an IQ18 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ18 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ18toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 18);
}
/**
 * @brief Convert an IQ17 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ17 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ17toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 17);
}
/**
 * @brief Convert an IQ16 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ16 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ16toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 16);
}
/**
 * @brief Convert an IQ15 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ15 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ15toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 15);
}
/**
 * @brief Convert an IQ14 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ14 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ14toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 14);
}
/**
 * @brief Convert an IQ13 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ13 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ13toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 13);
}
/**
 * @brief Convert an IQ12 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ12 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ12toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 12);
}
/**
 * @brief Convert an IQ11 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ11 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ11toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 11);
}
/**
 * @brief Convert an IQ10 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ10 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ10toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 10);
}
/**
 * @brief Convert an IQ9 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ9 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ9toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 9);
}
/**
 * @brief Convert an IQ8 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ8 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ8toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 8);
}
/**
 * @brief Convert an IQ7 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ7 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ7toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 7);
}
/**
 * @brief Convert an IQ6 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ6 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ6toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 6);
}
/**
 * @brief Convert an IQ5 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ5 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ5toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 5);
}
/**
 * @brief Convert an IQ4 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ4 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ4toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 4);
}
/**
 * @brief Convert an IQ3 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ3 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ3toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 3);
}
/**
 * @brief Convert an IQ2 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ2 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ2toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 2);
}
/**
 * @brief Convert an IQ1 number to a string.
 *
 * @param string          Pointer to the buffer to store the converted IQ number.
 * @param format          The format string specifying how to convert the IQ number.
 * @param iqNInput        IQ1 type input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
int16_t _IQ1toa(char *string, const char *format, int32_t iqNInput)
{
    return __IQNtoa(string, format, iqNInput, 1);
}
