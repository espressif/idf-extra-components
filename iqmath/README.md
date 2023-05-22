# IQMath Library

[![Component Registry](https://components.espressif.com/components/espressif/iqmath/badge.svg)](https://components.espressif.com/components/espressif/iqmath)

The IQmath Library is a collection of highly optimized and high-precision mathematical functions for C programmers to seamlessly port a floating-point algorithm into fixed-point code on ESP chips. These routines are typically used in computationally intensive real-time applications where optimal execution speed, high accuracy and ultra-low energy are critical. By using the IQmath library, it is possible to achieve execution speeds considerably faster and energy consumption considerably lower than equivalent code written using floating-point math.

The IQmath library provides functions for use with 32-bit data types and high accuracy.

## IQmath Data Types

The IQmath library uses a 32-bit fixed-point signed number (i.e. `int32_t`) as its basic data type. The IQ format of this fixed-point number can range from `IQ1` to `IQ30`, where the IQ format number indicates the number of fractional bits. The IQ format value is stored as an integer with an implied scale based on the IQ format and the number of fractional bits. The equation below shows how a IQ format decimal number $x_{iq}$ is stored using an integer value $x_i$ with an implied scale, where $n$ represents the number of fractional bits.

$$
IQ_n(x_{iq}) = x_i * 2^{-n}
$$

For example the IQ24 value of 3.625 is stored as an integer value of 60817408, shown in the equation below.

$$
IQ_{24}(3.625) = 60817408 * 2^{-24}
$$

C typedefs are provided for the various IQ formats, and these IQmath data types should be used in preference to the underlying “int32_t” data type to make it clear which variables are in IQ format.

The following table provides the characteristics of the various IQ formats:

| Type  | Integer Bits | Fractional Bits | Min Range      | Max Range                 | Resolution    |
|-------|--------------|-----------------|----------------|---------------------------|---------------|
| _iq30 | 2            | 30              | -2             | 1.999 999 999             | 0.000 000 001 |
| _iq29 | 3            | 29              | -4             | 3.999 999 998             | 0.000 000 002 |
| _iq28 | 4            | 28              | -8             | 7.999 999 996             | 0.000 000 004 |
| _iq27 | 5            | 27              | -16            | 15.999 999 993            | 0.000 000 007 |
| _iq26 | 6            | 26              | -32            | 31.999 999 985            | 0.000 000 015 |
| _iq25 | 7            | 25              | -64            | 63.999 999 970            | 0.000 000 030 |
| _iq24 | 8            | 24              | -128           | 127.999 999 940           | 0.000 000 060 |
| _iq23 | 9            | 23              | -256           | 255.999 999 881           | 0.000 000 119 |
| _iq22 | 10           | 22              | -512           | 511.999 999 762           | 0.000 000 238 |
| _iq21 | 11           | 21              | -1,024         | 1,023.999 999 523         | 0.000 000 477 |
| _iq20 | 12           | 20              | -2,048         | 2,047.999 999 046         | 0.000 000 954 |
| _iq19 | 13           | 19              | -4,096         | 4,095.999 998 093         | 0.000 001 907 |
| _iq18 | 14           | 18              | -8,192         | 8,191.999 996 185         | 0.000 003 815 |
| _iq17 | 15           | 17              | -16,384        | 16,383.999 992 371        | 0.000 007 629 |
| _iq16 | 16           | 16              | -32,768        | 32,767.999 984 741        | 0.000 015 259 |
| _iq15 | 17           | 15              | -65,536        | 65,535.999 969 483        | 0.000 030 518 |
| _iq14 | 18           | 14              | -131,072       | 131,071.999 938 965       | 0.000 061 035 |
| _iq13 | 19           | 13              | -262,144       | 262,143.999 877 930       | 0.000 122 070 |
| _iq12 | 20           | 12              | -524,288       | 524,287.999 755 859       | 0.000 244 141 |
| _iq11 | 21           | 11              | -1,048,576     | 1,048,575.999 511 720     | 0.000 488 281 |
| _iq10 | 22           | 10              | -2,097,152     | 2,097,151.999 023 440     | 0.000 976 563 |
| _iq9  | 23           | 9               | -4,194,304     | 4,194,303.998 046 880     | 0.001 953 125 |
| _iq8  | 24           | 8               | -8,388,608     | 8,388,607.996 093 750     | 0.003 906 250 |
| _iq7  | 25           | 7               | -16,777,216    | 16,777,215.992 187 500    | 0.007 812 500 |
| _iq6  | 26           | 6               | -33,554,432    | 33,554,431.984 375 000    | 0.015 625 000 |
| _iq5  | 27           | 5               | -67,108,864    | 67,108,863.968 750 000    | 0.031 250 000 |
| _iq4  | 28           | 4               | -134,217,728   | 134,217,727.937 500 000   | 0.062 500 000 |
| _iq3  | 29           | 3               | -268,435,456   | 268,435,455.875 000 000   | 0.125 000 000 |
| _iq2  | 30           | 2               | -536,870,912   | 536,870,911.750 000 000   | 0.250 000 000 |
| _iq1  | 31           | 1               | -1,073,741,824 | 1,073,741,823.500 000 000 | 0.500 000 000 |

## API Guide

IQmath includes five types of routines:

* **Format conversion functions**: methods to convert numbers to and from the various formats.
* **Arithmetic functions**: methods to perform basic arithmetic (addition, subtraction, multiplication, division).
* **Trigonometric functions**: methods to perform trigonometric functions (sin, cos, atan, and so on).
* **Mathematical functions**: methods to perform advanced arithmetic (square root, ex , and so on).
* **Miscellaneous**: miscellaneous methods (saturation and absolute value).
