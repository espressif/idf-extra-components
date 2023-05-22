/*!****************************************************************************
 *  @file       IQmathLib.h
 *  @brief      Library of IQMath operations.
 *
 *  <hr>
 ******************************************************************************/
#ifndef __IQMATHLIB_H__
#define __IQMATHLIB_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
/*!
 * @brief The IQ format to be used when the IQ format is not explicitly specified
 * (such as _IQcos instead of _IQ16cos).  This value must be between 1 and 30,
 * inclusive.
 */
//*****************************************************************************
#ifndef GLOBAL_IQ
#define GLOBAL_IQ                24
#endif

//*****************************************************************************
//
// Include some standard headers.
//
//*****************************************************************************
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
//*****************************************************************************
//
// Various Useful Constant Definitions:
//
//*****************************************************************************
#define Q30                     30
#define Q29                     29
#define Q28                     28
#define Q27                     27
#define Q26                     26
#define Q25                     25
#define Q24                     24
#define Q23                     23
#define Q22                     22
#define Q21                     21
#define Q20                     20
#define Q19                     19
#define Q18                     18
#define Q17                     17
#define Q16                     16
#define Q15                     15
#define Q14                     14
#define Q13                     13
#define Q12                     12
#define Q11                     11
#define Q10                     10
#define Q9                      9
#define Q8                      8
#define Q7                      7
#define Q6                      6
#define Q5                      5
#define Q4                      4
#define Q3                      3
#define Q2                      2
#define Q1                      1
#ifndef QG
#define QG                      GLOBAL_IQ
#endif

#define MAX_IQ_POS              LONG_MAX
#define MAX_IQ_NEG              LONG_MIN
#define MIN_IQ_POS              1
#define MIN_IQ_NEG              -1

//*****************************************************************************
//
// The types for the various IQ formats.
//
//*****************************************************************************
typedef int32_t _iq30;
typedef int32_t _iq29;
typedef int32_t _iq28;
typedef int32_t _iq27;
typedef int32_t _iq26;
typedef int32_t _iq25;
typedef int32_t _iq24;
typedef int32_t _iq23;
typedef int32_t _iq22;
typedef int32_t _iq21;
typedef int32_t _iq20;
typedef int32_t _iq19;
typedef int32_t _iq18;
typedef int32_t _iq17;
typedef int32_t _iq16;
typedef int32_t _iq15;
typedef int32_t _iq14;
typedef int32_t _iq13;
typedef int32_t _iq12;
typedef int32_t _iq11;
typedef int32_t _iq10;
typedef int32_t _iq9;
typedef int32_t _iq8;
typedef int32_t _iq7;
typedef int32_t _iq6;
typedef int32_t _iq5;
typedef int32_t _iq4;
typedef int32_t _iq3;
typedef int32_t _iq2;
typedef int32_t _iq1;
typedef int32_t _iq;

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

//*****************************************************************************
//
// Simple multiplies or divides, which are accomplished with simple shifts.
//
//*****************************************************************************

/**
 * @brief Multiplies an IQ value by 2.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of multiplication.
 */
#define _IQmpy2(A)              ((A) << 1)
/**
 * @brief Multiplies an IQ value by 4.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of multiplication.
 */
#define _IQmpy4(A)              ((A) << 2)
/**
 * @brief Multiplies an IQ value by 8.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of multiplication.
 */
#define _IQmpy8(A)              ((A) << 3)
/**
 * @brief Multiplies an IQ value by 16.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of multiplication.
 */
#define _IQmpy16(A)             ((A) << 4)
/**
 * @brief Multiplies an IQ value by 32.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of multiplication.
 */
#define _IQmpy32(A)             ((A) << 5)
/**
 * @brief Multiplies an IQ value by 64.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of multiplication.
 */
#define _IQmpy64(A)             ((A) << 6)
/**
 * @brief Divides an IQ value by 2.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of division.
 */
#define _IQdiv2(A)              ((A) >> 1)
/**
 * @brief Divides an IQ value by 4.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of division.
 */
#define _IQdiv4(A)              ((A) >> 2)
/**
 * @brief Divides an IQ value by 8.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of division.
 */
#define _IQdiv8(A)              ((A) >> 3)
/**
 * @brief Divides an IQ value by 16.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of division.
 */
#define _IQdiv16(A)             ((A) >> 4)
/**
 * @brief Divides an IQ value by 32.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of division.
 */
#define _IQdiv32(A)             ((A) >> 5)
/**
 * @brief Divides an IQ value by 64.
 *
 * @param A               IQ type input.
 *
 * @return                IQ type result of division.
 */
#define _IQdiv64(A)             ((A) >> 6)

//*****************************************************************************
//
// Convert a value into an IQ number.
//
//*****************************************************************************
/**
 * @brief Converts a value into an IQ30 number.
 *
 * @param A               Number input.
 *
 * @return                IQ30 type result.
 */
#define _IQ30(A)                ((_iq30)((A) * ((_iq30)1 << 30)))
/**
 * @brief Converts a value into an IQ29 number.
 *
 * @param A               Number input.
 *
 * @return                IQ29 type result.
 */
#define _IQ29(A)                ((_iq29)((A) * ((_iq29)1 << 29)))
/**
 * @brief Converts a value into an IQ28 number.
 *
 * @param A               Number input.
 *
 * @return                IQ28 type result.
 */
#define _IQ28(A)                ((_iq28)((A) * ((_iq28)1 << 28)))
/**
 * @brief Converts a value into an IQ27 number.
 *
 * @param A               Number input.
 *
 * @return                IQ27 type result.
 */
#define _IQ27(A)                ((_iq27)((A) * ((_iq27)1 << 27)))
/**
 * @brief Converts a value into an IQ26 number.
 *
 * @param A               Number input.
 *
 * @return                IQ26 type result.
 */
#define _IQ26(A)                ((_iq26)((A) * ((_iq26)1 << 26)))
/**
 * @brief Converts a value into an IQ25 number.
 *
 * @param A               Number input.
 *
 * @return                IQ25 type result.
 */
#define _IQ25(A)                ((_iq25)((A) * ((_iq25)1 << 25)))
/**
 * @brief Converts a value into an IQ24 number.
 *
 * @param A               Number input.
 *
 * @return                IQ24 type result.
 */
#define _IQ24(A)                ((_iq24)((A) * ((_iq24)1 << 24)))
/**
 * @brief Converts a value into an IQ23 number.
 *
 * @param A               Number input.
 *
 * @return                IQ23 type result.
 */
#define _IQ23(A)                ((_iq23)((A) * ((_iq23)1 << 23)))
/**
 * @brief Converts a value into an IQ22 number.
 *
 * @param A               Number input.
 *
 * @return                IQ22 type result.
 */
#define _IQ22(A)                ((_iq22)((A) * ((_iq22)1 << 22)))
/**
 * @brief Converts a value into an IQ21 number.
 *
 * @param A               Number input.
 *
 * @return                IQ21 type result.
 */
#define _IQ21(A)                ((_iq21)((A) * ((_iq21)1 << 21)))
/**
 * @brief Converts a value into an IQ20 number.
 *
 * @param A               Number input.
 *
 * @return                IQ20 type result.
 */
#define _IQ20(A)                ((_iq20)((A) * ((_iq20)1 << 20)))
/**
 * @brief Converts a value into an IQ19 number.
 *
 * @param A               Number input.
 *
 * @return                IQ19 type result.
 */
#define _IQ19(A)                ((_iq19)((A) * ((_iq19)1 << 19)))
/**
 * @brief Converts a value into an IQ18 number.
 *
 * @param A               Number input.
 *
 * @return                IQ18 type result.
 */
#define _IQ18(A)                ((_iq18)((A) * ((_iq18)1 << 18)))
/**
 * @brief Converts a value into an IQ17 number.
 *
 * @param A               Number input.
 *
 * @return                IQ17 type result.
 */
#define _IQ17(A)                ((_iq17)((A) * ((_iq17)1 << 17)))
/**
 * @brief Converts a value into an IQ16 number.
 *
 * @param A               Number input.
 *
 * @return                IQ16 type result.
 */
#define _IQ16(A)                ((_iq16)((A) * ((_iq16)1 << 16)))
/**
 * @brief Converts a value into an IQ15 number.
 *
 * @param A               Number input.
 *
 * @return                IQ15 type result.
 */
#define _IQ15(A)                ((_iq15)((A) * ((_iq15)1 << 15)))
/**
 * @brief Converts a value into an IQ14 number.
 *
 * @param A               Number input.
 *
 * @return                IQ14 type result.
 */
#define _IQ14(A)                ((_iq14)((A) * ((_iq14)1 << 14)))
/**
 * @brief Converts a value into an IQ13 number.
 *
 * @param A               Number input.
 *
 * @return                IQ13 type result.
 */
#define _IQ13(A)                ((_iq13)((A) * ((_iq13)1 << 13)))
/**
 * @brief Converts a value into an IQ12 number.
 *
 * @param A               Number input.
 *
 * @return                IQ12 type result.
 */
#define _IQ12(A)                ((_iq12)((A) * ((_iq12)1 << 12)))
/**
 * @brief Converts a value into an IQ11 number.
 *
 * @param A               Number input.
 *
 * @return                IQ11 type result.
 */
#define _IQ11(A)                ((_iq11)((A) * ((_iq11)1 << 11)))
/**
 * @brief Converts a value into an IQ10 number.
 *
 * @param A               Number input.
 *
 * @return                IQ10 type result.
 */
#define _IQ10(A)                ((_iq10)((A) * ((_iq10)1 << 10)))
/**
 * @brief Converts a value into an IQ9 number.
 *
 * @param A               Number input.
 *
 * @return                IQ9 type result.
 */
#define _IQ9(A)                 ((_iq9)((A) * ((_iq9)1 << 9)))
/**
 * @brief Converts a value into an IQ8 number.
 *
 * @param A               Number input.
 *
 * @return                IQ8 type result.
 */
#define _IQ8(A)                 ((_iq8)((A) * ((_iq8)1 << 8)))
/**
 * @brief Converts a value into an IQ7 number.
 *
 * @param A               Number input.
 *
 * @return                IQ7 type result.
 */
#define _IQ7(A)                 ((_iq7)((A) * ((_iq7)1 << 7)))
/**
 * @brief Converts a value into an IQ6 number.
 *
 * @param A               Number input.
 *
 * @return                IQ6 type result.
 */
#define _IQ6(A)                 ((_iq6)((A) * ((_iq6)1 << 6)))
/**
 * @brief Converts a value into an IQ5 number.
 *
 * @param A               Number input.
 *
 * @return                IQ5 type result.
 */
#define _IQ5(A)                 ((_iq5)((A) * ((_iq5)1 << 5)))
/**
 * @brief Converts a value into an IQ4 number.
 *
 * @param A               Number input.
 *
 * @return                IQ4 type result.
 */
#define _IQ4(A)                 ((_iq4)((A) * ((_iq4)1 << 4)))
/**
 * @brief Converts a value into an IQ3 number.
 *
 * @param A               Number input.
 *
 * @return                IQ3 type result.
 */
#define _IQ3(A)                 ((_iq3)((A) * ((_iq3)1 << 3)))
/**
 * @brief Converts a value into an IQ2 number.
 *
 * @param A               Number input.
 *
 * @return                IQ2 type result.
 */
#define _IQ2(A)                 ((_iq2)((A) * ((_iq2)1 << 2)))
/**
 * @brief Converts a value into an IQ1 number.
 *
 * @param A               Number input.
 *
 * @return                IQ1 type result.
 */
#define _IQ1(A)                 ((_iq1)((A) * ((_iq1)1 << 1)))

/**
 * @brief Converts a value into an the global IQ  format.
 *
 * @param A               Number input.
 *
 * @return                Global IQ type result.
 */
#if GLOBAL_IQ == 30
#define _IQ(A)                  _IQ30(A)
#endif
#if GLOBAL_IQ == 29
#define _IQ(A)                  _IQ29(A)
#endif
#if GLOBAL_IQ == 28
#define _IQ(A)                  _IQ28(A)
#endif
#if GLOBAL_IQ == 27
#define _IQ(A)                  _IQ27(A)
#endif
#if GLOBAL_IQ == 26
#define _IQ(A)                  _IQ26(A)
#endif
#if GLOBAL_IQ == 25
#define _IQ(A)                  _IQ25(A)
#endif
#if GLOBAL_IQ == 24
#define _IQ(A)                  _IQ24(A)
#endif
#if GLOBAL_IQ == 23
#define _IQ(A)                  _IQ23(A)
#endif
#if GLOBAL_IQ == 22
#define _IQ(A)                  _IQ22(A)
#endif
#if GLOBAL_IQ == 21
#define _IQ(A)                  _IQ21(A)
#endif
#if GLOBAL_IQ == 20
#define _IQ(A)                  _IQ20(A)
#endif
#if GLOBAL_IQ == 19
#define _IQ(A)                  _IQ19(A)
#endif
#if GLOBAL_IQ == 18
#define _IQ(A)                  _IQ18(A)
#endif
#if GLOBAL_IQ == 17
#define _IQ(A)                  _IQ17(A)
#endif
#if GLOBAL_IQ == 16
#define _IQ(A)                  _IQ16(A)
#endif
#if GLOBAL_IQ == 15
#define _IQ(A)                  _IQ15(A)
#endif
#if GLOBAL_IQ == 14
#define _IQ(A)                  _IQ14(A)
#endif
#if GLOBAL_IQ == 13
#define _IQ(A)                  _IQ13(A)
#endif
#if GLOBAL_IQ == 12
#define _IQ(A)                  _IQ12(A)
#endif
#if GLOBAL_IQ == 11
#define _IQ(A)                  _IQ11(A)
#endif
#if GLOBAL_IQ == 10
#define _IQ(A)                  _IQ10(A)
#endif
#if GLOBAL_IQ == 9
#define _IQ(A)                  _IQ9(A)
#endif
#if GLOBAL_IQ == 8
#define _IQ(A)                  _IQ8(A)
#endif
#if GLOBAL_IQ == 7
#define _IQ(A)                  _IQ7(A)
#endif
#if GLOBAL_IQ == 6
#define _IQ(A)                  _IQ6(A)
#endif
#if GLOBAL_IQ == 5
#define _IQ(A)                  _IQ5(A)
#endif
#if GLOBAL_IQ == 4
#define _IQ(A)                  _IQ4(A)
#endif
#if GLOBAL_IQ == 3
#define _IQ(A)                  _IQ3(A)
#endif
#if GLOBAL_IQ == 2
#define _IQ(A)                  _IQ2(A)
#endif
#if GLOBAL_IQ == 1
#define _IQ(A)                  _IQ1(A)
#endif

//*****************************************************************************
//
// Convert an IQ number to a floating point value.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern float _IQ30toF(_iq30 A);
extern float _IQ29toF(_iq29 A);
extern float _IQ28toF(_iq28 A);
extern float _IQ27toF(_iq27 A);
extern float _IQ26toF(_iq26 A);
extern float _IQ25toF(_iq25 A);
extern float _IQ24toF(_iq24 A);
extern float _IQ23toF(_iq23 A);
extern float _IQ22toF(_iq22 A);
extern float _IQ21toF(_iq21 A);
extern float _IQ20toF(_iq20 A);
extern float _IQ19toF(_iq19 A);
extern float _IQ18toF(_iq18 A);
extern float _IQ17toF(_iq17 A);
extern float _IQ16toF(_iq16 A);
extern float _IQ15toF(_iq15 A);
extern float _IQ14toF(_iq14 A);
extern float _IQ13toF(_iq13 A);
extern float _IQ12toF(_iq12 A);
extern float _IQ11toF(_iq11 A);
extern float _IQ10toF(_iq10 A);
extern float _IQ9toF(_iq9 A);
extern float _IQ8toF(_iq8 A);
extern float _IQ7toF(_iq7 A);
extern float _IQ6toF(_iq6 A);
extern float _IQ5toF(_iq5 A);
extern float _IQ4toF(_iq4 A);
extern float _IQ3toF(_iq3 A);
extern float _IQ2toF(_iq2 A);
extern float _IQ1toF(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Convert a global IQ format number to a floating point value.
 *
 * @param A               Number input.
 *
 * @return                Global IQ type result.
 */
#if GLOBAL_IQ == 30
#define _IQtoF(A)               _IQ30toF(A)
#endif
#if GLOBAL_IQ == 29
#define _IQtoF(A)               _IQ29toF(A)
#endif
#if GLOBAL_IQ == 28
#define _IQtoF(A)               _IQ28toF(A)
#endif
#if GLOBAL_IQ == 27
#define _IQtoF(A)               _IQ27toF(A)
#endif
#if GLOBAL_IQ == 26
#define _IQtoF(A)               _IQ26toF(A)
#endif
#if GLOBAL_IQ == 25
#define _IQtoF(A)               _IQ25toF(A)
#endif
#if GLOBAL_IQ == 24
#define _IQtoF(A)               _IQ24toF(A)
#endif
#if GLOBAL_IQ == 23
#define _IQtoF(A)               _IQ23toF(A)
#endif
#if GLOBAL_IQ == 22
#define _IQtoF(A)               _IQ22toF(A)
#endif
#if GLOBAL_IQ == 21
#define _IQtoF(A)               _IQ21toF(A)
#endif
#if GLOBAL_IQ == 20
#define _IQtoF(A)               _IQ20toF(A)
#endif
#if GLOBAL_IQ == 19
#define _IQtoF(A)               _IQ19toF(A)
#endif
#if GLOBAL_IQ == 18
#define _IQtoF(A)               _IQ18toF(A)
#endif
#if GLOBAL_IQ == 17
#define _IQtoF(A)               _IQ17toF(A)
#endif
#if GLOBAL_IQ == 16
#define _IQtoF(A)               _IQ16toF(A)
#endif
#if GLOBAL_IQ == 15
#define _IQtoF(A)               _IQ15toF(A)
#endif
#if GLOBAL_IQ == 14
#define _IQtoF(A)               _IQ14toF(A)
#endif
#if GLOBAL_IQ == 13
#define _IQtoF(A)               _IQ13toF(A)
#endif
#if GLOBAL_IQ == 12
#define _IQtoF(A)               _IQ12toF(A)
#endif
#if GLOBAL_IQ == 11
#define _IQtoF(A)               _IQ11toF(A)
#endif
#if GLOBAL_IQ == 10
#define _IQtoF(A)               _IQ10toF(A)
#endif
#if GLOBAL_IQ == 9
#define _IQtoF(A)               _IQ9toF(A)
#endif
#if GLOBAL_IQ == 8
#define _IQtoF(A)               _IQ8toF(A)
#endif
#if GLOBAL_IQ == 7
#define _IQtoF(A)               _IQ7toF(A)
#endif
#if GLOBAL_IQ == 6
#define _IQtoF(A)               _IQ6toF(A)
#endif
#if GLOBAL_IQ == 5
#define _IQtoF(A)               _IQ5toF(A)
#endif
#if GLOBAL_IQ == 4
#define _IQtoF(A)               _IQ4toF(A)
#endif
#if GLOBAL_IQ == 3
#define _IQtoF(A)               _IQ3toF(A)
#endif
#if GLOBAL_IQ == 2
#define _IQtoF(A)               _IQ2toF(A)
#endif
#if GLOBAL_IQ == 1
#define _IQtoF(A)               _IQ1toF(A)
#endif

//*****************************************************************************
//
// Saturates an IQ number in a given range.
//
//*****************************************************************************
/**
 * @brief Saturates an IQ number in a given range.
 *
 * @param A               IQ number to be saturated.
 * @param Pos             Maximum positive value.
 * @param Neg             Minimum negative Value.
 *
 * @return                Saturated IQ type result.
 */
#define _IQsat(A, Pos, Neg)     (((A) > (Pos)) ?                              \
                                 (Pos) :                                      \
                                 (((A) < (Neg)) ? (Neg) : (A)))

#ifndef DOXYGEN_SHOULD_SKIP_THIS
//*****************************************************************************
//
// Converts an IQ number between the global IQ format and a specified IQ
// format.
//
//*****************************************************************************
#define _IQtoIQ30(A)            ((_iq30)(A) << (30 - GLOBAL_IQ))
#define _IQ30toIQ(A)            ((_iq30)(A) >> (30 - GLOBAL_IQ))

#if (GLOBAL_IQ >= 29)
#define _IQtoIQ29(A)            ((_iq29)(A) >> (GLOBAL_IQ - 29))
#define _IQ29toIQ(A)            ((_iq29)(A) << (GLOBAL_IQ - 29))
#else
#define _IQtoIQ29(A)            ((_iq29)(A) << (29 - GLOBAL_IQ))
#define _IQ29toIQ(A)            ((_iq29)(A) >> (29 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 28)
#define _IQtoIQ28(A)            ((_iq28)(A) >> (GLOBAL_IQ - 28))
#define _IQ28toIQ(A)            ((_iq28)(A) << (GLOBAL_IQ - 28))
#else
#define _IQtoIQ28(A)            ((_iq28)(A) << (28 - GLOBAL_IQ))
#define _IQ28toIQ(A)            ((_iq28)(A) >> (28 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 27)
#define _IQtoIQ27(A)            ((_iq27)(A) >> (GLOBAL_IQ - 27))
#define _IQ27toIQ(A)            ((_iq27)(A) << (GLOBAL_IQ - 27))
#else
#define _IQtoIQ27(A)            ((_iq27)(A) << (27 - GLOBAL_IQ))
#define _IQ27toIQ(A)            ((_iq27)(A) >> (27 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 26)
#define _IQtoIQ26(A)            ((_iq26)(A) >> (GLOBAL_IQ - 26))
#define _IQ26toIQ(A)            ((_iq26)(A) << (GLOBAL_IQ - 26))
#else
#define _IQtoIQ26(A)            ((_iq26)(A) << (26 - GLOBAL_IQ))
#define _IQ26toIQ(A)            ((_iq26)(A) >> (26 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 25)
#define _IQtoIQ25(A)            ((_iq25)(A) >> (GLOBAL_IQ - 25))
#define _IQ25toIQ(A)            ((_iq25)(A) << (GLOBAL_IQ - 25))
#else
#define _IQtoIQ25(A)            ((_iq25)(A) << (25 - GLOBAL_IQ))
#define _IQ25toIQ(A)            ((_iq25)(A) >> (25 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 24)
#define _IQtoIQ24(A)            ((_iq24)(A) >> (GLOBAL_IQ - 24))
#define _IQ24toIQ(A)            ((_iq24)(A) << (GLOBAL_IQ - 24))
#else
#define _IQtoIQ24(A)            ((_iq24)(A) << (24 - GLOBAL_IQ))
#define _IQ24toIQ(A)            ((_iq24)(A) >> (24 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 23)
#define _IQtoIQ23(A)            ((_iq23)(A) >> (GLOBAL_IQ - 23))
#define _IQ23toIQ(A)            ((_iq23)(A) << (GLOBAL_IQ - 23))
#else
#define _IQtoIQ23(A)            ((_iq23)(A) << (23 - GLOBAL_IQ))
#define _IQ23toIQ(A)            ((_iq23)(A) >> (23 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 22)
#define _IQtoIQ22(A)            ((_iq22)(A) >> (GLOBAL_IQ - 22))
#define _IQ22toIQ(A)            ((_iq22)(A) << (GLOBAL_IQ - 22))
#else
#define _IQtoIQ22(A)            ((_iq22)(A) << (22 - GLOBAL_IQ))
#define _IQ22toIQ(A)            ((_iq22)(A) >> (22 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 21)
#define _IQtoIQ21(A)            ((_iq21)(A) >> (GLOBAL_IQ - 21))
#define _IQ21toIQ(A)            ((_iq21)(A) << (GLOBAL_IQ - 21))
#else
#define _IQtoIQ21(A)            ((_iq21)(A) << (21 - GLOBAL_IQ))
#define _IQ21toIQ(A)            ((_iq21)(A) >> (21 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 20)
#define _IQtoIQ20(A)            ((_iq20)(A) >> (GLOBAL_IQ - 20))
#define _IQ20toIQ(A)            ((_iq20)(A) << (GLOBAL_IQ - 20))
#else
#define _IQtoIQ20(A)            ((_iq20)(A) << (20 - GLOBAL_IQ))
#define _IQ20toIQ(A)            ((_iq20)(A) >> (20 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 19)
#define _IQtoIQ19(A)            ((_iq19)(A) >> (GLOBAL_IQ - 19))
#define _IQ19toIQ(A)            ((_iq19)(A) << (GLOBAL_IQ - 19))
#else
#define _IQtoIQ19(A)            ((_iq19)(A) << (19 - GLOBAL_IQ))
#define _IQ19toIQ(A)            ((_iq19)(A) >> (19 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 18)
#define _IQtoIQ18(A)            ((_iq18)(A) >> (GLOBAL_IQ - 18))
#define _IQ18toIQ(A)            ((_iq18)(A) << (GLOBAL_IQ - 18))
#else
#define _IQtoIQ18(A)            ((_iq18)(A) << (18 - GLOBAL_IQ))
#define _IQ18toIQ(A)            ((_iq18)(A) >> (18 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 17)
#define _IQtoIQ17(A)            ((_iq17)(A) >> (GLOBAL_IQ - 17))
#define _IQ17toIQ(A)            ((_iq17)(A) << (GLOBAL_IQ - 17))
#else
#define _IQtoIQ17(A)            ((_iq17)(A) << (17 - GLOBAL_IQ))
#define _IQ17toIQ(A)            ((_iq17)(A) >> (17 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 16)
#define _IQtoIQ16(A)            ((_iq16)(A) >> (GLOBAL_IQ - 16))
#define _IQ16toIQ(A)            ((_iq16)(A) << (GLOBAL_IQ - 16))
#else
#define _IQtoIQ16(A)            ((_iq16)(A) << (16 - GLOBAL_IQ))
#define _IQ16toIQ(A)            ((_iq16)(A) >> (16 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 15)
#define _IQtoIQ15(A)            ((_iq15)(A) >> (GLOBAL_IQ - 15))
#define _IQ15toIQ(A)            ((_iq15)(A) << (GLOBAL_IQ - 15))
#else
#define _IQtoIQ15(A)            ((_iq15)(A) << (15 - GLOBAL_IQ))
#define _IQ15toIQ(A)            ((_iq15)(A) >> (15 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 14)
#define _IQtoIQ14(A)            ((_iq14)(A) >> (GLOBAL_IQ - 14))
#define _IQ14toIQ(A)            ((_iq14)(A) << (GLOBAL_IQ - 14))
#else
#define _IQtoIQ14(A)            ((_iq14)(A) << (14 - GLOBAL_IQ))
#define _IQ14toIQ(A)            ((_iq14)(A) >> (14 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 13)
#define _IQtoIQ13(A)            ((_iq13)(A) >> (GLOBAL_IQ - 13))
#define _IQ13toIQ(A)            ((_iq13)(A) << (GLOBAL_IQ - 13))
#else
#define _IQtoIQ13(A)            ((_iq13)(A) << (13 - GLOBAL_IQ))
#define _IQ13toIQ(A)            ((_iq13)(A) >> (13 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 12)
#define _IQtoIQ12(A)            ((_iq12)(A) >> (GLOBAL_IQ - 12))
#define _IQ12toIQ(A)            ((_iq12)(A) << (GLOBAL_IQ - 12))
#else
#define _IQtoIQ12(A)            ((_iq12)(A) << (12 - GLOBAL_IQ))
#define _IQ12toIQ(A)            ((_iq12)(A) >> (12 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 11)
#define _IQtoIQ11(A)            ((_iq11)(A) >> (GLOBAL_IQ - 11))
#define _IQ11toIQ(A)            ((_iq11)(A) << (GLOBAL_IQ - 11))
#else
#define _IQtoIQ11(A)            ((_iq11)(A) << (11 - GLOBAL_IQ))
#define _IQ11toIQ(A)            ((_iq11)(A) >> (11 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 10)
#define _IQtoIQ10(A)            ((_iq10)(A) >> (GLOBAL_IQ - 10))
#define _IQ10toIQ(A)            ((_iq10)(A) << (GLOBAL_IQ - 10))
#else
#define _IQtoIQ10(A)            ((_iq10)(A) << (10 - GLOBAL_IQ))
#define _IQ10toIQ(A)            ((_iq10)(A) >> (10 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 9)
#define _IQtoIQ9(A)             ((_iq9)(A) >> (GLOBAL_IQ - 9))
#define _IQ9toIQ(A)             ((_iq9)(A) << (GLOBAL_IQ - 9))
#else
#define _IQtoIQ9(A)             ((_iq9)(A) << (9 - GLOBAL_IQ))
#define _IQ9toIQ(A)             ((_iq9)(A) >> (9 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 8)
#define _IQtoIQ8(A)             ((_iq8)(A) >> (GLOBAL_IQ - 8))
#define _IQ8toIQ(A)             ((_iq8)(A) << (GLOBAL_IQ - 8))
#else
#define _IQtoIQ8(A)             ((_iq8)(A) << (8 - GLOBAL_IQ))
#define _IQ8toIQ(A)             ((_iq8)(A) >> (8 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 7)
#define _IQtoIQ7(A)             ((_iq7)(A) >> (GLOBAL_IQ - 7))
#define _IQ7toIQ(A)             ((_iq7)(A) << (GLOBAL_IQ - 7))
#else
#define _IQtoIQ7(A)             ((_iq7)(A) << (7 - GLOBAL_IQ))
#define _IQ7toIQ(A)             ((_iq7)(A) >> (7 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 6)
#define _IQtoIQ6(A)             ((_iq6)(A) >> (GLOBAL_IQ - 6))
#define _IQ6toIQ(A)             ((_iq6)(A) << (GLOBAL_IQ - 6))
#else
#define _IQtoIQ6(A)             ((_iq6)(A) << (6 - GLOBAL_IQ))
#define _IQ6toIQ(A)             ((_iq6)(A) >> (6 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 5)
#define _IQtoIQ5(A)             ((_iq5)(A) >> (GLOBAL_IQ - 5))
#define _IQ5toIQ(A)             ((_iq5)(A) << (GLOBAL_IQ - 5))
#else
#define _IQtoIQ5(A)             ((_iq5)(A) << (5 - GLOBAL_IQ))
#define _IQ5toIQ(A)             ((_iq5)(A) >> (5 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 4)
#define _IQtoIQ4(A)             ((_iq4)(A) >> (GLOBAL_IQ - 4))
#define _IQ4toIQ(A)             ((_iq4)(A) << (GLOBAL_IQ - 4))
#else
#define _IQtoIQ4(A)             ((_iq4)(A) << (4 - GLOBAL_IQ))
#define _IQ4toIQ(A)             ((_iq4)(A) >> (4 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 3)
#define _IQtoIQ3(A)             ((_iq3)(A) >> (GLOBAL_IQ - 3))
#define _IQ3toIQ(A)             ((_iq3)(A) << (GLOBAL_IQ - 3))
#else
#define _IQtoIQ3(A)             ((_iq3)(A) << (3 - GLOBAL_IQ))
#define _IQ3toIQ(A)             ((_iq3)(A) >> (3 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 2)
#define _IQtoIQ2(A)             ((_iq2)(A) >> (GLOBAL_IQ - 2))
#define _IQ2toIQ(A)             ((_iq2)(A) << (GLOBAL_IQ - 2))
#else
#define _IQtoIQ2(A)             ((_iq2)(A) << (2 - GLOBAL_IQ))
#define _IQ2toIQ(A)             ((_iq2)(A) >> (2 - GLOBAL_IQ))
#endif

#define _IQtoIQ1(A)             ((_iq1)(A) >> (GLOBAL_IQ - 1))
#define _IQ1toIQ(A)             ((_iq1)(A) << (GLOBAL_IQ - 1))
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
//*****************************************************************************
//
// Converts an IQ number between a specified IQ format and another
// specified IQ format.
//
// For the functions of type _IQXtoIQY, the values are shifted by the
// difference of X and Y, depending on which one is larger. The definition is
// as follows:
//
//    #define _IQXtoIQY(A)
//      If X < Y
//          ((_iqY) (A) << (Y-X)
//      else
//          ((_iqY) (A) >> (X-Y)
//
//*****************************************************************************
/* IQ1 to IQN */
#define _IQ1toIQ30(A)            ((_iq30)(A) << (29))
#define _IQ1toIQ29(A)            ((_iq29)(A) << (28))
#define _IQ1toIQ28(A)            ((_iq28)(A) << (27))
#define _IQ1toIQ27(A)            ((_iq27)(A) << (26))
#define _IQ1toIQ26(A)            ((_iq26)(A) << (25))
#define _IQ1toIQ25(A)            ((_iq25)(A) << (24))
#define _IQ1toIQ24(A)            ((_iq24)(A) << (23))
#define _IQ1toIQ23(A)            ((_iq23)(A) << (22))
#define _IQ1toIQ22(A)            ((_iq22)(A) << (21))
#define _IQ1toIQ21(A)            ((_iq21)(A) << (20))
#define _IQ1toIQ20(A)            ((_iq20)(A) << (19))
#define _IQ1toIQ19(A)            ((_iq19)(A) << (18))
#define _IQ1toIQ18(A)            ((_iq18)(A) << (17))
#define _IQ1toIQ17(A)            ((_iq17)(A) << (16))
#define _IQ1toIQ16(A)            ((_iq16)(A) << (15))
#define _IQ1toIQ15(A)            ((_iq15)(A) << (14))
#define _IQ1toIQ14(A)            ((_iq14)(A) << (13))
#define _IQ1toIQ13(A)            ((_iq13)(A) << (12))
#define _IQ1toIQ12(A)            ((_iq12)(A) << (11))
#define _IQ1toIQ11(A)            ((_iq11)(A) << (10))
#define _IQ1toIQ10(A)            ((_iq10)(A) << (9))
#define  _IQ1toIQ9(A)            ((_iq9 )(A) << (8))
#define  _IQ1toIQ8(A)            ((_iq8 )(A) << (7))
#define  _IQ1toIQ7(A)            ((_iq7 )(A) << (6))
#define  _IQ1toIQ6(A)            ((_iq6 )(A) << (5))
#define  _IQ1toIQ5(A)            ((_iq5 )(A) << (4))
#define  _IQ1toIQ4(A)            ((_iq4 )(A) << (3))
#define  _IQ1toIQ3(A)            ((_iq3 )(A) << (2))
#define  _IQ1toIQ2(A)            ((_iq2 )(A) << (1))

/* IQ2 to IQN */
#define _IQ2toIQ30(A)            ((_iq30)(A) << (28))
#define _IQ2toIQ29(A)            ((_iq29)(A) << (27))
#define _IQ2toIQ28(A)            ((_iq28)(A) << (26))
#define _IQ2toIQ27(A)            ((_iq27)(A) << (25))
#define _IQ2toIQ26(A)            ((_iq26)(A) << (24))
#define _IQ2toIQ25(A)            ((_iq25)(A) << (23))
#define _IQ2toIQ24(A)            ((_iq24)(A) << (22))
#define _IQ2toIQ23(A)            ((_iq23)(A) << (21))
#define _IQ2toIQ22(A)            ((_iq22)(A) << (20))
#define _IQ2toIQ21(A)            ((_iq21)(A) << (19))
#define _IQ2toIQ20(A)            ((_iq20)(A) << (18))
#define _IQ2toIQ19(A)            ((_iq19)(A) << (17))
#define _IQ2toIQ18(A)            ((_iq18)(A) << (16))
#define _IQ2toIQ17(A)            ((_iq17)(A) << (15))
#define _IQ2toIQ16(A)            ((_iq16)(A) << (14))
#define _IQ2toIQ15(A)            ((_iq15)(A) << (13))
#define _IQ2toIQ14(A)            ((_iq14)(A) << (12))
#define _IQ2toIQ13(A)            ((_iq13)(A) << (11))
#define _IQ2toIQ12(A)            ((_iq12)(A) << (10))
#define _IQ2toIQ11(A)            ((_iq11)(A) << (9))
#define _IQ2toIQ10(A)            ((_iq10)(A) << (8))
#define  _IQ2toIQ9(A)            ((_iq9 )(A) << (7))
#define  _IQ2toIQ8(A)            ((_iq8 )(A) << (6))
#define  _IQ2toIQ7(A)            ((_iq7 )(A) << (5))
#define  _IQ2toIQ6(A)            ((_iq6 )(A) << (4))
#define  _IQ2toIQ5(A)            ((_iq5 )(A) << (3))
#define  _IQ2toIQ4(A)            ((_iq4 )(A) << (2))
#define  _IQ2toIQ3(A)            ((_iq3 )(A) << (1))
#define  _IQ2toIQ1(A)            ((_iq1 )(A) >> (1))

/* IQ3 to IQN */
#define _IQ3toIQ30(A)            ((_iq30)(A) << (27))
#define _IQ3toIQ29(A)            ((_iq29)(A) << (26))
#define _IQ3toIQ28(A)            ((_iq28)(A) << (25))
#define _IQ3toIQ27(A)            ((_iq27)(A) << (24))
#define _IQ3toIQ26(A)            ((_iq26)(A) << (23))
#define _IQ3toIQ25(A)            ((_iq25)(A) << (22))
#define _IQ3toIQ24(A)            ((_iq24)(A) << (21))
#define _IQ3toIQ23(A)            ((_iq23)(A) << (20))
#define _IQ3toIQ22(A)            ((_iq22)(A) << (19))
#define _IQ3toIQ21(A)            ((_iq21)(A) << (18))
#define _IQ3toIQ20(A)            ((_iq20)(A) << (17))
#define _IQ3toIQ19(A)            ((_iq19)(A) << (16))
#define _IQ3toIQ18(A)            ((_iq18)(A) << (15))
#define _IQ3toIQ17(A)            ((_iq17)(A) << (14))
#define _IQ3toIQ16(A)            ((_iq16)(A) << (13))
#define _IQ3toIQ15(A)            ((_iq15)(A) << (12))
#define _IQ3toIQ14(A)            ((_iq14)(A) << (11))
#define _IQ3toIQ13(A)            ((_iq13)(A) << (10))
#define _IQ3toIQ12(A)            ((_iq12)(A) << (9))
#define _IQ3toIQ11(A)            ((_iq11)(A) << (8))
#define _IQ3toIQ10(A)            ((_iq10)(A) << (7))
#define  _IQ3toIQ9(A)            ((_iq9 )(A) << (6))
#define  _IQ3toIQ8(A)            ((_iq8 )(A) << (5))
#define  _IQ3toIQ7(A)            ((_iq7 )(A) << (4))
#define  _IQ3toIQ6(A)            ((_iq6 )(A) << (3))
#define  _IQ3toIQ5(A)            ((_iq5 )(A) << (2))
#define  _IQ3toIQ4(A)            ((_iq4 )(A) << (1))
#define  _IQ3toIQ2(A)            ((_iq2 )(A) >> (1))
#define  _IQ3toIQ1(A)            ((_iq1 )(A) >> (2))

/* IQ4 to IQN */
#define _IQ4toIQ30(A)            ((_iq30)(A) << (26))
#define _IQ4toIQ29(A)            ((_iq29)(A) << (25))
#define _IQ4toIQ28(A)            ((_iq28)(A) << (24))
#define _IQ4toIQ27(A)            ((_iq27)(A) << (23))
#define _IQ4toIQ26(A)            ((_iq26)(A) << (22))
#define _IQ4toIQ25(A)            ((_iq25)(A) << (21))
#define _IQ4toIQ24(A)            ((_iq24)(A) << (20))
#define _IQ4toIQ23(A)            ((_iq23)(A) << (19))
#define _IQ4toIQ22(A)            ((_iq22)(A) << (18))
#define _IQ4toIQ21(A)            ((_iq21)(A) << (17))
#define _IQ4toIQ20(A)            ((_iq20)(A) << (16))
#define _IQ4toIQ19(A)            ((_iq19)(A) << (15))
#define _IQ4toIQ18(A)            ((_iq18)(A) << (14))
#define _IQ4toIQ17(A)            ((_iq17)(A) << (13))
#define _IQ4toIQ16(A)            ((_iq16)(A) << (12))
#define _IQ4toIQ15(A)            ((_iq15)(A) << (11))
#define _IQ4toIQ14(A)            ((_iq14)(A) << (10))
#define _IQ4toIQ13(A)            ((_iq13)(A) << (9))
#define _IQ4toIQ12(A)            ((_iq12)(A) << (8))
#define _IQ4toIQ11(A)            ((_iq11)(A) << (7))
#define _IQ4toIQ10(A)            ((_iq10)(A) << (6))
#define  _IQ4toIQ9(A)            ((_iq9 )(A) << (5))
#define  _IQ4toIQ8(A)            ((_iq8 )(A) << (4))
#define  _IQ4toIQ7(A)            ((_iq7 )(A) << (3))
#define  _IQ4toIQ6(A)            ((_iq6 )(A) << (2))
#define  _IQ4toIQ5(A)            ((_iq5 )(A) << (1))
#define  _IQ4toIQ3(A)            ((_iq3 )(A) >> (1))
#define  _IQ4toIQ2(A)            ((_iq2 )(A) >> (2))
#define  _IQ4toIQ1(A)            ((_iq1 )(A) >> (3))

/* IQ5 to IQN */
#define _IQ5toIQ30(A)            ((_iq30)(A) << (25))
#define _IQ5toIQ29(A)            ((_iq29)(A) << (24))
#define _IQ5toIQ28(A)            ((_iq28)(A) << (23))
#define _IQ5toIQ27(A)            ((_iq27)(A) << (22))
#define _IQ5toIQ26(A)            ((_iq26)(A) << (21))
#define _IQ5toIQ25(A)            ((_iq25)(A) << (20))
#define _IQ5toIQ24(A)            ((_iq24)(A) << (19))
#define _IQ5toIQ23(A)            ((_iq23)(A) << (18))
#define _IQ5toIQ22(A)            ((_iq22)(A) << (17))
#define _IQ5toIQ21(A)            ((_iq21)(A) << (16))
#define _IQ5toIQ20(A)            ((_iq20)(A) << (15))
#define _IQ5toIQ19(A)            ((_iq19)(A) << (14))
#define _IQ5toIQ18(A)            ((_iq18)(A) << (13))
#define _IQ5toIQ17(A)            ((_iq17)(A) << (12))
#define _IQ5toIQ16(A)            ((_iq16)(A) << (11))
#define _IQ5toIQ15(A)            ((_iq15)(A) << (10))
#define _IQ5toIQ14(A)            ((_iq14)(A) << (9))
#define _IQ5toIQ13(A)            ((_iq13)(A) << (8))
#define _IQ5toIQ12(A)            ((_iq12)(A) << (7))
#define _IQ5toIQ11(A)            ((_iq11)(A) << (6))
#define _IQ5toIQ10(A)            ((_iq10)(A) << (5))
#define  _IQ5toIQ9(A)            ((_iq9 )(A) << (4))
#define  _IQ5toIQ8(A)            ((_iq8 )(A) << (3))
#define  _IQ5toIQ7(A)            ((_iq7 )(A) << (2))
#define  _IQ5toIQ6(A)            ((_iq6 )(A) << (1))
#define  _IQ5toIQ4(A)            ((_iq4 )(A) >> (1))
#define  _IQ5toIQ3(A)            ((_iq3 )(A) >> (2))
#define  _IQ5toIQ2(A)            ((_iq2 )(A) >> (3))
#define  _IQ5toIQ1(A)            ((_iq1 )(A) >> (4))

/* IQ6 to IQN */
#define _IQ6toIQ30(A)            ((_iq30)(A) << (24))
#define _IQ6toIQ29(A)            ((_iq29)(A) << (23))
#define _IQ6toIQ28(A)            ((_iq28)(A) << (22))
#define _IQ6toIQ27(A)            ((_iq27)(A) << (21))
#define _IQ6toIQ26(A)            ((_iq26)(A) << (20))
#define _IQ6toIQ25(A)            ((_iq25)(A) << (19))
#define _IQ6toIQ24(A)            ((_iq24)(A) << (18))
#define _IQ6toIQ23(A)            ((_iq23)(A) << (17))
#define _IQ6toIQ22(A)            ((_iq22)(A) << (16))
#define _IQ6toIQ21(A)            ((_iq21)(A) << (15))
#define _IQ6toIQ20(A)            ((_iq20)(A) << (14))
#define _IQ6toIQ19(A)            ((_iq19)(A) << (13))
#define _IQ6toIQ18(A)            ((_iq18)(A) << (12))
#define _IQ6toIQ17(A)            ((_iq17)(A) << (11))
#define _IQ6toIQ16(A)            ((_iq16)(A) << (10))
#define _IQ6toIQ15(A)            ((_iq15)(A) << (9))
#define _IQ6toIQ14(A)            ((_iq14)(A) << (8))
#define _IQ6toIQ13(A)            ((_iq13)(A) << (7))
#define _IQ6toIQ12(A)            ((_iq12)(A) << (6))
#define _IQ6toIQ11(A)            ((_iq11)(A) << (5))
#define _IQ6toIQ10(A)            ((_iq10)(A) << (4))
#define  _IQ6toIQ9(A)            ((_iq9 )(A) << (3))
#define  _IQ6toIQ8(A)            ((_iq8 )(A) << (2))
#define  _IQ6toIQ7(A)            ((_iq7 )(A) << (1))
#define  _IQ6toIQ5(A)            ((_iq5 )(A) >> (1))
#define  _IQ6toIQ4(A)            ((_iq4 )(A) >> (2))
#define  _IQ6toIQ3(A)            ((_iq3 )(A) >> (3))
#define  _IQ6toIQ2(A)            ((_iq2 )(A) >> (4))
#define  _IQ6toIQ1(A)            ((_iq1 )(A) >> (5))

/* IQ7 to IQN */
#define _IQ7toIQ30(A)            ((_iq30)(A) << (23))
#define _IQ7toIQ29(A)            ((_iq29)(A) << (22))
#define _IQ7toIQ28(A)            ((_iq28)(A) << (21))
#define _IQ7toIQ27(A)            ((_iq27)(A) << (20))
#define _IQ7toIQ26(A)            ((_iq26)(A) << (19))
#define _IQ7toIQ25(A)            ((_iq25)(A) << (18))
#define _IQ7toIQ24(A)            ((_iq24)(A) << (17))
#define _IQ7toIQ23(A)            ((_iq23)(A) << (16))
#define _IQ7toIQ22(A)            ((_iq22)(A) << (15))
#define _IQ7toIQ21(A)            ((_iq21)(A) << (14))
#define _IQ7toIQ20(A)            ((_iq20)(A) << (13))
#define _IQ7toIQ19(A)            ((_iq19)(A) << (12))
#define _IQ7toIQ18(A)            ((_iq18)(A) << (11))
#define _IQ7toIQ17(A)            ((_iq17)(A) << (10))
#define _IQ7toIQ16(A)            ((_iq16)(A) << (9))
#define _IQ7toIQ15(A)            ((_iq15)(A) << (8))
#define _IQ7toIQ14(A)            ((_iq14)(A) << (7))
#define _IQ7toIQ13(A)            ((_iq13)(A) << (6))
#define _IQ7toIQ12(A)            ((_iq12)(A) << (5))
#define _IQ7toIQ11(A)            ((_iq11)(A) << (4))
#define _IQ7toIQ10(A)            ((_iq10)(A) << (3))
#define  _IQ7toIQ9(A)            ((_iq9 )(A) << (2))
#define  _IQ7toIQ8(A)            ((_iq8 )(A) << (1))
#define  _IQ7toIQ6(A)            ((_iq6 )(A) >> (1))
#define  _IQ7toIQ5(A)            ((_iq5 )(A) >> (2))
#define  _IQ7toIQ4(A)            ((_iq4 )(A) >> (3))
#define  _IQ7toIQ3(A)            ((_iq3 )(A) >> (4))
#define  _IQ7toIQ2(A)            ((_iq2 )(A) >> (5))
#define  _IQ7toIQ1(A)            ((_iq1 )(A) >> (6))

/* IQ8 to IQN */
#define _IQ8toIQ30(A)            ((_iq30)(A) << (22))
#define _IQ8toIQ29(A)            ((_iq29)(A) << (21))
#define _IQ8toIQ28(A)            ((_iq28)(A) << (20))
#define _IQ8toIQ27(A)            ((_iq27)(A) << (19))
#define _IQ8toIQ26(A)            ((_iq26)(A) << (18))
#define _IQ8toIQ25(A)            ((_iq25)(A) << (17))
#define _IQ8toIQ24(A)            ((_iq24)(A) << (16))
#define _IQ8toIQ23(A)            ((_iq23)(A) << (15))
#define _IQ8toIQ22(A)            ((_iq22)(A) << (14))
#define _IQ8toIQ21(A)            ((_iq21)(A) << (13))
#define _IQ8toIQ20(A)            ((_iq20)(A) << (12))
#define _IQ8toIQ19(A)            ((_iq19)(A) << (11))
#define _IQ8toIQ18(A)            ((_iq18)(A) << (10))
#define _IQ8toIQ17(A)            ((_iq17)(A) << (9))
#define _IQ8toIQ16(A)            ((_iq16)(A) << (8))
#define _IQ8toIQ15(A)            ((_iq15)(A) << (7))
#define _IQ8toIQ14(A)            ((_iq14)(A) << (6))
#define _IQ8toIQ13(A)            ((_iq13)(A) << (5))
#define _IQ8toIQ12(A)            ((_iq12)(A) << (4))
#define _IQ8toIQ11(A)            ((_iq11)(A) << (3))
#define _IQ8toIQ10(A)            ((_iq10)(A) << (2))
#define  _IQ8toIQ9(A)            ((_iq9 )(A) << (1))
#define  _IQ8toIQ7(A)            ((_iq7 )(A) >> (1))
#define  _IQ8toIQ6(A)            ((_iq6 )(A) >> (2))
#define  _IQ8toIQ5(A)            ((_iq5 )(A) >> (3))
#define  _IQ8toIQ4(A)            ((_iq4 )(A) >> (4))
#define  _IQ8toIQ3(A)            ((_iq3 )(A) >> (5))
#define  _IQ8toIQ2(A)            ((_iq2 )(A) >> (6))
#define  _IQ8toIQ1(A)            ((_iq1 )(A) >> (7))

/* IQ9 to IQN */
#define _IQ9toIQ30(A)            ((_iq30)(A) << (21))
#define _IQ9toIQ29(A)            ((_iq29)(A) << (20))
#define _IQ9toIQ28(A)            ((_iq28)(A) << (19))
#define _IQ9toIQ27(A)            ((_iq27)(A) << (18))
#define _IQ9toIQ26(A)            ((_iq26)(A) << (17))
#define _IQ9toIQ25(A)            ((_iq25)(A) << (16))
#define _IQ9toIQ24(A)            ((_iq24)(A) << (15))
#define _IQ9toIQ23(A)            ((_iq23)(A) << (14))
#define _IQ9toIQ22(A)            ((_iq22)(A) << (13))
#define _IQ9toIQ21(A)            ((_iq21)(A) << (12))
#define _IQ9toIQ20(A)            ((_iq20)(A) << (11))
#define _IQ9toIQ19(A)            ((_iq19)(A) << (10))
#define _IQ9toIQ18(A)            ((_iq18)(A) << (9))
#define _IQ9toIQ17(A)            ((_iq17)(A) << (8))
#define _IQ9toIQ16(A)            ((_iq16)(A) << (7))
#define _IQ9toIQ15(A)            ((_iq15)(A) << (6))
#define _IQ9toIQ14(A)            ((_iq14)(A) << (5))
#define _IQ9toIQ13(A)            ((_iq13)(A) << (4))
#define _IQ9toIQ12(A)            ((_iq12)(A) << (3))
#define _IQ9toIQ11(A)            ((_iq11)(A) << (2))
#define _IQ9toIQ10(A)            ((_iq10)(A) << (1))
#define  _IQ9toIQ8(A)            ((_iq8 )(A) >> (1))
#define  _IQ9toIQ7(A)            ((_iq7 )(A) >> (2))
#define  _IQ9toIQ6(A)            ((_iq6 )(A) >> (3))
#define  _IQ9toIQ5(A)            ((_iq5 )(A) >> (4))
#define  _IQ9toIQ4(A)            ((_iq4 )(A) >> (5))
#define  _IQ9toIQ3(A)            ((_iq3 )(A) >> (6))
#define  _IQ9toIQ2(A)            ((_iq2 )(A) >> (7))
#define  _IQ9toIQ1(A)            ((_iq1 )(A) >> (8))

/* IQ10 to IQN */
#define _IQ10toIQ30(A)            ((_iq30)(A) << (20))
#define _IQ10toIQ29(A)            ((_iq29)(A) << (19))
#define _IQ10toIQ28(A)            ((_iq28)(A) << (18))
#define _IQ10toIQ27(A)            ((_iq27)(A) << (17))
#define _IQ10toIQ26(A)            ((_iq26)(A) << (16))
#define _IQ10toIQ25(A)            ((_iq25)(A) << (15))
#define _IQ10toIQ24(A)            ((_iq24)(A) << (14))
#define _IQ10toIQ23(A)            ((_iq23)(A) << (13))
#define _IQ10toIQ22(A)            ((_iq22)(A) << (12))
#define _IQ10toIQ21(A)            ((_iq21)(A) << (11))
#define _IQ10toIQ20(A)            ((_iq20)(A) << (10))
#define _IQ10toIQ19(A)            ((_iq19)(A) << (9))
#define _IQ10toIQ18(A)            ((_iq18)(A) << (8))
#define _IQ10toIQ17(A)            ((_iq17)(A) << (7))
#define _IQ10toIQ16(A)            ((_iq16)(A) << (6))
#define _IQ10toIQ15(A)            ((_iq15)(A) << (5))
#define _IQ10toIQ14(A)            ((_iq14)(A) << (4))
#define _IQ10toIQ13(A)            ((_iq13)(A) << (3))
#define _IQ10toIQ12(A)            ((_iq12)(A) << (2))
#define _IQ10toIQ11(A)            ((_iq11)(A) << (1))
#define  _IQ10toIQ9(A)            ((_iq9 )(A) >> (1))
#define  _IQ10toIQ8(A)            ((_iq8 )(A) >> (2))
#define  _IQ10toIQ7(A)            ((_iq7 )(A) >> (3))
#define  _IQ10toIQ6(A)            ((_iq6 )(A) >> (4))
#define  _IQ10toIQ5(A)            ((_iq5 )(A) >> (5))
#define  _IQ10toIQ4(A)            ((_iq4 )(A) >> (6))
#define  _IQ10toIQ3(A)            ((_iq3 )(A) >> (7))
#define  _IQ10toIQ2(A)            ((_iq2 )(A) >> (8))
#define  _IQ10toIQ1(A)            ((_iq1 )(A) >> (9))

/* IQ11 to IQN */
#define _IQ11toIQ30(A)            ((_iq30)(A) << (19))
#define _IQ11toIQ29(A)            ((_iq29)(A) << (18))
#define _IQ11toIQ28(A)            ((_iq28)(A) << (17))
#define _IQ11toIQ27(A)            ((_iq27)(A) << (16))
#define _IQ11toIQ26(A)            ((_iq26)(A) << (15))
#define _IQ11toIQ25(A)            ((_iq25)(A) << (14))
#define _IQ11toIQ24(A)            ((_iq24)(A) << (13))
#define _IQ11toIQ23(A)            ((_iq23)(A) << (12))
#define _IQ11toIQ22(A)            ((_iq22)(A) << (11))
#define _IQ11toIQ21(A)            ((_iq21)(A) << (10))
#define _IQ11toIQ20(A)            ((_iq20)(A) << (9))
#define _IQ11toIQ19(A)            ((_iq19)(A) << (8))
#define _IQ11toIQ18(A)            ((_iq18)(A) << (7))
#define _IQ11toIQ17(A)            ((_iq17)(A) << (6))
#define _IQ11toIQ16(A)            ((_iq16)(A) << (5))
#define _IQ11toIQ15(A)            ((_iq15)(A) << (4))
#define _IQ11toIQ14(A)            ((_iq14)(A) << (3))
#define _IQ11toIQ13(A)            ((_iq13)(A) << (2))
#define _IQ11toIQ12(A)            ((_iq12)(A) << (1))
#define _IQ11toIQ10(A)            ((_iq10)(A) >> (1))
#define  _IQ11toIQ9(A)            ((_iq9 )(A) >> (2))
#define  _IQ11toIQ8(A)            ((_iq8 )(A) >> (3))
#define  _IQ11toIQ7(A)            ((_iq7 )(A) >> (4))
#define  _IQ11toIQ6(A)            ((_iq6 )(A) >> (5))
#define  _IQ11toIQ5(A)            ((_iq5 )(A) >> (6))
#define  _IQ11toIQ4(A)            ((_iq4 )(A) >> (7))
#define  _IQ11toIQ3(A)            ((_iq3 )(A) >> (8))
#define  _IQ11toIQ2(A)            ((_iq2 )(A) >> (9))
#define  _IQ11toIQ1(A)            ((_iq1 )(A) >> (10))

/* IQ12 to IQN */
#define _IQ12toIQ30(A)            ((_iq30)(A) << (18))
#define _IQ12toIQ29(A)            ((_iq29)(A) << (17))
#define _IQ12toIQ28(A)            ((_iq28)(A) << (16))
#define _IQ12toIQ27(A)            ((_iq27)(A) << (15))
#define _IQ12toIQ26(A)            ((_iq26)(A) << (14))
#define _IQ12toIQ25(A)            ((_iq25)(A) << (13))
#define _IQ12toIQ24(A)            ((_iq24)(A) << (12))
#define _IQ12toIQ23(A)            ((_iq23)(A) << (11))
#define _IQ12toIQ22(A)            ((_iq22)(A) << (10))
#define _IQ12toIQ21(A)            ((_iq21)(A) << (9))
#define _IQ12toIQ20(A)            ((_iq20)(A) << (8))
#define _IQ12toIQ19(A)            ((_iq19)(A) << (7))
#define _IQ12toIQ18(A)            ((_iq18)(A) << (6))
#define _IQ12toIQ17(A)            ((_iq17)(A) << (5))
#define _IQ12toIQ16(A)            ((_iq16)(A) << (4))
#define _IQ12toIQ15(A)            ((_iq15)(A) << (3))
#define _IQ12toIQ14(A)            ((_iq14)(A) << (2))
#define _IQ12toIQ13(A)            ((_iq13)(A) << (1))
#define _IQ12toIQ11(A)            ((_iq11)(A) >> (1))
#define _IQ12toIQ10(A)            ((_iq10)(A) >> (2))
#define  _IQ12toIQ9(A)            ((_iq9 )(A) >> (3))
#define  _IQ12toIQ8(A)            ((_iq8 )(A) >> (4))
#define  _IQ12toIQ7(A)            ((_iq7 )(A) >> (5))
#define  _IQ12toIQ6(A)            ((_iq6 )(A) >> (6))
#define  _IQ12toIQ5(A)            ((_iq5 )(A) >> (7))
#define  _IQ12toIQ4(A)            ((_iq4 )(A) >> (8))
#define  _IQ12toIQ3(A)            ((_iq3 )(A) >> (9))
#define  _IQ12toIQ2(A)            ((_iq2 )(A) >> (10))
#define  _IQ12toIQ1(A)            ((_iq1 )(A) >> (11))

/* IQ13 to IQN */
#define _IQ13toIQ30(A)            ((_iq30)(A) << (17))
#define _IQ13toIQ29(A)            ((_iq29)(A) << (16))
#define _IQ13toIQ28(A)            ((_iq28)(A) << (15))
#define _IQ13toIQ27(A)            ((_iq27)(A) << (14))
#define _IQ13toIQ26(A)            ((_iq26)(A) << (13))
#define _IQ13toIQ25(A)            ((_iq25)(A) << (12))
#define _IQ13toIQ24(A)            ((_iq24)(A) << (11))
#define _IQ13toIQ23(A)            ((_iq23)(A) << (10))
#define _IQ13toIQ22(A)            ((_iq22)(A) << (9))
#define _IQ13toIQ21(A)            ((_iq21)(A) << (8))
#define _IQ13toIQ20(A)            ((_iq20)(A) << (7))
#define _IQ13toIQ19(A)            ((_iq19)(A) << (6))
#define _IQ13toIQ18(A)            ((_iq18)(A) << (5))
#define _IQ13toIQ17(A)            ((_iq17)(A) << (4))
#define _IQ13toIQ16(A)            ((_iq16)(A) << (3))
#define _IQ13toIQ15(A)            ((_iq15)(A) << (2))
#define _IQ13toIQ14(A)            ((_iq14)(A) << (1))
#define _IQ13toIQ12(A)            ((_iq12)(A) >> (1))
#define _IQ13toIQ11(A)            ((_iq11)(A) >> (2))
#define _IQ13toIQ10(A)            ((_iq10)(A) >> (3))
#define  _IQ13toIQ9(A)            ((_iq9 )(A) >> (4))
#define  _IQ13toIQ8(A)            ((_iq8 )(A) >> (5))
#define  _IQ13toIQ7(A)            ((_iq7 )(A) >> (6))
#define  _IQ13toIQ6(A)            ((_iq6 )(A) >> (7))
#define  _IQ13toIQ5(A)            ((_iq5 )(A) >> (8))
#define  _IQ13toIQ4(A)            ((_iq4 )(A) >> (9))
#define  _IQ13toIQ3(A)            ((_iq3 )(A) >> (10))
#define  _IQ13toIQ2(A)            ((_iq2 )(A) >> (11))
#define  _IQ13toIQ1(A)            ((_iq1 )(A) >> (12))

/* IQ14 to IQN */
#define _IQ14toIQ30(A)            ((_iq30)(A) << (16))
#define _IQ14toIQ29(A)            ((_iq29)(A) << (15))
#define _IQ14toIQ28(A)            ((_iq28)(A) << (14))
#define _IQ14toIQ27(A)            ((_iq27)(A) << (13))
#define _IQ14toIQ26(A)            ((_iq26)(A) << (12))
#define _IQ14toIQ25(A)            ((_iq25)(A) << (11))
#define _IQ14toIQ24(A)            ((_iq24)(A) << (10))
#define _IQ14toIQ23(A)            ((_iq23)(A) << (9))
#define _IQ14toIQ22(A)            ((_iq22)(A) << (8))
#define _IQ14toIQ21(A)            ((_iq21)(A) << (7))
#define _IQ14toIQ20(A)            ((_iq20)(A) << (6))
#define _IQ14toIQ19(A)            ((_iq19)(A) << (5))
#define _IQ14toIQ18(A)            ((_iq18)(A) << (4))
#define _IQ14toIQ17(A)            ((_iq17)(A) << (3))
#define _IQ14toIQ16(A)            ((_iq16)(A) << (2))
#define _IQ14toIQ15(A)            ((_iq15)(A) << (1))
#define _IQ14toIQ13(A)            ((_iq13)(A) >> (1))
#define _IQ14toIQ12(A)            ((_iq12)(A) >> (2))
#define _IQ14toIQ11(A)            ((_iq11)(A) >> (3))
#define _IQ14toIQ10(A)            ((_iq10)(A) >> (4))
#define  _IQ14toIQ9(A)            ((_iq9 )(A) >> (5))
#define  _IQ14toIQ8(A)            ((_iq8 )(A) >> (6))
#define  _IQ14toIQ7(A)            ((_iq7 )(A) >> (7))
#define  _IQ14toIQ6(A)            ((_iq6 )(A) >> (8))
#define  _IQ14toIQ5(A)            ((_iq5 )(A) >> (9))
#define  _IQ14toIQ4(A)            ((_iq4 )(A) >> (10))
#define  _IQ14toIQ3(A)            ((_iq3 )(A) >> (11))
#define  _IQ14toIQ2(A)            ((_iq2 )(A) >> (12))
#define  _IQ14toIQ1(A)            ((_iq1 )(A) >> (13))

/* IQ15 to IQN */
#define _IQ15toIQ30(A)            ((_iq30)(A) << (15))
#define _IQ15toIQ29(A)            ((_iq29)(A) << (14))
#define _IQ15toIQ28(A)            ((_iq28)(A) << (13))
#define _IQ15toIQ27(A)            ((_iq27)(A) << (12))
#define _IQ15toIQ26(A)            ((_iq26)(A) << (11))
#define _IQ15toIQ25(A)            ((_iq25)(A) << (10))
#define _IQ15toIQ24(A)            ((_iq24)(A) << (9))
#define _IQ15toIQ23(A)            ((_iq23)(A) << (8))
#define _IQ15toIQ22(A)            ((_iq22)(A) << (7))
#define _IQ15toIQ21(A)            ((_iq21)(A) << (6))
#define _IQ15toIQ20(A)            ((_iq20)(A) << (5))
#define _IQ15toIQ19(A)            ((_iq19)(A) << (4))
#define _IQ15toIQ18(A)            ((_iq18)(A) << (3))
#define _IQ15toIQ17(A)            ((_iq17)(A) << (2))
#define _IQ15toIQ16(A)            ((_iq16)(A) << (1))
#define _IQ15toIQ14(A)            ((_iq14)(A) >> (1))
#define _IQ15toIQ13(A)            ((_iq13)(A) >> (2))
#define _IQ15toIQ12(A)            ((_iq12)(A) >> (3))
#define _IQ15toIQ11(A)            ((_iq11)(A) >> (4))
#define _IQ15toIQ10(A)            ((_iq10)(A) >> (5))
#define  _IQ15toIQ9(A)            ((_iq9 )(A) >> (6))
#define  _IQ15toIQ8(A)            ((_iq8 )(A) >> (7))
#define  _IQ15toIQ7(A)            ((_iq7 )(A) >> (8))
#define  _IQ15toIQ6(A)            ((_iq6 )(A) >> (9))
#define  _IQ15toIQ5(A)            ((_iq5 )(A) >> (10))
#define  _IQ15toIQ4(A)            ((_iq4 )(A) >> (11))
#define  _IQ15toIQ3(A)            ((_iq3 )(A) >> (12))
#define  _IQ15toIQ2(A)            ((_iq2 )(A) >> (13))
#define  _IQ15toIQ1(A)            ((_iq1 )(A) >> (14))

/* IQ16 to IQN */
#define _IQ16toIQ30(A)            ((_iq30)(A) << (14))
#define _IQ16toIQ29(A)            ((_iq29)(A) << (13))
#define _IQ16toIQ28(A)            ((_iq28)(A) << (12))
#define _IQ16toIQ27(A)            ((_iq27)(A) << (11))
#define _IQ16toIQ26(A)            ((_iq26)(A) << (10))
#define _IQ16toIQ25(A)            ((_iq25)(A) << (9))
#define _IQ16toIQ24(A)            ((_iq24)(A) << (8))
#define _IQ16toIQ23(A)            ((_iq23)(A) << (7))
#define _IQ16toIQ22(A)            ((_iq22)(A) << (6))
#define _IQ16toIQ21(A)            ((_iq21)(A) << (5))
#define _IQ16toIQ20(A)            ((_iq20)(A) << (4))
#define _IQ16toIQ19(A)            ((_iq19)(A) << (3))
#define _IQ16toIQ18(A)            ((_iq18)(A) << (2))
#define _IQ16toIQ17(A)            ((_iq17)(A) << (1))
#define _IQ16toIQ15(A)            ((_iq15)(A) >> (1))
#define _IQ16toIQ14(A)            ((_iq14)(A) >> (2))
#define _IQ16toIQ13(A)            ((_iq13)(A) >> (3))
#define _IQ16toIQ12(A)            ((_iq12)(A) >> (4))
#define _IQ16toIQ11(A)            ((_iq11)(A) >> (5))
#define _IQ16toIQ10(A)            ((_iq10)(A) >> (6))
#define  _IQ16toIQ9(A)            ((_iq9 )(A) >> (7))
#define  _IQ16toIQ8(A)            ((_iq8 )(A) >> (8))
#define  _IQ16toIQ7(A)            ((_iq7 )(A) >> (9))
#define  _IQ16toIQ6(A)            ((_iq6 )(A) >> (10))
#define  _IQ16toIQ5(A)            ((_iq5 )(A) >> (11))
#define  _IQ16toIQ4(A)            ((_iq4 )(A) >> (12))
#define  _IQ16toIQ3(A)            ((_iq3 )(A) >> (13))
#define  _IQ16toIQ2(A)            ((_iq2 )(A) >> (14))
#define  _IQ16toIQ1(A)            ((_iq1 )(A) >> (15))

/* IQ17 to IQN */
#define _IQ17toIQ30(A)            ((_iq30)(A) << (13))
#define _IQ17toIQ29(A)            ((_iq29)(A) << (12))
#define _IQ17toIQ28(A)            ((_iq28)(A) << (11))
#define _IQ17toIQ27(A)            ((_iq27)(A) << (10))
#define _IQ17toIQ26(A)            ((_iq26)(A) << (9))
#define _IQ17toIQ25(A)            ((_iq25)(A) << (8))
#define _IQ17toIQ24(A)            ((_iq24)(A) << (7))
#define _IQ17toIQ23(A)            ((_iq23)(A) << (6))
#define _IQ17toIQ22(A)            ((_iq22)(A) << (5))
#define _IQ17toIQ21(A)            ((_iq21)(A) << (4))
#define _IQ17toIQ20(A)            ((_iq20)(A) << (3))
#define _IQ17toIQ19(A)            ((_iq19)(A) << (2))
#define _IQ17toIQ18(A)            ((_iq18)(A) << (1))
#define _IQ17toIQ16(A)            ((_iq16)(A) >> (1))
#define _IQ17toIQ15(A)            ((_iq15)(A) >> (2))
#define _IQ17toIQ14(A)            ((_iq14)(A) >> (3))
#define _IQ17toIQ13(A)            ((_iq13)(A) >> (4))
#define _IQ17toIQ12(A)            ((_iq12)(A) >> (5))
#define _IQ17toIQ11(A)            ((_iq11)(A) >> (6))
#define _IQ17toIQ10(A)            ((_iq10)(A) >> (7))
#define  _IQ17toIQ9(A)            ((_iq9 )(A) >> (8))
#define  _IQ17toIQ8(A)            ((_iq8 )(A) >> (9))
#define  _IQ17toIQ7(A)            ((_iq7 )(A) >> (10))
#define  _IQ17toIQ6(A)            ((_iq6 )(A) >> (11))
#define  _IQ17toIQ5(A)            ((_iq5 )(A) >> (12))
#define  _IQ17toIQ4(A)            ((_iq4 )(A) >> (13))
#define  _IQ17toIQ3(A)            ((_iq3 )(A) >> (14))
#define  _IQ17toIQ2(A)            ((_iq2 )(A) >> (15))
#define  _IQ17toIQ1(A)            ((_iq1 )(A) >> (16))

/* IQ18 to IQN */
#define _IQ18toIQ30(A)            ((_iq30)(A) << (12))
#define _IQ18toIQ29(A)            ((_iq29)(A) << (11))
#define _IQ18toIQ28(A)            ((_iq28)(A) << (10))
#define _IQ18toIQ27(A)            ((_iq27)(A) << (9))
#define _IQ18toIQ26(A)            ((_iq26)(A) << (8))
#define _IQ18toIQ25(A)            ((_iq25)(A) << (7))
#define _IQ18toIQ24(A)            ((_iq24)(A) << (6))
#define _IQ18toIQ23(A)            ((_iq23)(A) << (5))
#define _IQ18toIQ22(A)            ((_iq22)(A) << (4))
#define _IQ18toIQ21(A)            ((_iq21)(A) << (3))
#define _IQ18toIQ20(A)            ((_iq20)(A) << (2))
#define _IQ18toIQ19(A)            ((_iq19)(A) << (1))
#define _IQ18toIQ17(A)            ((_iq17)(A) >> (1))
#define _IQ18toIQ16(A)            ((_iq16)(A) >> (2))
#define _IQ18toIQ15(A)            ((_iq15)(A) >> (3))
#define _IQ18toIQ14(A)            ((_iq14)(A) >> (4))
#define _IQ18toIQ13(A)            ((_iq13)(A) >> (5))
#define _IQ18toIQ12(A)            ((_iq12)(A) >> (6))
#define _IQ18toIQ11(A)            ((_iq11)(A) >> (7))
#define _IQ18toIQ10(A)            ((_iq10)(A) >> (8))
#define  _IQ18toIQ9(A)            ((_iq9 )(A) >> (9))
#define  _IQ18toIQ8(A)            ((_iq8 )(A) >> (10))
#define  _IQ18toIQ7(A)            ((_iq7 )(A) >> (11))
#define  _IQ18toIQ6(A)            ((_iq6 )(A) >> (12))
#define  _IQ18toIQ5(A)            ((_iq5 )(A) >> (13))
#define  _IQ18toIQ4(A)            ((_iq4 )(A) >> (14))
#define  _IQ18toIQ3(A)            ((_iq3 )(A) >> (15))
#define  _IQ18toIQ2(A)            ((_iq2 )(A) >> (16))
#define  _IQ18toIQ1(A)            ((_iq1 )(A) >> (17))

/* IQ19 to IQN */
#define _IQ19toIQ30(A)            ((_iq30)(A) << (11))
#define _IQ19toIQ29(A)            ((_iq29)(A) << (10))
#define _IQ19toIQ28(A)            ((_iq28)(A) << (9))
#define _IQ19toIQ27(A)            ((_iq27)(A) << (8))
#define _IQ19toIQ26(A)            ((_iq26)(A) << (7))
#define _IQ19toIQ25(A)            ((_iq25)(A) << (6))
#define _IQ19toIQ24(A)            ((_iq24)(A) << (5))
#define _IQ19toIQ23(A)            ((_iq23)(A) << (4))
#define _IQ19toIQ22(A)            ((_iq22)(A) << (3))
#define _IQ19toIQ21(A)            ((_iq21)(A) << (2))
#define _IQ19toIQ20(A)            ((_iq20)(A) << (1))
#define _IQ19toIQ18(A)            ((_iq18)(A) >> (1))
#define _IQ19toIQ17(A)            ((_iq17)(A) >> (2))
#define _IQ19toIQ16(A)            ((_iq16)(A) >> (3))
#define _IQ19toIQ15(A)            ((_iq15)(A) >> (4))
#define _IQ19toIQ14(A)            ((_iq14)(A) >> (5))
#define _IQ19toIQ13(A)            ((_iq13)(A) >> (6))
#define _IQ19toIQ12(A)            ((_iq12)(A) >> (7))
#define _IQ19toIQ11(A)            ((_iq11)(A) >> (8))
#define _IQ19toIQ10(A)            ((_iq10)(A) >> (9))
#define  _IQ19toIQ9(A)            ((_iq9 )(A) >> (10))
#define  _IQ19toIQ8(A)            ((_iq8 )(A) >> (11))
#define  _IQ19toIQ7(A)            ((_iq7 )(A) >> (12))
#define  _IQ19toIQ6(A)            ((_iq6 )(A) >> (13))
#define  _IQ19toIQ5(A)            ((_iq5 )(A) >> (14))
#define  _IQ19toIQ4(A)            ((_iq4 )(A) >> (15))
#define  _IQ19toIQ3(A)            ((_iq3 )(A) >> (16))
#define  _IQ19toIQ2(A)            ((_iq2 )(A) >> (17))
#define  _IQ19toIQ1(A)            ((_iq1 )(A) >> (18))

/* IQ20 to IQN */
#define _IQ20toIQ30(A)            ((_iq30)(A) << (10))
#define _IQ20toIQ29(A)            ((_iq29)(A) << (9))
#define _IQ20toIQ28(A)            ((_iq28)(A) << (8))
#define _IQ20toIQ27(A)            ((_iq27)(A) << (7))
#define _IQ20toIQ26(A)            ((_iq26)(A) << (6))
#define _IQ20toIQ25(A)            ((_iq25)(A) << (5))
#define _IQ20toIQ24(A)            ((_iq24)(A) << (4))
#define _IQ20toIQ23(A)            ((_iq23)(A) << (3))
#define _IQ20toIQ22(A)            ((_iq22)(A) << (2))
#define _IQ20toIQ21(A)            ((_iq21)(A) << (1))
#define _IQ20toIQ19(A)            ((_iq19)(A) >> (1))
#define _IQ20toIQ18(A)            ((_iq18)(A) >> (2))
#define _IQ20toIQ17(A)            ((_iq17)(A) >> (3))
#define _IQ20toIQ16(A)            ((_iq16)(A) >> (4))
#define _IQ20toIQ15(A)            ((_iq15)(A) >> (5))
#define _IQ20toIQ14(A)            ((_iq14)(A) >> (6))
#define _IQ20toIQ13(A)            ((_iq13)(A) >> (7))
#define _IQ20toIQ12(A)            ((_iq12)(A) >> (8))
#define _IQ20toIQ11(A)            ((_iq11)(A) >> (9))
#define _IQ20toIQ10(A)            ((_iq10)(A) >> (10))
#define  _IQ20toIQ9(A)            ((_iq9 )(A) >> (11))
#define  _IQ20toIQ8(A)            ((_iq8 )(A) >> (12))
#define  _IQ20toIQ7(A)            ((_iq7 )(A) >> (13))
#define  _IQ20toIQ6(A)            ((_iq6 )(A) >> (14))
#define  _IQ20toIQ5(A)            ((_iq5 )(A) >> (15))
#define  _IQ20toIQ4(A)            ((_iq4 )(A) >> (16))
#define  _IQ20toIQ3(A)            ((_iq3 )(A) >> (17))
#define  _IQ20toIQ2(A)            ((_iq2 )(A) >> (18))
#define  _IQ20toIQ1(A)            ((_iq1 )(A) >> (19))

/* IQ21 to IQN */
#define _IQ21toIQ30(A)            ((_iq30)(A) << (9))
#define _IQ21toIQ29(A)            ((_iq29)(A) << (8))
#define _IQ21toIQ28(A)            ((_iq28)(A) << (7))
#define _IQ21toIQ27(A)            ((_iq27)(A) << (6))
#define _IQ21toIQ26(A)            ((_iq26)(A) << (5))
#define _IQ21toIQ25(A)            ((_iq25)(A) << (4))
#define _IQ21toIQ24(A)            ((_iq24)(A) << (3))
#define _IQ21toIQ23(A)            ((_iq23)(A) << (2))
#define _IQ21toIQ22(A)            ((_iq22)(A) << (1))
#define _IQ21toIQ20(A)            ((_iq20)(A) >> (1))
#define _IQ21toIQ19(A)            ((_iq19)(A) >> (2))
#define _IQ21toIQ18(A)            ((_iq18)(A) >> (3))
#define _IQ21toIQ17(A)            ((_iq17)(A) >> (4))
#define _IQ21toIQ16(A)            ((_iq16)(A) >> (5))
#define _IQ21toIQ15(A)            ((_iq15)(A) >> (6))
#define _IQ21toIQ14(A)            ((_iq14)(A) >> (7))
#define _IQ21toIQ13(A)            ((_iq13)(A) >> (8))
#define _IQ21toIQ12(A)            ((_iq12)(A) >> (9))
#define _IQ21toIQ11(A)            ((_iq11)(A) >> (10))
#define _IQ21toIQ10(A)            ((_iq10)(A) >> (11))
#define  _IQ21toIQ9(A)            ((_iq9 )(A) >> (12))
#define  _IQ21toIQ8(A)            ((_iq8 )(A) >> (13))
#define  _IQ21toIQ7(A)            ((_iq7 )(A) >> (14))
#define  _IQ21toIQ6(A)            ((_iq6 )(A) >> (15))
#define  _IQ21toIQ5(A)            ((_iq5 )(A) >> (16))
#define  _IQ21toIQ4(A)            ((_iq4 )(A) >> (17))
#define  _IQ21toIQ3(A)            ((_iq3 )(A) >> (18))
#define  _IQ21toIQ2(A)            ((_iq2 )(A) >> (19))
#define  _IQ21toIQ1(A)            ((_iq1 )(A) >> (20))

/* IQ22 to IQN */
#define _IQ22toIQ30(A)            ((_iq30)(A) << (8))
#define _IQ22toIQ29(A)            ((_iq29)(A) << (7))
#define _IQ22toIQ28(A)            ((_iq28)(A) << (6))
#define _IQ22toIQ27(A)            ((_iq27)(A) << (5))
#define _IQ22toIQ26(A)            ((_iq26)(A) << (4))
#define _IQ22toIQ25(A)            ((_iq25)(A) << (3))
#define _IQ22toIQ24(A)            ((_iq24)(A) << (2))
#define _IQ22toIQ23(A)            ((_iq23)(A) << (1))
#define _IQ22toIQ21(A)            ((_iq21)(A) >> (1))
#define _IQ22toIQ20(A)            ((_iq20)(A) >> (2))
#define _IQ22toIQ19(A)            ((_iq19)(A) >> (3))
#define _IQ22toIQ18(A)            ((_iq18)(A) >> (4))
#define _IQ22toIQ17(A)            ((_iq17)(A) >> (5))
#define _IQ22toIQ16(A)            ((_iq16)(A) >> (6))
#define _IQ22toIQ15(A)            ((_iq15)(A) >> (7))
#define _IQ22toIQ14(A)            ((_iq14)(A) >> (8))
#define _IQ22toIQ13(A)            ((_iq13)(A) >> (9))
#define _IQ22toIQ12(A)            ((_iq12)(A) >> (10))
#define _IQ22toIQ11(A)            ((_iq11)(A) >> (11))
#define _IQ22toIQ10(A)            ((_iq10)(A) >> (12))
#define  _IQ22toIQ9(A)            ((_iq9 )(A) >> (13))
#define  _IQ22toIQ8(A)            ((_iq8 )(A) >> (14))
#define  _IQ22toIQ7(A)            ((_iq7 )(A) >> (15))
#define  _IQ22toIQ6(A)            ((_iq6 )(A) >> (16))
#define  _IQ22toIQ5(A)            ((_iq5 )(A) >> (17))
#define  _IQ22toIQ4(A)            ((_iq4 )(A) >> (18))
#define  _IQ22toIQ3(A)            ((_iq3 )(A) >> (19))
#define  _IQ22toIQ2(A)            ((_iq2 )(A) >> (20))
#define  _IQ22toIQ1(A)            ((_iq1 )(A) >> (21))

/* IQ23 to IQN */
#define _IQ23toIQ30(A)            ((_iq30)(A) << (7))
#define _IQ23toIQ29(A)            ((_iq29)(A) << (6))
#define _IQ23toIQ28(A)            ((_iq28)(A) << (5))
#define _IQ23toIQ27(A)            ((_iq27)(A) << (4))
#define _IQ23toIQ26(A)            ((_iq26)(A) << (3))
#define _IQ23toIQ25(A)            ((_iq25)(A) << (2))
#define _IQ23toIQ24(A)            ((_iq24)(A) << (1))
#define _IQ23toIQ22(A)            ((_iq22)(A) >> (1))
#define _IQ23toIQ21(A)            ((_iq21)(A) >> (2))
#define _IQ23toIQ20(A)            ((_iq20)(A) >> (3))
#define _IQ23toIQ19(A)            ((_iq19)(A) >> (4))
#define _IQ23toIQ18(A)            ((_iq18)(A) >> (5))
#define _IQ23toIQ17(A)            ((_iq17)(A) >> (6))
#define _IQ23toIQ16(A)            ((_iq16)(A) >> (7))
#define _IQ23toIQ15(A)            ((_iq15)(A) >> (8))
#define _IQ23toIQ14(A)            ((_iq14)(A) >> (9))
#define _IQ23toIQ13(A)            ((_iq13)(A) >> (10))
#define _IQ23toIQ12(A)            ((_iq12)(A) >> (11))
#define _IQ23toIQ11(A)            ((_iq11)(A) >> (12))
#define _IQ23toIQ10(A)            ((_iq10)(A) >> (13))
#define  _IQ23toIQ9(A)            ((_iq9 )(A) >> (14))
#define  _IQ23toIQ8(A)            ((_iq8 )(A) >> (15))
#define  _IQ23toIQ7(A)            ((_iq7 )(A) >> (16))
#define  _IQ23toIQ6(A)            ((_iq6 )(A) >> (17))
#define  _IQ23toIQ5(A)            ((_iq5 )(A) >> (18))
#define  _IQ23toIQ4(A)            ((_iq4 )(A) >> (19))
#define  _IQ23toIQ3(A)            ((_iq3 )(A) >> (20))
#define  _IQ23toIQ2(A)            ((_iq2 )(A) >> (21))
#define  _IQ23toIQ1(A)            ((_iq1 )(A) >> (22))

/* IQ24 to IQN */
#define _IQ24toIQ30(A)            ((_iq30)(A) << (6))
#define _IQ24toIQ29(A)            ((_iq29)(A) << (5))
#define _IQ24toIQ28(A)            ((_iq28)(A) << (4))
#define _IQ24toIQ27(A)            ((_iq27)(A) << (3))
#define _IQ24toIQ26(A)            ((_iq26)(A) << (2))
#define _IQ24toIQ25(A)            ((_iq25)(A) << (1))
#define _IQ24toIQ23(A)            ((_iq23)(A) >> (1))
#define _IQ24toIQ22(A)            ((_iq22)(A) >> (2))
#define _IQ24toIQ21(A)            ((_iq21)(A) >> (3))
#define _IQ24toIQ20(A)            ((_iq20)(A) >> (4))
#define _IQ24toIQ19(A)            ((_iq19)(A) >> (5))
#define _IQ24toIQ18(A)            ((_iq18)(A) >> (6))
#define _IQ24toIQ17(A)            ((_iq17)(A) >> (7))
#define _IQ24toIQ16(A)            ((_iq16)(A) >> (8))
#define _IQ24toIQ15(A)            ((_iq15)(A) >> (9))
#define _IQ24toIQ14(A)            ((_iq14)(A) >> (10))
#define _IQ24toIQ13(A)            ((_iq13)(A) >> (11))
#define _IQ24toIQ12(A)            ((_iq12)(A) >> (12))
#define _IQ24toIQ11(A)            ((_iq11)(A) >> (13))
#define _IQ24toIQ10(A)            ((_iq10)(A) >> (14))
#define  _IQ24toIQ9(A)            ((_iq9 )(A) >> (15))
#define  _IQ24toIQ8(A)            ((_iq8 )(A) >> (16))
#define  _IQ24toIQ7(A)            ((_iq7 )(A) >> (17))
#define  _IQ24toIQ6(A)            ((_iq6 )(A) >> (18))
#define  _IQ24toIQ5(A)            ((_iq5 )(A) >> (19))
#define  _IQ24toIQ4(A)            ((_iq4 )(A) >> (20))
#define  _IQ24toIQ3(A)            ((_iq3 )(A) >> (21))
#define  _IQ24toIQ2(A)            ((_iq2 )(A) >> (22))
#define  _IQ24toIQ1(A)            ((_iq1 )(A) >> (23))

/* IQ25 to IQN */
#define _IQ25toIQ30(A)            ((_iq30)(A) << (5))
#define _IQ25toIQ29(A)            ((_iq29)(A) << (4))
#define _IQ25toIQ28(A)            ((_iq28)(A) << (3))
#define _IQ25toIQ27(A)            ((_iq27)(A) << (2))
#define _IQ25toIQ26(A)            ((_iq26)(A) << (1))
#define _IQ25toIQ24(A)            ((_iq24)(A) >> (1))
#define _IQ25toIQ23(A)            ((_iq23)(A) >> (2))
#define _IQ25toIQ22(A)            ((_iq22)(A) >> (3))
#define _IQ25toIQ21(A)            ((_iq21)(A) >> (4))
#define _IQ25toIQ20(A)            ((_iq20)(A) >> (5))
#define _IQ25toIQ19(A)            ((_iq19)(A) >> (6))
#define _IQ25toIQ18(A)            ((_iq18)(A) >> (7))
#define _IQ25toIQ17(A)            ((_iq17)(A) >> (8))
#define _IQ25toIQ16(A)            ((_iq16)(A) >> (9))
#define _IQ25toIQ15(A)            ((_iq15)(A) >> (10))
#define _IQ25toIQ14(A)            ((_iq14)(A) >> (11))
#define _IQ25toIQ13(A)            ((_iq13)(A) >> (12))
#define _IQ25toIQ12(A)            ((_iq12)(A) >> (13))
#define _IQ25toIQ11(A)            ((_iq11)(A) >> (14))
#define _IQ25toIQ10(A)            ((_iq10)(A) >> (15))
#define  _IQ25toIQ9(A)            ((_iq9 )(A) >> (16))
#define  _IQ25toIQ8(A)            ((_iq8 )(A) >> (17))
#define  _IQ25toIQ7(A)            ((_iq7 )(A) >> (18))
#define  _IQ25toIQ6(A)            ((_iq6 )(A) >> (19))
#define  _IQ25toIQ5(A)            ((_iq5 )(A) >> (20))
#define  _IQ25toIQ4(A)            ((_iq4 )(A) >> (21))
#define  _IQ25toIQ3(A)            ((_iq3 )(A) >> (22))
#define  _IQ25toIQ2(A)            ((_iq2 )(A) >> (23))
#define  _IQ25toIQ1(A)            ((_iq1 )(A) >> (24))

/* IQ26 to IQN */
#define _IQ26toIQ30(A)            ((_iq30)(A) << (4))
#define _IQ26toIQ29(A)            ((_iq29)(A) << (3))
#define _IQ26toIQ28(A)            ((_iq28)(A) << (2))
#define _IQ26toIQ27(A)            ((_iq27)(A) << (1))
#define _IQ26toIQ25(A)            ((_iq25)(A) >> (1))
#define _IQ26toIQ24(A)            ((_iq24)(A) >> (2))
#define _IQ26toIQ23(A)            ((_iq23)(A) >> (3))
#define _IQ26toIQ22(A)            ((_iq22)(A) >> (4))
#define _IQ26toIQ21(A)            ((_iq21)(A) >> (5))
#define _IQ26toIQ20(A)            ((_iq20)(A) >> (6))
#define _IQ26toIQ19(A)            ((_iq19)(A) >> (7))
#define _IQ26toIQ18(A)            ((_iq18)(A) >> (8))
#define _IQ26toIQ17(A)            ((_iq17)(A) >> (9))
#define _IQ26toIQ16(A)            ((_iq16)(A) >> (10))
#define _IQ26toIQ15(A)            ((_iq15)(A) >> (11))
#define _IQ26toIQ14(A)            ((_iq14)(A) >> (12))
#define _IQ26toIQ13(A)            ((_iq13)(A) >> (13))
#define _IQ26toIQ12(A)            ((_iq12)(A) >> (14))
#define _IQ26toIQ11(A)            ((_iq11)(A) >> (15))
#define _IQ26toIQ10(A)            ((_iq10)(A) >> (16))
#define  _IQ26toIQ9(A)            ((_iq9 )(A) >> (17))
#define  _IQ26toIQ8(A)            ((_iq8 )(A) >> (18))
#define  _IQ26toIQ7(A)            ((_iq7 )(A) >> (19))
#define  _IQ26toIQ6(A)            ((_iq6 )(A) >> (20))
#define  _IQ26toIQ5(A)            ((_iq5 )(A) >> (21))
#define  _IQ26toIQ4(A)            ((_iq4 )(A) >> (22))
#define  _IQ26toIQ3(A)            ((_iq3 )(A) >> (23))
#define  _IQ26toIQ2(A)            ((_iq2 )(A) >> (24))
#define  _IQ26toIQ1(A)            ((_iq1 )(A) >> (25))

/* IQ27 to IQN */
#define _IQ27toIQ30(A)            ((_iq30)(A) << (3))
#define _IQ27toIQ29(A)            ((_iq29)(A) << (2))
#define _IQ27toIQ28(A)            ((_iq28)(A) << (1))
#define _IQ27toIQ26(A)            ((_iq26)(A) >> (1))
#define _IQ27toIQ25(A)            ((_iq25)(A) >> (2))
#define _IQ27toIQ24(A)            ((_iq24)(A) >> (3))
#define _IQ27toIQ23(A)            ((_iq23)(A) >> (4))
#define _IQ27toIQ22(A)            ((_iq22)(A) >> (5))
#define _IQ27toIQ21(A)            ((_iq21)(A) >> (6))
#define _IQ27toIQ20(A)            ((_iq20)(A) >> (7))
#define _IQ27toIQ19(A)            ((_iq19)(A) >> (8))
#define _IQ27toIQ18(A)            ((_iq18)(A) >> (9))
#define _IQ27toIQ17(A)            ((_iq17)(A) >> (10))
#define _IQ27toIQ16(A)            ((_iq16)(A) >> (11))
#define _IQ27toIQ15(A)            ((_iq15)(A) >> (12))
#define _IQ27toIQ14(A)            ((_iq14)(A) >> (13))
#define _IQ27toIQ13(A)            ((_iq13)(A) >> (14))
#define _IQ27toIQ12(A)            ((_iq12)(A) >> (15))
#define _IQ27toIQ11(A)            ((_iq11)(A) >> (16))
#define _IQ27toIQ10(A)            ((_iq10)(A) >> (17))
#define  _IQ27toIQ9(A)            ((_iq9 )(A) >> (18))
#define  _IQ27toIQ8(A)            ((_iq8 )(A) >> (19))
#define  _IQ27toIQ7(A)            ((_iq7 )(A) >> (20))
#define  _IQ27toIQ6(A)            ((_iq6 )(A) >> (21))
#define  _IQ27toIQ5(A)            ((_iq5 )(A) >> (22))
#define  _IQ27toIQ4(A)            ((_iq4 )(A) >> (23))
#define  _IQ27toIQ3(A)            ((_iq3 )(A) >> (24))
#define  _IQ27toIQ2(A)            ((_iq2 )(A) >> (25))
#define  _IQ27toIQ1(A)            ((_iq1 )(A) >> (26))

/* IQ28 to IQN */
#define _IQ28toIQ30(A)            ((_iq30)(A) << (2))
#define _IQ28toIQ29(A)            ((_iq29)(A) << (1))
#define _IQ28toIQ27(A)            ((_iq27)(A) >> (1))
#define _IQ28toIQ26(A)            ((_iq26)(A) >> (2))
#define _IQ28toIQ25(A)            ((_iq25)(A) >> (3))
#define _IQ28toIQ24(A)            ((_iq24)(A) >> (4))
#define _IQ28toIQ23(A)            ((_iq23)(A) >> (5))
#define _IQ28toIQ22(A)            ((_iq22)(A) >> (6))
#define _IQ28toIQ21(A)            ((_iq21)(A) >> (7))
#define _IQ28toIQ20(A)            ((_iq20)(A) >> (8))
#define _IQ28toIQ19(A)            ((_iq19)(A) >> (9))
#define _IQ28toIQ18(A)            ((_iq18)(A) >> (10))
#define _IQ28toIQ17(A)            ((_iq17)(A) >> (11))
#define _IQ28toIQ16(A)            ((_iq16)(A) >> (12))
#define _IQ28toIQ15(A)            ((_iq15)(A) >> (13))
#define _IQ28toIQ14(A)            ((_iq14)(A) >> (14))
#define _IQ28toIQ13(A)            ((_iq13)(A) >> (15))
#define _IQ28toIQ12(A)            ((_iq12)(A) >> (16))
#define _IQ28toIQ11(A)            ((_iq11)(A) >> (17))
#define _IQ28toIQ10(A)            ((_iq10)(A) >> (18))
#define  _IQ28toIQ9(A)            ((_iq9 )(A) >> (19))
#define  _IQ28toIQ8(A)            ((_iq8 )(A) >> (20))
#define  _IQ28toIQ7(A)            ((_iq7 )(A) >> (21))
#define  _IQ28toIQ6(A)            ((_iq6 )(A) >> (22))
#define  _IQ28toIQ5(A)            ((_iq5 )(A) >> (23))
#define  _IQ28toIQ4(A)            ((_iq4 )(A) >> (24))
#define  _IQ28toIQ3(A)            ((_iq3 )(A) >> (25))
#define  _IQ28toIQ2(A)            ((_iq2 )(A) >> (26))
#define  _IQ28toIQ1(A)            ((_iq1 )(A) >> (27))

/* IQ29 to IQN */
#define _IQ29toIQ30(A)            ((_iq30)(A) << (1))
#define _IQ29toIQ28(A)            ((_iq28)(A) >> (1))
#define _IQ29toIQ27(A)            ((_iq27)(A) >> (2))
#define _IQ29toIQ26(A)            ((_iq26)(A) >> (3))
#define _IQ29toIQ25(A)            ((_iq25)(A) >> (4))
#define _IQ29toIQ24(A)            ((_iq24)(A) >> (5))
#define _IQ29toIQ23(A)            ((_iq23)(A) >> (6))
#define _IQ29toIQ22(A)            ((_iq22)(A) >> (7))
#define _IQ29toIQ21(A)            ((_iq21)(A) >> (8))
#define _IQ29toIQ20(A)            ((_iq20)(A) >> (9))
#define _IQ29toIQ19(A)            ((_iq19)(A) >> (10))
#define _IQ29toIQ18(A)            ((_iq18)(A) >> (11))
#define _IQ29toIQ17(A)            ((_iq17)(A) >> (12))
#define _IQ29toIQ16(A)            ((_iq16)(A) >> (13))
#define _IQ29toIQ15(A)            ((_iq15)(A) >> (14))
#define _IQ29toIQ14(A)            ((_iq14)(A) >> (15))
#define _IQ29toIQ13(A)            ((_iq13)(A) >> (16))
#define _IQ29toIQ12(A)            ((_iq12)(A) >> (17))
#define _IQ29toIQ11(A)            ((_iq11)(A) >> (18))
#define _IQ29toIQ10(A)            ((_iq10)(A) >> (19))
#define  _IQ29toIQ9(A)            ((_iq9 )(A) >> (20))
#define  _IQ29toIQ8(A)            ((_iq8 )(A) >> (21))
#define  _IQ29toIQ7(A)            ((_iq7 )(A) >> (22))
#define  _IQ29toIQ6(A)            ((_iq6 )(A) >> (23))
#define  _IQ29toIQ5(A)            ((_iq5 )(A) >> (24))
#define  _IQ29toIQ4(A)            ((_iq4 )(A) >> (25))
#define  _IQ29toIQ3(A)            ((_iq3 )(A) >> (26))
#define  _IQ29toIQ2(A)            ((_iq2 )(A) >> (27))
#define  _IQ29toIQ1(A)            ((_iq1 )(A) >> (28))

/* IQ30 to IQN */
#define _IQ30toIQ29(A)            ((_iq29)(A) >> (1))
#define _IQ30toIQ28(A)            ((_iq28)(A) >> (2))
#define _IQ30toIQ27(A)            ((_iq27)(A) >> (3))
#define _IQ30toIQ26(A)            ((_iq26)(A) >> (4))
#define _IQ30toIQ25(A)            ((_iq25)(A) >> (5))
#define _IQ30toIQ24(A)            ((_iq24)(A) >> (6))
#define _IQ30toIQ23(A)            ((_iq23)(A) >> (7))
#define _IQ30toIQ22(A)            ((_iq22)(A) >> (8))
#define _IQ30toIQ21(A)            ((_iq21)(A) >> (9))
#define _IQ30toIQ20(A)            ((_iq20)(A) >> (10))
#define _IQ30toIQ19(A)            ((_iq19)(A) >> (11))
#define _IQ30toIQ18(A)            ((_iq18)(A) >> (12))
#define _IQ30toIQ17(A)            ((_iq17)(A) >> (13))
#define _IQ30toIQ16(A)            ((_iq16)(A) >> (14))
#define _IQ30toIQ15(A)            ((_iq15)(A) >> (15))
#define _IQ30toIQ14(A)            ((_iq14)(A) >> (16))
#define _IQ30toIQ13(A)            ((_iq13)(A) >> (17))
#define _IQ30toIQ12(A)            ((_iq12)(A) >> (18))
#define _IQ30toIQ11(A)            ((_iq11)(A) >> (19))
#define _IQ30toIQ10(A)            ((_iq10)(A) >> (20))
#define  _IQ30toIQ9(A)            ((_iq9 )(A) >> (21))
#define  _IQ30toIQ8(A)            ((_iq8 )(A) >> (22))
#define  _IQ30toIQ7(A)            ((_iq7 )(A) >> (23))
#define  _IQ30toIQ6(A)            ((_iq6 )(A) >> (24))
#define  _IQ30toIQ5(A)            ((_iq5 )(A) >> (25))
#define  _IQ30toIQ4(A)            ((_iq4 )(A) >> (26))
#define  _IQ30toIQ3(A)            ((_iq3 )(A) >> (27))
#define  _IQ30toIQ2(A)            ((_iq2 )(A) >> (28))
#define  _IQ30toIQ1(A)            ((_iq1 )(A) >> (29))

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
//*****************************************************************************
//
// Converts a number between IQ format and 16-bit Qn format.
//
//*****************************************************************************
#if (GLOBAL_IQ >= 15)
#define _IQtoQ15(A)             ((int32_t)(A) >> (GLOBAL_IQ - 15))
#define _Q15toIQ(A)             ((_iq15)(A) << (GLOBAL_IQ - 15))
#else
#define _IQtoQ15(A)             ((int32_t)(A) << (15 - GLOBAL_IQ))
#define _Q15toIQ(A)             ((_iq15)(A) >> (15 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 14)
#define _IQtoQ14(A)             ((int32_t)(A) >> (GLOBAL_IQ - 14))
#define _Q14toIQ(A)             ((_iq14)(A) << (GLOBAL_IQ - 14))
#else
#define _IQtoQ14(A)             ((int32_t)(A) << (14 - GLOBAL_IQ))
#define _Q14toIQ(A)             ((_iq14)(A) >> (14 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 13)
#define _IQtoQ13(A)             ((int32_t)(A) >> (GLOBAL_IQ - 13))
#define _Q13toIQ(A)             ((_iq13)(A) << (GLOBAL_IQ - 13))
#else
#define _IQtoQ13(A)             ((int32_t)(A) << (13 - GLOBAL_IQ))
#define _Q13toIQ(A)             ((_iq13)(A) >> (13 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 12)
#define _IQtoQ12(A)             ((int32_t)(A) >> (GLOBAL_IQ - 12))
#define _Q12toIQ(A)             ((_iq12)(A) << (GLOBAL_IQ - 12))
#else
#define _IQtoQ12(A)             ((int32_t)(A) << (12 - GLOBAL_IQ))
#define _Q12toIQ(A)             ((_iq12)(A) >> (12 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 11)
#define _IQtoQ11(A)             ((int32_t)(A) >> (GLOBAL_IQ - 11))
#define _Q11toIQ(A)             ((_iq11)(A) << (GLOBAL_IQ - 11))
#else
#define _IQtoQ11(A)             ((int32_t)(A) << (11 - GLOBAL_IQ))
#define _Q11toIQ(A)             ((_iq11)(A) >> (11 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 10)
#define _IQtoQ10(A)             ((int32_t)(A) >> (GLOBAL_IQ - 10))
#define _Q10toIQ(A)             ((_iq10)(A) << (GLOBAL_IQ - 10))
#else
#define _IQtoQ10(A)             ((int32_t)(A) << (10 - GLOBAL_IQ))
#define _Q10toIQ(A)             ((_iq10)(A) >> (10 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 9)
#define _IQtoQ9(A)              ((int32_t)(A) >> (GLOBAL_IQ - 9))
#define _Q9toIQ(A)              ((_iq9)(A) << (GLOBAL_IQ - 9))
#else
#define _IQtoQ9(A)              ((int32_t)(A) << (9 - GLOBAL_IQ))
#define _Q9toIQ(A)              ((_iq9)(A) >> (9 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 8)
#define _IQtoQ8(A)              ((int32_t)(A) >> (GLOBAL_IQ - 8))
#define _Q8toIQ(A)              ((_iq8)(A) << (GLOBAL_IQ - 8))
#else
#define _IQtoQ8(A)              ((int32_t)(A) << (8 - GLOBAL_IQ))
#define _Q8toIQ(A)              ((_iq8)(A) >> (8 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 7)
#define _IQtoQ7(A)              ((int32_t)(A) >> (GLOBAL_IQ - 7))
#define _Q7toIQ(A)              ((_iq7)(A) << (GLOBAL_IQ - 7))
#else
#define _IQtoQ7(A)              ((int32_t)(A) << (7 - GLOBAL_IQ))
#define _Q7toIQ(A)              ((_iq7)(A) >> (7 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 6)
#define _IQtoQ6(A)              ((int32_t)(A) >> (GLOBAL_IQ - 6))
#define _Q6toIQ(A)              ((_iq6)(A) << (GLOBAL_IQ - 6))
#else
#define _IQtoQ6(A)              ((int32_t)(A) << (6 - GLOBAL_IQ))
#define _Q6toIQ(A)              ((_iq6)(A) >> (6 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 5)
#define _IQtoQ5(A)              ((int32_t)(A) >> (GLOBAL_IQ - 5))
#define _Q5toIQ(A)              ((_iq5)(A) << (GLOBAL_IQ - 5))
#else
#define _IQtoQ5(A)              ((int32_t)(A) << (5 - GLOBAL_IQ))
#define _Q5toIQ(A)              ((_iq5)(A) >> (5 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 4)
#define _IQtoQ4(A)              ((int32_t)(A) >> (GLOBAL_IQ - 4))
#define _Q4toIQ(A)              ((_iq4)(A) << (GLOBAL_IQ - 4))
#else
#define _IQtoQ4(A)              ((int32_t)(A) << (4 - GLOBAL_IQ))
#define _Q4toIQ(A)              ((_iq4)(A) >> (4 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 3)
#define _IQtoQ3(A)              ((int32_t)(A) >> (GLOBAL_IQ - 3))
#define _Q3toIQ(A)              ((_iq3)(A) << (GLOBAL_IQ - 3))
#else
#define _IQtoQ3(A)              ((int32_t)(A) << (3 - GLOBAL_IQ))
#define _Q3toIQ(A)              ((_iq3)(A) >> (3 - GLOBAL_IQ))
#endif

#if (GLOBAL_IQ >= 2)
#define _IQtoQ2(A)              ((int32_t)(A) >> (GLOBAL_IQ - 2))
#define _Q2toIQ(A)              ((_iq2)(A) << (GLOBAL_IQ - 2))
#else
#define _IQtoQ2(A)              ((int32_t)(A) << (2 - GLOBAL_IQ))
#define _Q2toIQ(A)              ((_iq2)(A) >> (2 - GLOBAL_IQ))
#endif

#define _IQtoQ1(A)              ((int32_t)(A) >> (GLOBAL_IQ - 1))
#define _Q1toIQ(A)              ((_iq1)(A) << (GLOBAL_IQ - 1))

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

//*****************************************************************************
//
// Multiplies two IQ numbers.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30mpy(_iq30 A, _iq30 B);
extern _iq29 _IQ29mpy(_iq29 A, _iq29 B);
extern _iq28 _IQ28mpy(_iq28 A, _iq28 B);
extern _iq27 _IQ27mpy(_iq27 A, _iq27 B);
extern _iq26 _IQ26mpy(_iq26 A, _iq26 B);
extern _iq25 _IQ25mpy(_iq25 A, _iq25 B);
extern _iq24 _IQ24mpy(_iq24 A, _iq24 B);
extern _iq23 _IQ23mpy(_iq23 A, _iq23 B);
extern _iq22 _IQ22mpy(_iq22 A, _iq22 B);
extern _iq21 _IQ21mpy(_iq21 A, _iq21 B);
extern _iq20 _IQ20mpy(_iq20 A, _iq20 B);
extern _iq19 _IQ19mpy(_iq19 A, _iq19 B);
extern _iq18 _IQ18mpy(_iq18 A, _iq18 B);
extern _iq17 _IQ17mpy(_iq17 A, _iq17 B);
extern _iq16 _IQ16mpy(_iq16 A, _iq16 B);
extern _iq15 _IQ15mpy(_iq15 A, _iq15 B);
extern _iq14 _IQ14mpy(_iq14 A, _iq14 B);
extern _iq13 _IQ13mpy(_iq13 A, _iq13 B);
extern _iq12 _IQ12mpy(_iq12 A, _iq12 B);
extern _iq11 _IQ11mpy(_iq11 A, _iq11 B);
extern _iq10 _IQ10mpy(_iq10 A, _iq10 B);
extern _iq9 _IQ9mpy(_iq9 A, _iq9 B);
extern _iq8 _IQ8mpy(_iq8 A, _iq8 B);
extern _iq7 _IQ7mpy(_iq7 A, _iq7 B);
extern _iq6 _IQ6mpy(_iq6 A, _iq6 B);
extern _iq5 _IQ5mpy(_iq5 A, _iq5 B);
extern _iq4 _IQ4mpy(_iq4 A, _iq4 B);
extern _iq3 _IQ3mpy(_iq3 A, _iq3 B);
extern _iq2 _IQ2mpy(_iq2 A, _iq2 B);
extern _iq1 _IQ1mpy(_iq1 A, _iq1 B);

//*****************************************************************************
//
// MathACL Repeat Operation: repeats same settings as last call (same IQ value, same operation).
// This version is only compatible with IQmpy and IQdiv.
//
//*****************************************************************************
#if ((defined (__IQMATH_USE_MATHACL__)) && (defined (__MSPM0_HAS_MATHACL__)))
extern int32_t _IQrepeat(int32_t A, int32_t B);
#endif


#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Multiplies two global IQ format numbers.
 *
 * @param A               Global IQ format number to be multiplied.
 * @param B               Global IQ format number to be multiplied.
 *
 * @return                Global IQ type result of multiplication.
 */
#if GLOBAL_IQ == 30
#define _IQmpy(A, B)            _IQ30mpy(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQmpy(A, B)            _IQ29mpy(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQmpy(A, B)            _IQ28mpy(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQmpy(A, B)            _IQ27mpy(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQmpy(A, B)            _IQ26mpy(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQmpy(A, B)            _IQ25mpy(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQmpy(A, B)            _IQ24mpy(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQmpy(A, B)            _IQ23mpy(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQmpy(A, B)            _IQ22mpy(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQmpy(A, B)            _IQ21mpy(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQmpy(A, B)            _IQ20mpy(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQmpy(A, B)            _IQ19mpy(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQmpy(A, B)            _IQ18mpy(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQmpy(A, B)            _IQ17mpy(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQmpy(A, B)            _IQ16mpy(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQmpy(A, B)            _IQ15mpy(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQmpy(A, B)            _IQ14mpy(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQmpy(A, B)            _IQ13mpy(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQmpy(A, B)            _IQ12mpy(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQmpy(A, B)            _IQ11mpy(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQmpy(A, B)            _IQ10mpy(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQmpy(A, B)            _IQ9mpy(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQmpy(A, B)            _IQ8mpy(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQmpy(A, B)            _IQ7mpy(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQmpy(A, B)            _IQ6mpy(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQmpy(A, B)            _IQ5mpy(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQmpy(A, B)            _IQ4mpy(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQmpy(A, B)            _IQ3mpy(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQmpy(A, B)            _IQ2mpy(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQmpy(A, B)            _IQ1mpy(A, B)
#endif

//*****************************************************************************
//
// Multiplies two IQ numbers, with rounding.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30rmpy(_iq30 A, _iq30 B);
extern _iq29 _IQ29rmpy(_iq29 A, _iq29 B);
extern _iq28 _IQ28rmpy(_iq28 A, _iq28 B);
extern _iq27 _IQ27rmpy(_iq27 A, _iq27 B);
extern _iq26 _IQ26rmpy(_iq26 A, _iq26 B);
extern _iq25 _IQ25rmpy(_iq25 A, _iq25 B);
extern _iq24 _IQ24rmpy(_iq24 A, _iq24 B);
extern _iq23 _IQ23rmpy(_iq23 A, _iq23 B);
extern _iq22 _IQ22rmpy(_iq22 A, _iq22 B);
extern _iq21 _IQ21rmpy(_iq21 A, _iq21 B);
extern _iq20 _IQ20rmpy(_iq20 A, _iq20 B);
extern _iq19 _IQ19rmpy(_iq19 A, _iq19 B);
extern _iq18 _IQ18rmpy(_iq18 A, _iq18 B);
extern _iq17 _IQ17rmpy(_iq17 A, _iq17 B);
extern _iq16 _IQ16rmpy(_iq16 A, _iq16 B);
extern _iq15 _IQ15rmpy(_iq15 A, _iq15 B);
extern _iq14 _IQ14rmpy(_iq14 A, _iq14 B);
extern _iq13 _IQ13rmpy(_iq13 A, _iq13 B);
extern _iq12 _IQ12rmpy(_iq12 A, _iq12 B);
extern _iq11 _IQ11rmpy(_iq11 A, _iq11 B);
extern _iq10 _IQ10rmpy(_iq10 A, _iq10 B);
extern _iq9 _IQ9rmpy(_iq9 A, _iq9 B);
extern _iq8 _IQ8rmpy(_iq8 A, _iq8 B);
extern _iq7 _IQ7rmpy(_iq7 A, _iq7 B);
extern _iq6 _IQ6rmpy(_iq6 A, _iq6 B);
extern _iq5 _IQ5rmpy(_iq5 A, _iq5 B);
extern _iq4 _IQ4rmpy(_iq4 A, _iq4 B);
extern _iq3 _IQ3rmpy(_iq3 A, _iq3 B);
extern _iq2 _IQ2rmpy(_iq2 A, _iq2 B);
extern _iq1 _IQ1rmpy(_iq1 A, _iq1 B);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Multiplies two global IQ format numbers, with rounding
 *
 * @param A               Global IQ format number to be multiplied.
 * @param B               Global IQ format number to be multiplied.
 *
 * @return                Global IQ type result of multiplication.
 */
#if GLOBAL_IQ == 30
#define _IQrmpy(A, B)           _IQ30rmpy(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQrmpy(A, B)           _IQ29rmpy(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQrmpy(A, B)           _IQ28rmpy(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQrmpy(A, B)           _IQ27rmpy(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQrmpy(A, B)           _IQ26rmpy(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQrmpy(A, B)           _IQ25rmpy(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQrmpy(A, B)           _IQ24rmpy(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQrmpy(A, B)           _IQ23rmpy(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQrmpy(A, B)           _IQ22rmpy(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQrmpy(A, B)           _IQ21rmpy(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQrmpy(A, B)           _IQ20rmpy(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQrmpy(A, B)           _IQ19rmpy(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQrmpy(A, B)           _IQ18rmpy(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQrmpy(A, B)           _IQ17rmpy(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQrmpy(A, B)           _IQ16rmpy(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQrmpy(A, B)           _IQ15rmpy(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQrmpy(A, B)           _IQ14rmpy(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQrmpy(A, B)           _IQ13rmpy(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQrmpy(A, B)           _IQ12rmpy(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQrmpy(A, B)           _IQ11rmpy(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQrmpy(A, B)           _IQ10rmpy(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQrmpy(A, B)           _IQ9rmpy(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQrmpy(A, B)           _IQ8rmpy(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQrmpy(A, B)           _IQ7rmpy(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQrmpy(A, B)           _IQ6rmpy(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQrmpy(A, B)           _IQ5rmpy(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQrmpy(A, B)           _IQ4rmpy(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQrmpy(A, B)           _IQ3rmpy(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQrmpy(A, B)           _IQ2rmpy(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQrmpy(A, B)           _IQ1rmpy(A, B)
#endif

//*****************************************************************************
//
// Multiplies two IQ numbers, with rounding and saturation.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30rsmpy(_iq30 A, _iq30 B);
extern _iq29 _IQ29rsmpy(_iq29 A, _iq29 B);
extern _iq28 _IQ28rsmpy(_iq28 A, _iq28 B);
extern _iq27 _IQ27rsmpy(_iq27 A, _iq27 B);
extern _iq26 _IQ26rsmpy(_iq26 A, _iq26 B);
extern _iq25 _IQ25rsmpy(_iq25 A, _iq25 B);
extern _iq24 _IQ24rsmpy(_iq24 A, _iq24 B);
extern _iq23 _IQ23rsmpy(_iq23 A, _iq23 B);
extern _iq22 _IQ22rsmpy(_iq22 A, _iq22 B);
extern _iq21 _IQ21rsmpy(_iq21 A, _iq21 B);
extern _iq20 _IQ20rsmpy(_iq20 A, _iq20 B);
extern _iq19 _IQ19rsmpy(_iq19 A, _iq19 B);
extern _iq18 _IQ18rsmpy(_iq18 A, _iq18 B);
extern _iq17 _IQ17rsmpy(_iq17 A, _iq17 B);
extern _iq16 _IQ16rsmpy(_iq16 A, _iq16 B);
extern _iq15 _IQ15rsmpy(_iq15 A, _iq15 B);
extern _iq14 _IQ14rsmpy(_iq14 A, _iq14 B);
extern _iq13 _IQ13rsmpy(_iq13 A, _iq13 B);
extern _iq12 _IQ12rsmpy(_iq12 A, _iq12 B);
extern _iq11 _IQ11rsmpy(_iq11 A, _iq11 B);
extern _iq10 _IQ10rsmpy(_iq10 A, _iq10 B);
extern _iq9 _IQ9rsmpy(_iq9 A, _iq9 B);
extern _iq8 _IQ8rsmpy(_iq8 A, _iq8 B);
extern _iq7 _IQ7rsmpy(_iq7 A, _iq7 B);
extern _iq6 _IQ6rsmpy(_iq6 A, _iq6 B);
extern _iq5 _IQ5rsmpy(_iq5 A, _iq5 B);
extern _iq4 _IQ4rsmpy(_iq4 A, _iq4 B);
extern _iq3 _IQ3rsmpy(_iq3 A, _iq3 B);
extern _iq2 _IQ2rsmpy(_iq2 A, _iq2 B);
extern _iq1 _IQ1rsmpy(_iq1 A, _iq1 B);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Multiplies two global IQ format numbers, with rounding and saturation.
 *
 * @param A               Global IQ format number to be multiplied.
 * @param B               Global IQ format number to be multiplied.
 *
 * @return                Global IQ type result of multiplication.
 */
#if GLOBAL_IQ == 30
#define _IQrsmpy(A, B)          _IQ30rsmpy(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQrsmpy(A, B)          _IQ29rsmpy(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQrsmpy(A, B)          _IQ28rsmpy(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQrsmpy(A, B)          _IQ27rsmpy(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQrsmpy(A, B)          _IQ26rsmpy(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQrsmpy(A, B)          _IQ25rsmpy(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQrsmpy(A, B)          _IQ24rsmpy(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQrsmpy(A, B)          _IQ23rsmpy(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQrsmpy(A, B)          _IQ22rsmpy(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQrsmpy(A, B)          _IQ21rsmpy(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQrsmpy(A, B)          _IQ20rsmpy(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQrsmpy(A, B)          _IQ19rsmpy(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQrsmpy(A, B)          _IQ18rsmpy(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQrsmpy(A, B)          _IQ17rsmpy(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQrsmpy(A, B)          _IQ16rsmpy(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQrsmpy(A, B)          _IQ15rsmpy(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQrsmpy(A, B)          _IQ14rsmpy(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQrsmpy(A, B)          _IQ13rsmpy(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQrsmpy(A, B)          _IQ12rsmpy(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQrsmpy(A, B)          _IQ11rsmpy(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQrsmpy(A, B)          _IQ10rsmpy(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQrsmpy(A, B)          _IQ9rsmpy(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQrsmpy(A, B)          _IQ8rsmpy(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQrsmpy(A, B)          _IQ7rsmpy(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQrsmpy(A, B)          _IQ6rsmpy(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQrsmpy(A, B)          _IQ5rsmpy(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQrsmpy(A, B)          _IQ4rsmpy(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQrsmpy(A, B)          _IQ3rsmpy(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQrsmpy(A, B)          _IQ2rsmpy(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQrsmpy(A, B)          _IQ1rsmpy(A, B)
#endif

//*****************************************************************************
//
// Divides two IQ numbers.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30div(_iq30 A, _iq30 B);
extern _iq29 _IQ29div(_iq29 A, _iq29 B);
extern _iq28 _IQ28div(_iq28 A, _iq28 B);
extern _iq27 _IQ27div(_iq27 A, _iq27 B);
extern _iq26 _IQ26div(_iq26 A, _iq26 B);
extern _iq25 _IQ25div(_iq25 A, _iq25 B);
extern _iq24 _IQ24div(_iq24 A, _iq24 B);
extern _iq23 _IQ23div(_iq23 A, _iq23 B);
extern _iq22 _IQ22div(_iq22 A, _iq22 B);
extern _iq21 _IQ21div(_iq21 A, _iq21 B);
extern _iq20 _IQ20div(_iq20 A, _iq20 B);
extern _iq19 _IQ19div(_iq19 A, _iq19 B);
extern _iq18 _IQ18div(_iq18 A, _iq18 B);
extern _iq17 _IQ17div(_iq17 A, _iq17 B);
extern _iq16 _IQ16div(_iq16 A, _iq16 B);
extern _iq15 _IQ15div(_iq15 A, _iq15 B);
extern _iq14 _IQ14div(_iq14 A, _iq14 B);
extern _iq13 _IQ13div(_iq13 A, _iq13 B);
extern _iq12 _IQ12div(_iq12 A, _iq12 B);
extern _iq11 _IQ11div(_iq11 A, _iq11 B);
extern _iq10 _IQ10div(_iq10 A, _iq10 B);
extern _iq9 _IQ9div(_iq9 A, _iq9 B);
extern _iq8 _IQ8div(_iq8 A, _iq8 B);
extern _iq7 _IQ7div(_iq7 A, _iq7 B);
extern _iq6 _IQ6div(_iq6 A, _iq6 B);
extern _iq5 _IQ5div(_iq5 A, _iq5 B);
extern _iq4 _IQ4div(_iq4 A, _iq4 B);
extern _iq3 _IQ3div(_iq3 A, _iq3 B);
extern _iq2 _IQ2div(_iq2 A, _iq2 B);
extern _iq1 _IQ1div(_iq1 A, _iq1 B);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Divides two global IQ format numbers.
 *
 * @param A               Global IQ format numerator to be divided.
 * @param B               Global IQ format denominator to divide by.
 *
 * @return                Global IQ type result of division.
 */
#if GLOBAL_IQ == 30
#define _IQdiv(A, B)            _IQ30div(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQdiv(A, B)            _IQ29div(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQdiv(A, B)            _IQ28div(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQdiv(A, B)            _IQ27div(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQdiv(A, B)            _IQ26div(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQdiv(A, B)            _IQ25div(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQdiv(A, B)            _IQ24div(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQdiv(A, B)            _IQ23div(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQdiv(A, B)            _IQ22div(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQdiv(A, B)            _IQ21div(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQdiv(A, B)            _IQ20div(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQdiv(A, B)            _IQ19div(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQdiv(A, B)            _IQ18div(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQdiv(A, B)            _IQ17div(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQdiv(A, B)            _IQ16div(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQdiv(A, B)            _IQ15div(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQdiv(A, B)            _IQ14div(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQdiv(A, B)            _IQ13div(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQdiv(A, B)            _IQ12div(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQdiv(A, B)            _IQ11div(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQdiv(A, B)            _IQ10div(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQdiv(A, B)            _IQ9div(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQdiv(A, B)            _IQ8div(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQdiv(A, B)            _IQ7div(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQdiv(A, B)            _IQ6div(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQdiv(A, B)            _IQ5div(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQdiv(A, B)            _IQ4div(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQdiv(A, B)            _IQ3div(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQdiv(A, B)            _IQ2div(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQdiv(A, B)            _IQ1div(A, B)
#endif

//*****************************************************************************
//
// Computes the sin of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq29 _IQ29sin(_iq29 A);
extern _iq28 _IQ28sin(_iq28 A);
extern _iq27 _IQ27sin(_iq27 A);
extern _iq26 _IQ26sin(_iq26 A);
extern _iq25 _IQ25sin(_iq25 A);
extern _iq24 _IQ24sin(_iq24 A);
extern _iq23 _IQ23sin(_iq23 A);
extern _iq22 _IQ22sin(_iq22 A);
extern _iq21 _IQ21sin(_iq21 A);
extern _iq20 _IQ20sin(_iq20 A);
extern _iq19 _IQ19sin(_iq19 A);
extern _iq18 _IQ18sin(_iq18 A);
extern _iq17 _IQ17sin(_iq17 A);
extern _iq16 _IQ16sin(_iq16 A);
extern _iq15 _IQ15sin(_iq15 A);
extern _iq14 _IQ14sin(_iq14 A);
extern _iq13 _IQ13sin(_iq13 A);
extern _iq12 _IQ12sin(_iq12 A);
extern _iq11 _IQ11sin(_iq11 A);
extern _iq10 _IQ10sin(_iq10 A);
extern _iq9 _IQ9sin(_iq9 A);
extern _iq8 _IQ8sin(_iq8 A);
extern _iq7 _IQ7sin(_iq7 A);
extern _iq6 _IQ6sin(_iq6 A);
extern _iq5 _IQ5sin(_iq5 A);
extern _iq4 _IQ4sin(_iq4 A);
extern _iq3 _IQ3sin(_iq3 A);
extern _iq2 _IQ2sin(_iq2 A);
extern _iq1 _IQ1sin(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the sine of a global IQ format input, in radians.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of sine operation.
 */
#if GLOBAL_IQ == 29
#define _IQsin(A)               _IQ29sin(A)
#endif
#if GLOBAL_IQ == 28
#define _IQsin(A)               _IQ28sin(A)
#endif
#if GLOBAL_IQ == 27
#define _IQsin(A)               _IQ27sin(A)
#endif
#if GLOBAL_IQ == 26
#define _IQsin(A)               _IQ26sin(A)
#endif
#if GLOBAL_IQ == 25
#define _IQsin(A)               _IQ25sin(A)
#endif
#if GLOBAL_IQ == 24
#define _IQsin(A)               _IQ24sin(A)
#endif
#if GLOBAL_IQ == 23
#define _IQsin(A)               _IQ23sin(A)
#endif
#if GLOBAL_IQ == 22
#define _IQsin(A)               _IQ22sin(A)
#endif
#if GLOBAL_IQ == 21
#define _IQsin(A)               _IQ21sin(A)
#endif
#if GLOBAL_IQ == 20
#define _IQsin(A)               _IQ20sin(A)
#endif
#if GLOBAL_IQ == 19
#define _IQsin(A)               _IQ19sin(A)
#endif
#if GLOBAL_IQ == 18
#define _IQsin(A)               _IQ18sin(A)
#endif
#if GLOBAL_IQ == 17
#define _IQsin(A)               _IQ17sin(A)
#endif
#if GLOBAL_IQ == 16
#define _IQsin(A)               _IQ16sin(A)
#endif
#if GLOBAL_IQ == 15
#define _IQsin(A)               _IQ15sin(A)
#endif
#if GLOBAL_IQ == 14
#define _IQsin(A)               _IQ14sin(A)
#endif
#if GLOBAL_IQ == 13
#define _IQsin(A)               _IQ13sin(A)
#endif
#if GLOBAL_IQ == 12
#define _IQsin(A)               _IQ12sin(A)
#endif
#if GLOBAL_IQ == 11
#define _IQsin(A)               _IQ11sin(A)
#endif
#if GLOBAL_IQ == 10
#define _IQsin(A)               _IQ10sin(A)
#endif
#if GLOBAL_IQ == 9
#define _IQsin(A)               _IQ9sin(A)
#endif
#if GLOBAL_IQ == 8
#define _IQsin(A)               _IQ8sin(A)
#endif
#if GLOBAL_IQ == 7
#define _IQsin(A)               _IQ7sin(A)
#endif
#if GLOBAL_IQ == 6
#define _IQsin(A)               _IQ6sin(A)
#endif
#if GLOBAL_IQ == 5
#define _IQsin(A)               _IQ5sin(A)
#endif
#if GLOBAL_IQ == 4
#define _IQsin(A)               _IQ4sin(A)
#endif
#if GLOBAL_IQ == 3
#define _IQsin(A)               _IQ3sin(A)
#endif
#if GLOBAL_IQ == 2
#define _IQsin(A)               _IQ2sin(A)
#endif
#if GLOBAL_IQ == 1
#define _IQsin(A)               _IQ1sin(A)
#endif

//*****************************************************************************
//
// Computes the sin of an IQ number, using cycles per unit instead of radians.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30sinPU(_iq30 A);
extern _iq29 _IQ29sinPU(_iq29 A);
extern _iq28 _IQ28sinPU(_iq28 A);
extern _iq27 _IQ27sinPU(_iq27 A);
extern _iq26 _IQ26sinPU(_iq26 A);
extern _iq25 _IQ25sinPU(_iq25 A);
extern _iq24 _IQ24sinPU(_iq24 A);
extern _iq23 _IQ23sinPU(_iq23 A);
extern _iq22 _IQ22sinPU(_iq22 A);
extern _iq21 _IQ21sinPU(_iq21 A);
extern _iq20 _IQ20sinPU(_iq20 A);
extern _iq19 _IQ19sinPU(_iq19 A);
extern _iq18 _IQ18sinPU(_iq18 A);
extern _iq17 _IQ17sinPU(_iq17 A);
extern _iq16 _IQ16sinPU(_iq16 A);
extern _iq15 _IQ15sinPU(_iq15 A);
extern _iq14 _IQ14sinPU(_iq14 A);
extern _iq13 _IQ13sinPU(_iq13 A);
extern _iq12 _IQ12sinPU(_iq12 A);
extern _iq11 _IQ11sinPU(_iq11 A);
extern _iq10 _IQ10sinPU(_iq10 A);
extern _iq9 _IQ9sinPU(_iq9 A);
extern _iq8 _IQ8sinPU(_iq8 A);
extern _iq7 _IQ7sinPU(_iq7 A);
extern _iq6 _IQ6sinPU(_iq6 A);
extern _iq5 _IQ5sinPU(_iq5 A);
extern _iq4 _IQ4sinPU(_iq4 A);
extern _iq3 _IQ3sinPU(_iq3 A);
extern _iq2 _IQ2sinPU(_iq2 A);
extern _iq1 _IQ1sinPU(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the sine of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type per-unit result of sine operation.
 */
#if GLOBAL_IQ == 30
#define _IQsinPU(A)             _IQ30sinPU(A)
#endif
#if GLOBAL_IQ == 29
#define _IQsinPU(A)             _IQ29sinPU(A)
#endif
#if GLOBAL_IQ == 28
#define _IQsinPU(A)             _IQ28sinPU(A)
#endif
#if GLOBAL_IQ == 27
#define _IQsinPU(A)             _IQ27sinPU(A)
#endif
#if GLOBAL_IQ == 26
#define _IQsinPU(A)             _IQ26sinPU(A)
#endif
#if GLOBAL_IQ == 25
#define _IQsinPU(A)             _IQ25sinPU(A)
#endif
#if GLOBAL_IQ == 24
#define _IQsinPU(A)             _IQ24sinPU(A)
#endif
#if GLOBAL_IQ == 23
#define _IQsinPU(A)             _IQ23sinPU(A)
#endif
#if GLOBAL_IQ == 22
#define _IQsinPU(A)             _IQ22sinPU(A)
#endif
#if GLOBAL_IQ == 21
#define _IQsinPU(A)             _IQ21sinPU(A)
#endif
#if GLOBAL_IQ == 20
#define _IQsinPU(A)             _IQ20sinPU(A)
#endif
#if GLOBAL_IQ == 19
#define _IQsinPU(A)             _IQ19sinPU(A)
#endif
#if GLOBAL_IQ == 18
#define _IQsinPU(A)             _IQ18sinPU(A)
#endif
#if GLOBAL_IQ == 17
#define _IQsinPU(A)             _IQ17sinPU(A)
#endif
#if GLOBAL_IQ == 16
#define _IQsinPU(A)             _IQ16sinPU(A)
#endif
#if GLOBAL_IQ == 15
#define _IQsinPU(A)             _IQ15sinPU(A)
#endif
#if GLOBAL_IQ == 14
#define _IQsinPU(A)             _IQ14sinPU(A)
#endif
#if GLOBAL_IQ == 13
#define _IQsinPU(A)             _IQ13sinPU(A)
#endif
#if GLOBAL_IQ == 12
#define _IQsinPU(A)             _IQ12sinPU(A)
#endif
#if GLOBAL_IQ == 11
#define _IQsinPU(A)             _IQ11sinPU(A)
#endif
#if GLOBAL_IQ == 10
#define _IQsinPU(A)             _IQ10sinPU(A)
#endif
#if GLOBAL_IQ == 9
#define _IQsinPU(A)             _IQ9sinPU(A)
#endif
#if GLOBAL_IQ == 8
#define _IQsinPU(A)             _IQ8sinPU(A)
#endif
#if GLOBAL_IQ == 7
#define _IQsinPU(A)             _IQ7sinPU(A)
#endif
#if GLOBAL_IQ == 6
#define _IQsinPU(A)             _IQ6sinPU(A)
#endif
#if GLOBAL_IQ == 5
#define _IQsinPU(A)             _IQ5sinPU(A)
#endif
#if GLOBAL_IQ == 4
#define _IQsinPU(A)             _IQ4sinPU(A)
#endif
#if GLOBAL_IQ == 3
#define _IQsinPU(A)             _IQ3sinPU(A)
#endif
#if GLOBAL_IQ == 2
#define _IQsinPU(A)             _IQ2sinPU(A)
#endif
#if GLOBAL_IQ == 1
#define _IQsinPU(A)             _IQ1sinPU(A)
#endif

//*****************************************************************************
//
// Computes the arcsin of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq29 _IQ29asin(_iq29 A);
extern _iq28 _IQ28asin(_iq28 A);
extern _iq27 _IQ27asin(_iq27 A);
extern _iq26 _IQ26asin(_iq26 A);
extern _iq25 _IQ25asin(_iq25 A);
extern _iq24 _IQ24asin(_iq24 A);
extern _iq23 _IQ23asin(_iq23 A);
extern _iq22 _IQ22asin(_iq22 A);
extern _iq21 _IQ21asin(_iq21 A);
extern _iq20 _IQ20asin(_iq20 A);
extern _iq19 _IQ19asin(_iq19 A);
extern _iq18 _IQ18asin(_iq18 A);
extern _iq17 _IQ17asin(_iq17 A);
extern _iq16 _IQ16asin(_iq16 A);
extern _iq15 _IQ15asin(_iq15 A);
extern _iq14 _IQ14asin(_iq14 A);
extern _iq13 _IQ13asin(_iq13 A);
extern _iq12 _IQ12asin(_iq12 A);
extern _iq11 _IQ11asin(_iq11 A);
extern _iq10 _IQ10asin(_iq10 A);
extern _iq9 _IQ9asin(_iq9 A);
extern _iq8 _IQ8asin(_iq8 A);
extern _iq7 _IQ7asin(_iq7 A);
extern _iq6 _IQ6asin(_iq6 A);
extern _iq5 _IQ5asin(_iq5 A);
extern _iq4 _IQ4asin(_iq4 A);
extern _iq3 _IQ3asin(_iq3 A);
extern _iq2 _IQ2asin(_iq2 A);
extern _iq1 _IQ1asin(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the inverse sine of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of inverse sine operation.
 */
#if GLOBAL_IQ == 29
#define _IQasin(A)              _IQ29asin(A)
#endif
#if GLOBAL_IQ == 28
#define _IQasin(A)              _IQ28asin(A)
#endif
#if GLOBAL_IQ == 27
#define _IQasin(A)              _IQ27asin(A)
#endif
#if GLOBAL_IQ == 26
#define _IQasin(A)              _IQ26asin(A)
#endif
#if GLOBAL_IQ == 25
#define _IQasin(A)              _IQ25asin(A)
#endif
#if GLOBAL_IQ == 24
#define _IQasin(A)              _IQ24asin(A)
#endif
#if GLOBAL_IQ == 23
#define _IQasin(A)              _IQ23asin(A)
#endif
#if GLOBAL_IQ == 22
#define _IQasin(A)              _IQ22asin(A)
#endif
#if GLOBAL_IQ == 21
#define _IQasin(A)              _IQ21asin(A)
#endif
#if GLOBAL_IQ == 20
#define _IQasin(A)              _IQ20asin(A)
#endif
#if GLOBAL_IQ == 19
#define _IQasin(A)              _IQ19asin(A)
#endif
#if GLOBAL_IQ == 18
#define _IQasin(A)              _IQ18asin(A)
#endif
#if GLOBAL_IQ == 17
#define _IQasin(A)              _IQ17asin(A)
#endif
#if GLOBAL_IQ == 16
#define _IQasin(A)              _IQ16asin(A)
#endif
#if GLOBAL_IQ == 15
#define _IQasin(A)              _IQ15asin(A)
#endif
#if GLOBAL_IQ == 14
#define _IQasin(A)              _IQ14asin(A)
#endif
#if GLOBAL_IQ == 13
#define _IQasin(A)              _IQ13asin(A)
#endif
#if GLOBAL_IQ == 12
#define _IQasin(A)              _IQ12asin(A)
#endif
#if GLOBAL_IQ == 11
#define _IQasin(A)              _IQ11asin(A)
#endif
#if GLOBAL_IQ == 10
#define _IQasin(A)              _IQ10asin(A)
#endif
#if GLOBAL_IQ == 9
#define _IQasin(A)              _IQ9asin(A)
#endif
#if GLOBAL_IQ == 8
#define _IQasin(A)              _IQ8asin(A)
#endif
#if GLOBAL_IQ == 7
#define _IQasin(A)              _IQ7asin(A)
#endif
#if GLOBAL_IQ == 6
#define _IQasin(A)              _IQ6asin(A)
#endif
#if GLOBAL_IQ == 5
#define _IQasin(A)              _IQ5asin(A)
#endif
#if GLOBAL_IQ == 4
#define _IQasin(A)              _IQ4asin(A)
#endif
#if GLOBAL_IQ == 3
#define _IQasin(A)              _IQ3asin(A)
#endif
#if GLOBAL_IQ == 2
#define _IQasin(A)              _IQ2asin(A)
#endif
#if GLOBAL_IQ == 1
#define _IQasin(A)              _IQ1asin(A)
#endif

//*****************************************************************************
//
// Computes the cos of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq29 _IQ29cos(_iq29 A);
extern _iq28 _IQ28cos(_iq28 A);
extern _iq27 _IQ27cos(_iq27 A);
extern _iq26 _IQ26cos(_iq26 A);
extern _iq25 _IQ25cos(_iq25 A);
extern _iq24 _IQ24cos(_iq24 A);
extern _iq23 _IQ23cos(_iq23 A);
extern _iq22 _IQ22cos(_iq22 A);
extern _iq21 _IQ21cos(_iq21 A);
extern _iq20 _IQ20cos(_iq20 A);
extern _iq19 _IQ19cos(_iq19 A);
extern _iq18 _IQ18cos(_iq18 A);
extern _iq17 _IQ17cos(_iq17 A);
extern _iq16 _IQ16cos(_iq16 A);
extern _iq15 _IQ15cos(_iq15 A);
extern _iq14 _IQ14cos(_iq14 A);
extern _iq13 _IQ13cos(_iq13 A);
extern _iq12 _IQ12cos(_iq12 A);
extern _iq11 _IQ11cos(_iq11 A);
extern _iq10 _IQ10cos(_iq10 A);
extern _iq9 _IQ9cos(_iq9 A);
extern _iq8 _IQ8cos(_iq8 A);
extern _iq7 _IQ7cos(_iq7 A);
extern _iq6 _IQ6cos(_iq6 A);
extern _iq5 _IQ5cos(_iq5 A);
extern _iq4 _IQ4cos(_iq4 A);
extern _iq3 _IQ3cos(_iq3 A);
extern _iq2 _IQ2cos(_iq2 A);
extern _iq1 _IQ1cos(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the cosine of a global IQ format input, in radians.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of cosine operation.
 */
#if GLOBAL_IQ == 29
#define _IQcos(A)               _IQ29cos(A)
#endif
#if GLOBAL_IQ == 28
#define _IQcos(A)               _IQ28cos(A)
#endif
#if GLOBAL_IQ == 27
#define _IQcos(A)               _IQ27cos(A)
#endif
#if GLOBAL_IQ == 26
#define _IQcos(A)               _IQ26cos(A)
#endif
#if GLOBAL_IQ == 25
#define _IQcos(A)               _IQ25cos(A)
#endif
#if GLOBAL_IQ == 24
#define _IQcos(A)               _IQ24cos(A)
#endif
#if GLOBAL_IQ == 23
#define _IQcos(A)               _IQ23cos(A)
#endif
#if GLOBAL_IQ == 22
#define _IQcos(A)               _IQ22cos(A)
#endif
#if GLOBAL_IQ == 21
#define _IQcos(A)               _IQ21cos(A)
#endif
#if GLOBAL_IQ == 20
#define _IQcos(A)               _IQ20cos(A)
#endif
#if GLOBAL_IQ == 19
#define _IQcos(A)               _IQ19cos(A)
#endif
#if GLOBAL_IQ == 18
#define _IQcos(A)               _IQ18cos(A)
#endif
#if GLOBAL_IQ == 17
#define _IQcos(A)               _IQ17cos(A)
#endif
#if GLOBAL_IQ == 16
#define _IQcos(A)               _IQ16cos(A)
#endif
#if GLOBAL_IQ == 15
#define _IQcos(A)               _IQ15cos(A)
#endif
#if GLOBAL_IQ == 14
#define _IQcos(A)               _IQ14cos(A)
#endif
#if GLOBAL_IQ == 13
#define _IQcos(A)               _IQ13cos(A)
#endif
#if GLOBAL_IQ == 12
#define _IQcos(A)               _IQ12cos(A)
#endif
#if GLOBAL_IQ == 11
#define _IQcos(A)               _IQ11cos(A)
#endif
#if GLOBAL_IQ == 10
#define _IQcos(A)               _IQ10cos(A)
#endif
#if GLOBAL_IQ == 9
#define _IQcos(A)               _IQ9cos(A)
#endif
#if GLOBAL_IQ == 8
#define _IQcos(A)               _IQ8cos(A)
#endif
#if GLOBAL_IQ == 7
#define _IQcos(A)               _IQ7cos(A)
#endif
#if GLOBAL_IQ == 6
#define _IQcos(A)               _IQ6cos(A)
#endif
#if GLOBAL_IQ == 5
#define _IQcos(A)               _IQ5cos(A)
#endif
#if GLOBAL_IQ == 4
#define _IQcos(A)               _IQ4cos(A)
#endif
#if GLOBAL_IQ == 3
#define _IQcos(A)               _IQ3cos(A)
#endif
#if GLOBAL_IQ == 2
#define _IQcos(A)               _IQ2cos(A)
#endif
#if GLOBAL_IQ == 1
#define _IQcos(A)               _IQ1cos(A)
#endif

//*****************************************************************************
//
// Computes the cos of an IQ number, using cycles per unit instead of radians.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30cosPU(_iq30 A);
extern _iq29 _IQ29cosPU(_iq29 A);
extern _iq28 _IQ28cosPU(_iq28 A);
extern _iq27 _IQ27cosPU(_iq27 A);
extern _iq26 _IQ26cosPU(_iq26 A);
extern _iq25 _IQ25cosPU(_iq25 A);
extern _iq24 _IQ24cosPU(_iq24 A);
extern _iq23 _IQ23cosPU(_iq23 A);
extern _iq22 _IQ22cosPU(_iq22 A);
extern _iq21 _IQ21cosPU(_iq21 A);
extern _iq20 _IQ20cosPU(_iq20 A);
extern _iq19 _IQ19cosPU(_iq19 A);
extern _iq18 _IQ18cosPU(_iq18 A);
extern _iq17 _IQ17cosPU(_iq17 A);
extern _iq16 _IQ16cosPU(_iq16 A);
extern _iq15 _IQ15cosPU(_iq15 A);
extern _iq14 _IQ14cosPU(_iq14 A);
extern _iq13 _IQ13cosPU(_iq13 A);
extern _iq12 _IQ12cosPU(_iq12 A);
extern _iq11 _IQ11cosPU(_iq11 A);
extern _iq10 _IQ10cosPU(_iq10 A);
extern _iq9 _IQ9cosPU(_iq9 A);
extern _iq8 _IQ8cosPU(_iq8 A);
extern _iq7 _IQ7cosPU(_iq7 A);
extern _iq6 _IQ6cosPU(_iq6 A);
extern _iq5 _IQ5cosPU(_iq5 A);
extern _iq4 _IQ4cosPU(_iq4 A);
extern _iq3 _IQ3cosPU(_iq3 A);
extern _iq2 _IQ2cosPU(_iq2 A);
extern _iq1 _IQ1cosPU(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the cossine of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type per-unit result of cosine operation.
 */
#if GLOBAL_IQ == 30
#define _IQcosPU(A)             _IQ30cosPU(A)
#endif
#if GLOBAL_IQ == 29
#define _IQcosPU(A)             _IQ29cosPU(A)
#endif
#if GLOBAL_IQ == 28
#define _IQcosPU(A)             _IQ28cosPU(A)
#endif
#if GLOBAL_IQ == 27
#define _IQcosPU(A)             _IQ27cosPU(A)
#endif
#if GLOBAL_IQ == 26
#define _IQcosPU(A)             _IQ26cosPU(A)
#endif
#if GLOBAL_IQ == 25
#define _IQcosPU(A)             _IQ25cosPU(A)
#endif
#if GLOBAL_IQ == 24
#define _IQcosPU(A)             _IQ24cosPU(A)
#endif
#if GLOBAL_IQ == 23
#define _IQcosPU(A)             _IQ23cosPU(A)
#endif
#if GLOBAL_IQ == 22
#define _IQcosPU(A)             _IQ22cosPU(A)
#endif
#if GLOBAL_IQ == 21
#define _IQcosPU(A)             _IQ21cosPU(A)
#endif
#if GLOBAL_IQ == 20
#define _IQcosPU(A)             _IQ20cosPU(A)
#endif
#if GLOBAL_IQ == 19
#define _IQcosPU(A)             _IQ19cosPU(A)
#endif
#if GLOBAL_IQ == 18
#define _IQcosPU(A)             _IQ18cosPU(A)
#endif
#if GLOBAL_IQ == 17
#define _IQcosPU(A)             _IQ17cosPU(A)
#endif
#if GLOBAL_IQ == 16
#define _IQcosPU(A)             _IQ16cosPU(A)
#endif
#if GLOBAL_IQ == 15
#define _IQcosPU(A)             _IQ15cosPU(A)
#endif
#if GLOBAL_IQ == 14
#define _IQcosPU(A)             _IQ14cosPU(A)
#endif
#if GLOBAL_IQ == 13
#define _IQcosPU(A)             _IQ13cosPU(A)
#endif
#if GLOBAL_IQ == 12
#define _IQcosPU(A)             _IQ12cosPU(A)
#endif
#if GLOBAL_IQ == 11
#define _IQcosPU(A)             _IQ11cosPU(A)
#endif
#if GLOBAL_IQ == 10
#define _IQcosPU(A)             _IQ10cosPU(A)
#endif
#if GLOBAL_IQ == 9
#define _IQcosPU(A)             _IQ9cosPU(A)
#endif
#if GLOBAL_IQ == 8
#define _IQcosPU(A)             _IQ8cosPU(A)
#endif
#if GLOBAL_IQ == 7
#define _IQcosPU(A)             _IQ7cosPU(A)
#endif
#if GLOBAL_IQ == 6
#define _IQcosPU(A)             _IQ6cosPU(A)
#endif
#if GLOBAL_IQ == 5
#define _IQcosPU(A)             _IQ5cosPU(A)
#endif
#if GLOBAL_IQ == 4
#define _IQcosPU(A)             _IQ4cosPU(A)
#endif
#if GLOBAL_IQ == 3
#define _IQcosPU(A)             _IQ3cosPU(A)
#endif
#if GLOBAL_IQ == 2
#define _IQcosPU(A)             _IQ2cosPU(A)
#endif
#if GLOBAL_IQ == 1
#define _IQcosPU(A)             _IQ1cosPU(A)
#endif

//*****************************************************************************
//
// Computes the arccos of an IQ number.
//
//*****************************************************************************
/**
 * @brief Computes the inverse cosine of an IQ29 type input.
 *
 * @param A               IQ29 input.
 *
 * @return                IQ29 type result of inverse cosine operation.
 */
#define _IQ29acos(A)            (_IQ29(1.570796327) - _IQ29asin(A))
/**
 * @brief Computes the inverse cosine of an IQ28 type input.
 *
 * @param A               IQ28 input.
 *
 * @return                IQ28 type result of inverse cosine operation.
 */
#define _IQ28acos(A)            (_IQ28(1.570796327) - _IQ28asin(A))
/**
 * @brief Computes the inverse cosine of an IQ27 type input.
 *
 * @param A               IQ27 input.
 *
 * @return                IQ27 type result of inverse cosine operation.
 */
#define _IQ27acos(A)            (_IQ27(1.570796327) - _IQ27asin(A))
/**
 * @brief Computes the inverse cosine of an IQ26 type input.
 *
 * @param A               IQ26 input.
 *
 * @return                IQ26 type result of inverse cosine operation.
 */
#define _IQ26acos(A)            (_IQ26(1.570796327) - _IQ26asin(A))
/**
 * @brief Computes the inverse cosine of an IQ25 type input.
 *
 * @param A               IQ25 input.
 *
 * @return                IQ25 type result of inverse cosine operation.
 */
#define _IQ25acos(A)            (_IQ25(1.570796327) - _IQ25asin(A))
/**
 * @brief Computes the inverse cosine of an IQ24 type input.
 *
 * @param A               IQ24 input.
 *
 * @return                IQ24 type result of inverse cosine operation.
 */
#define _IQ24acos(A)            (_IQ24(1.570796327) - _IQ24asin(A))
/**
 * @brief Computes the inverse cosine of an IQ23 type input.
 *
 * @param A               IQ23 input.
 *
 * @return                IQ23 type result of inverse cosine operation.
 */
#define _IQ23acos(A)            (_IQ23(1.570796327) - _IQ23asin(A))
/**
 * @brief Computes the inverse cosine of an IQ22 type input.
 *
 * @param A               IQ22 input.
 *
 * @return                IQ22 type result of inverse cosine operation.
 */
#define _IQ22acos(A)            (_IQ22(1.570796327) - _IQ22asin(A))
/**
 * @brief Computes the inverse cosine of an IQ21 type input.
 *
 * @param A               IQ21 input.
 *
 * @return                IQ21 type result of inverse cosine operation.
 */
#define _IQ21acos(A)            (_IQ21(1.570796327) - _IQ21asin(A))
/**
 * @brief Computes the inverse cosine of an IQ20 type input.
 *
 * @param A               IQ20 input.
 *
 * @return                IQ20 type result of inverse cosine operation.
 */
#define _IQ20acos(A)            (_IQ20(1.570796327) - _IQ20asin(A))
/**
 * @brief Computes the inverse cosine of an IQ19 type input.
 *
 * @param A               IQ19 input.
 *
 * @return                IQ19 type result of inverse cosine operation.
 */
#define _IQ19acos(A)            (_IQ19(1.570796327) - _IQ19asin(A))
/**
 * @brief Computes the inverse cosine of an IQ18 type input.
 *
 * @param A               IQ18 input.
 *
 * @return                IQ18 type result of inverse cosine operation.
 */
#define _IQ18acos(A)            (_IQ18(1.570796327) - _IQ18asin(A))
/**
 * @brief Computes the inverse cosine of an IQ17 type input.
 *
 * @param A               IQ17 input.
 *
 * @return                IQ17 type result of inverse cosine operation.
 */
#define _IQ17acos(A)            (_IQ17(1.570796327) - _IQ17asin(A))
/**
 * @brief Computes the inverse cosine of an IQ16 type input.
 *
 * @param A               IQ16 input.
 *
 * @return                IQ16 type result of inverse cosine operation.
 */
#define _IQ16acos(A)            (_IQ16(1.570796327) - _IQ16asin(A))
/**
 * @brief Computes the inverse cosine of an IQ15 type input.
 *
 * @param A               IQ15 input.
 *
 * @return                IQ15 type result of inverse cosine operation.
 */
#define _IQ15acos(A)            (_IQ15(1.570796327) - _IQ15asin(A))
/**
 * @brief Computes the inverse cosine of an IQ14 type input.
 *
 * @param A               IQ14 input.
 *
 * @return                IQ14 type result of inverse cosine operation.
 */
#define _IQ14acos(A)            (_IQ14(1.570796327) - _IQ14asin(A))
/**
 * @brief Computes the inverse cosine of an IQ13 type input.
 *
 * @param A               IQ13 input.
 *
 * @return                IQ13 type result of inverse cosine operation.
 */
#define _IQ13acos(A)            (_IQ13(1.570796327) - _IQ13asin(A))
/**
 * @brief Computes the inverse cosine of an IQ12 type input.
 *
 * @param A               IQ12 input.
 *
 * @return                IQ12 type result of inverse cosine operation.
 */
#define _IQ12acos(A)            (_IQ12(1.570796327) - _IQ12asin(A))
/**
 * @brief Computes the inverse cosine of an IQ11 type input.
 *
 * @param A               IQ11 input.
 *
 * @return                IQ11 type result of inverse cosine operation.
 */
#define _IQ11acos(A)            (_IQ11(1.570796327) - _IQ11asin(A))
/**
 * @brief Computes the inverse cosine of an IQ10 type input.
 *
 * @param A               IQ10 input.
 *
 * @return                IQ10 type result of inverse cosine operation.
 */
#define _IQ10acos(A)            (_IQ10(1.570796327) - _IQ10asin(A))
/**
 * @brief Computes the inverse cosine of an IQ9 type input.
 *
 * @param A               IQ9 input.
 *
 * @return                IQ9 type result of inverse cosine operation.
 */
#define _IQ9acos(A)             (_IQ9(1.570796327) - _IQ9asin(A))
/**
 * @brief Computes the inverse cosine of an IQ8 type input.
 *
 * @param A               IQ8 input.
 *
 * @return                IQ8 type result of inverse cosine operation.
 */
#define _IQ8acos(A)             (_IQ8(1.570796327) - _IQ8asin(A))
/**
 * @brief Computes the inverse cosine of an IQ7 type input.
 *
 * @param A               IQ7 input.
 *
 * @return                IQ7 type result of inverse cosine operation.
 */
#define _IQ7acos(A)             (_IQ7(1.570796327) - _IQ7asin(A))
/**
 * @brief Computes the inverse cosine of an IQ6 type input.
 *
 * @param A               IQ6 input.
 *
 * @return                IQ6 type result of inverse cosine operation.
 */
#define _IQ6acos(A)             (_IQ6(1.570796327) - _IQ6asin(A))
/**
 * @brief Computes the inverse cosine of an IQ5 type input.
 *
 * @param A               IQ5 input.
 *
 * @return                IQ5 type result of inverse cosine operation.
 */
#define _IQ5acos(A)             (_IQ5(1.570796327) - _IQ5asin(A))
/**
 * @brief Computes the inverse cosine of an IQ4 type input.
 *
 * @param A               IQ4 input.
 *
 * @return                IQ4 type result of inverse cosine operation.
 */
#define _IQ4acos(A)             (_IQ4(1.570796327) - _IQ4asin(A))
/**
 * @brief Computes the inverse cosine of an IQ3 type input.
 *
 * @param A               IQ3 input.
 *
 * @return                IQ3 type result of inverse cosine operation.
 */
#define _IQ3acos(A)             (_IQ3(1.570796327) - _IQ3asin(A))
/**
 * @brief Computes the inverse cosine of an IQ2 type input.
 *
 * @param A               IQ2 input.
 *
 * @return                IQ2 type result of inverse cosine operation.
 */
#define _IQ2acos(A)             (_IQ2(1.570796327) - _IQ2asin(A))
/**
 * @brief Computes the inverse cosine of an IQ1 type input.
 *
 * @param A               IQ1 input.
 *
 * @return                IQ1 type result of inverse cosine operation.
 */
#define _IQ1acos(A)             (_IQ1(1.570796327) - _IQ1asin(A))

/**
 * @brief Computes the inverse cosine of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of inverse cosine operation.
 */
#if GLOBAL_IQ == 29
#define _IQacos(A)              _IQ29acos(A)
#endif
#if GLOBAL_IQ == 28
#define _IQacos(A)              _IQ28acos(A)
#endif
#if GLOBAL_IQ == 27
#define _IQacos(A)              _IQ27acos(A)
#endif
#if GLOBAL_IQ == 26
#define _IQacos(A)              _IQ26acos(A)
#endif
#if GLOBAL_IQ == 25
#define _IQacos(A)              _IQ25acos(A)
#endif
#if GLOBAL_IQ == 24
#define _IQacos(A)              _IQ24acos(A)
#endif
#if GLOBAL_IQ == 23
#define _IQacos(A)              _IQ23acos(A)
#endif
#if GLOBAL_IQ == 22
#define _IQacos(A)              _IQ22acos(A)
#endif
#if GLOBAL_IQ == 21
#define _IQacos(A)              _IQ21acos(A)
#endif
#if GLOBAL_IQ == 20
#define _IQacos(A)              _IQ20acos(A)
#endif
#if GLOBAL_IQ == 19
#define _IQacos(A)              _IQ19acos(A)
#endif
#if GLOBAL_IQ == 18
#define _IQacos(A)              _IQ18acos(A)
#endif
#if GLOBAL_IQ == 17
#define _IQacos(A)              _IQ17acos(A)
#endif
#if GLOBAL_IQ == 16
#define _IQacos(A)              _IQ16acos(A)
#endif
#if GLOBAL_IQ == 15
#define _IQacos(A)              _IQ15acos(A)
#endif
#if GLOBAL_IQ == 14
#define _IQacos(A)              _IQ14acos(A)
#endif
#if GLOBAL_IQ == 13
#define _IQacos(A)              _IQ13acos(A)
#endif
#if GLOBAL_IQ == 12
#define _IQacos(A)              _IQ12acos(A)
#endif
#if GLOBAL_IQ == 11
#define _IQacos(A)              _IQ11acos(A)
#endif
#if GLOBAL_IQ == 10
#define _IQacos(A)              _IQ10acos(A)
#endif
#if GLOBAL_IQ == 9
#define _IQacos(A)              _IQ9acos(A)
#endif
#if GLOBAL_IQ == 8
#define _IQacos(A)              _IQ8acos(A)
#endif
#if GLOBAL_IQ == 7
#define _IQacos(A)              _IQ7acos(A)
#endif
#if GLOBAL_IQ == 6
#define _IQacos(A)              _IQ6acos(A)
#endif
#if GLOBAL_IQ == 5
#define _IQacos(A)              _IQ5acos(A)
#endif
#if GLOBAL_IQ == 4
#define _IQacos(A)              _IQ4acos(A)
#endif
#if GLOBAL_IQ == 3
#define _IQacos(A)              _IQ3acos(A)
#endif
#if GLOBAL_IQ == 2
#define _IQacos(A)              _IQ2acos(A)
#endif
#if GLOBAL_IQ == 1
#define _IQacos(A)              _IQ1acos(A)
#endif

//*****************************************************************************
//
// Computes the arctan of a coordinate specified by two IQ numbers.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq29 _IQ29atan2(_iq29 A, _iq29 B);
extern _iq28 _IQ28atan2(_iq28 A, _iq28 B);
extern _iq27 _IQ27atan2(_iq27 A, _iq27 B);
extern _iq26 _IQ26atan2(_iq26 A, _iq26 B);
extern _iq25 _IQ25atan2(_iq25 A, _iq25 B);
extern _iq24 _IQ24atan2(_iq24 A, _iq24 B);
extern _iq23 _IQ23atan2(_iq23 A, _iq23 B);
extern _iq22 _IQ22atan2(_iq22 A, _iq22 B);
extern _iq21 _IQ21atan2(_iq21 A, _iq21 B);
extern _iq20 _IQ20atan2(_iq20 A, _iq20 B);
extern _iq19 _IQ19atan2(_iq19 A, _iq19 B);
extern _iq18 _IQ18atan2(_iq18 A, _iq18 B);
extern _iq17 _IQ17atan2(_iq17 A, _iq17 B);
extern _iq16 _IQ16atan2(_iq16 A, _iq16 B);
extern _iq15 _IQ15atan2(_iq15 A, _iq15 B);
extern _iq14 _IQ14atan2(_iq14 A, _iq14 B);
extern _iq13 _IQ13atan2(_iq13 A, _iq13 B);
extern _iq12 _IQ12atan2(_iq12 A, _iq12 B);
extern _iq11 _IQ11atan2(_iq11 A, _iq11 B);
extern _iq10 _IQ10atan2(_iq10 A, _iq10 B);
extern _iq9 _IQ9atan2(_iq9 A, _iq9 B);
extern _iq8 _IQ8atan2(_iq8 A, _iq8 B);
extern _iq7 _IQ7atan2(_iq7 A, _iq7 B);
extern _iq6 _IQ6atan2(_iq6 A, _iq6 B);
extern _iq5 _IQ5atan2(_iq5 A, _iq5 B);
extern _iq4 _IQ4atan2(_iq4 A, _iq4 B);
extern _iq3 _IQ3atan2(_iq3 A, _iq3 B);
extern _iq2 _IQ2atan2(_iq2 A, _iq2 B);
extern _iq1 _IQ1atan2(_iq1 A, _iq1 B);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Compute the 4-quadrant arctangent of a
 * global IQ format input, in radians.
 *
 * @param A               Global IQ format input.
 * @param B               Global IQ format input.
 *
 * @return                Global IQ type result of
 *                        4-quadrant arctangent.
 */
#if GLOBAL_IQ == 29
#define _IQatan2(A, B)          _IQ29atan2(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQatan2(A, B)          _IQ28atan2(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQatan2(A, B)          _IQ27atan2(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQatan2(A, B)          _IQ26atan2(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQatan2(A, B)          _IQ25atan2(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQatan2(A, B)          _IQ24atan2(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQatan2(A, B)          _IQ23atan2(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQatan2(A, B)          _IQ22atan2(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQatan2(A, B)          _IQ21atan2(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQatan2(A, B)          _IQ20atan2(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQatan2(A, B)          _IQ19atan2(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQatan2(A, B)          _IQ18atan2(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQatan2(A, B)          _IQ17atan2(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQatan2(A, B)          _IQ16atan2(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQatan2(A, B)          _IQ15atan2(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQatan2(A, B)          _IQ14atan2(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQatan2(A, B)          _IQ13atan2(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQatan2(A, B)          _IQ12atan2(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQatan2(A, B)          _IQ11atan2(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQatan2(A, B)          _IQ10atan2(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQatan2(A, B)          _IQ9atan2(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQatan2(A, B)          _IQ8atan2(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQatan2(A, B)          _IQ7atan2(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQatan2(A, B)          _IQ6atan2(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQatan2(A, B)          _IQ5atan2(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQatan2(A, B)          _IQ4atan2(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQatan2(A, B)          _IQ3atan2(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQatan2(A, B)          _IQ2atan2(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQatan2(A, B)          _IQ1atan2(A, B)
#endif

//*****************************************************************************
//
// Computes the arctan of a coordinate specified by two IQ numbers, returning
// the value in cycles per unit instead of radians.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30atan2PU(_iq30 A, _iq30 B);
extern _iq29 _IQ29atan2PU(_iq29 A, _iq29 B);
extern _iq28 _IQ28atan2PU(_iq28 A, _iq28 B);
extern _iq27 _IQ27atan2PU(_iq27 A, _iq27 B);
extern _iq26 _IQ26atan2PU(_iq26 A, _iq26 B);
extern _iq25 _IQ25atan2PU(_iq25 A, _iq25 B);
extern _iq24 _IQ24atan2PU(_iq24 A, _iq24 B);
extern _iq23 _IQ23atan2PU(_iq23 A, _iq23 B);
extern _iq22 _IQ22atan2PU(_iq22 A, _iq22 B);
extern _iq21 _IQ21atan2PU(_iq21 A, _iq21 B);
extern _iq20 _IQ20atan2PU(_iq20 A, _iq20 B);
extern _iq19 _IQ19atan2PU(_iq19 A, _iq19 B);
extern _iq18 _IQ18atan2PU(_iq18 A, _iq18 B);
extern _iq17 _IQ17atan2PU(_iq17 A, _iq17 B);
extern _iq16 _IQ16atan2PU(_iq16 A, _iq16 B);
extern _iq15 _IQ15atan2PU(_iq15 A, _iq15 B);
extern _iq14 _IQ14atan2PU(_iq14 A, _iq14 B);
extern _iq13 _IQ13atan2PU(_iq13 A, _iq13 B);
extern _iq12 _IQ12atan2PU(_iq12 A, _iq12 B);
extern _iq11 _IQ11atan2PU(_iq11 A, _iq11 B);
extern _iq10 _IQ10atan2PU(_iq10 A, _iq10 B);
extern _iq9 _IQ9atan2PU(_iq9 A, _iq9 B);
extern _iq8 _IQ8atan2PU(_iq8 A, _iq8 B);
extern _iq7 _IQ7atan2PU(_iq7 A, _iq7 B);
extern _iq6 _IQ6atan2PU(_iq6 A, _iq6 B);
extern _iq5 _IQ5atan2PU(_iq5 A, _iq5 B);
extern _iq4 _IQ4atan2PU(_iq4 A, _iq4 B);
extern _iq3 _IQ3atan2PU(_iq3 A, _iq3 B);
extern _iq2 _IQ2atan2PU(_iq2 A, _iq2 B);
extern _iq1 _IQ1atan2PU(_iq1 A, _iq1 B);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Compute the 4-quadrant arctangent of a
 * global IQ format input.
 *
 * @param A               Global IQ format input.
 * @param B               Global IQ format input.
 *
 * @return                Global IQ type per-unit result of
 *                        4-quadrant arctangent.
 */
#if GLOBAL_IQ == 30
#define _IQatan2PU(A, B)        _IQ30atan2PU(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQatan2PU(A, B)        _IQ29atan2PU(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQatan2PU(A, B)        _IQ28atan2PU(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQatan2PU(A, B)        _IQ27atan2PU(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQatan2PU(A, B)        _IQ26atan2PU(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQatan2PU(A, B)        _IQ25atan2PU(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQatan2PU(A, B)        _IQ24atan2PU(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQatan2PU(A, B)        _IQ23atan2PU(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQatan2PU(A, B)        _IQ22atan2PU(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQatan2PU(A, B)        _IQ21atan2PU(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQatan2PU(A, B)        _IQ20atan2PU(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQatan2PU(A, B)        _IQ19atan2PU(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQatan2PU(A, B)        _IQ18atan2PU(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQatan2PU(A, B)        _IQ17atan2PU(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQatan2PU(A, B)        _IQ16atan2PU(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQatan2PU(A, B)        _IQ15atan2PU(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQatan2PU(A, B)        _IQ14atan2PU(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQatan2PU(A, B)        _IQ13atan2PU(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQatan2PU(A, B)        _IQ12atan2PU(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQatan2PU(A, B)        _IQ11atan2PU(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQatan2PU(A, B)        _IQ10atan2PU(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQatan2PU(A, B)        _IQ9atan2PU(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQatan2PU(A, B)        _IQ8atan2PU(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQatan2PU(A, B)        _IQ7atan2PU(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQatan2PU(A, B)        _IQ6atan2PU(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQatan2PU(A, B)        _IQ5atan2PU(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQatan2PU(A, B)        _IQ4atan2PU(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQatan2PU(A, B)        _IQ3atan2PU(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQatan2PU(A, B)        _IQ2atan2PU(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQatan2PU(A, B)        _IQ1atan2PU(A, B)
#endif

//*****************************************************************************
//
// Computes the arctan of an IQ number.
//
//*****************************************************************************
/**
 * @brief Computes the inverse tangnet of an IQ29 format input.
 *
 * @param A               IQ29 format input.
 *
 * @return                IQ29 type result of inverse tangent operation.
 */
#define _IQ29atan(A)            _IQ29atan2(A, _IQ29(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ28 format input.
 *
 * @param A               IQ28 format input.
 *
 * @return                IQ28 type result of inverse tangent operation.
 */
#define _IQ28atan(A)            _IQ28atan2(A, _IQ28(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ27 format input.
 *
 * @param A               IQ27 format input.
 *
 * @return                IQ27 type result of inverse tangent operation.
 */
#define _IQ27atan(A)            _IQ27atan2(A, _IQ27(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ26 format input.
 *
 * @param A               IQ26 format input.
 *
 * @return                IQ26 type result of inverse tangent operation.
 */
#define _IQ26atan(A)            _IQ26atan2(A, _IQ26(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ25 format input.
 *
 * @param A               IQ25 format input.
 *
 * @return                IQ25 type result of inverse tangent operation.
 */
#define _IQ25atan(A)            _IQ25atan2(A, _IQ25(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ24 format input.
 *
 * @param A               IQ24 format input.
 *
 * @return                IQ24 type result of inverse tangent operation.
 */
#define _IQ24atan(A)            _IQ24atan2(A, _IQ24(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ23 format input.
 *
 * @param A               IQ23 format input.
 *
 * @return                IQ23 type result of inverse tangent operation.
 */
#define _IQ23atan(A)            _IQ23atan2(A, _IQ23(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ22 format input.
 *
 * @param A               IQ22 format input.
 *
 * @return                IQ22 type result of inverse tangent operation.
 */
#define _IQ22atan(A)            _IQ22atan2(A, _IQ22(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ21 format input.
 *
 * @param A               IQ21 format input.
 *
 * @return                IQ21 type result of inverse tangent operation.
 */
#define _IQ21atan(A)            _IQ21atan2(A, _IQ21(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ20 format input.
 *
 * @param A               IQ20 format input.
 *
 * @return                IQ20 type result of inverse tangent operation.
 */
#define _IQ20atan(A)            _IQ20atan2(A, _IQ20(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ19 format input.
 *
 * @param A               IQ19 format input.
 *
 * @return                IQ19 type result of inverse tangent operation.
 */
#define _IQ19atan(A)            _IQ19atan2(A, _IQ19(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ18 format input.
 *
 * @param A               IQ18 format input.
 *
 * @return                IQ18 type result of inverse tangent operation.
 */
#define _IQ18atan(A)            _IQ18atan2(A, _IQ18(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ17 format input.
 *
 * @param A               IQ17 format input.
 *
 * @return                IQ17 type result of inverse tangent operation.
 */
#define _IQ17atan(A)            _IQ17atan2(A, _IQ17(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ16 format input.
 *
 * @param A               IQ16 format input.
 *
 * @return                IQ16 type result of inverse tangent operation.
 */
#define _IQ16atan(A)            _IQ16atan2(A, _IQ16(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ15 format input.
 *
 * @param A               IQ15 format input.
 *
 * @return                IQ15 type result of inverse tangent operation.
 */
#define _IQ15atan(A)            _IQ15atan2(A, _IQ15(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ14 format input.
 *
 * @param A               IQ14 format input.
 *
 * @return                IQ14 type result of inverse tangent operation.
 */
#define _IQ14atan(A)            _IQ14atan2(A, _IQ14(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ13 format input.
 *
 * @param A               IQ13 format input.
 *
 * @return                IQ13 type result of inverse tangent operation.
 */
#define _IQ13atan(A)            _IQ13atan2(A, _IQ13(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ12 format input.
 *
 * @param A               IQ12 format input.
 *
 * @return                IQ12 type result of inverse tangent operation.
 */
#define _IQ12atan(A)            _IQ12atan2(A, _IQ12(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ11 format input.
 *
 * @param A               IQ11 format input.
 *
 * @return                IQ11 type result of inverse tangent operation.
 */
#define _IQ11atan(A)            _IQ11atan2(A, _IQ11(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ10 format input.
 *
 * @param A               IQ10 format input.
 *
 * @return                IQ10 type result of inverse tangent operation.
 */
#define _IQ10atan(A)            _IQ10atan2(A, _IQ10(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ9 format input.
 *
 * @param A               IQ9 format input.
 *
 * @return                IQ9 type result of inverse tangent operation.
 */
#define _IQ9atan(A)             _IQ9atan2(A, _IQ9(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ8 format input.
 *
 * @param A               IQ8 format input.
 *
 * @return                IQ8 type result of inverse tangent operation.
 */
#define _IQ8atan(A)             _IQ8atan2(A, _IQ8(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ7 format input.
 *
 * @param A               IQ7 format input.
 *
 * @return                IQ7 type result of inverse tangent operation.
 */
#define _IQ7atan(A)             _IQ7atan2(A, _IQ7(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ6 format input.
 *
 * @param A               IQ6 format input.
 *
 * @return                IQ6 type result of inverse tangent operation.
 */
#define _IQ6atan(A)             _IQ6atan2(A, _IQ6(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ5 format input.
 *
 * @param A               IQ5 format input.
 *
 * @return                IQ5 type result of inverse tangent operation.
 */
#define _IQ5atan(A)             _IQ5atan2(A, _IQ5(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ4 format input.
 *
 * @param A               IQ4 format input.
 *
 * @return                IQ4 type result of inverse tangent operation.
 */
#define _IQ4atan(A)             _IQ4atan2(A, _IQ4(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ3 format input.
 *
 * @param A               IQ3 format input.
 *
 * @return                IQ3 type result of inverse tangent operation.
 */
#define _IQ3atan(A)             _IQ3atan2(A, _IQ3(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ2 format input.
 *
 * @param A               IQ2 format input.
 *
 * @return                IQ2 type result of inverse tangent operation.
 */
#define _IQ2atan(A)             _IQ2atan2(A, _IQ2(1.0))
/**
 * @brief Computes the inverse tangnet of an IQ1 format input.
 *
 * @param A               IQ1 format input.
 *
 * @return                IQ1 type result of inverse tangent operation.
 */
#define _IQ1atan(A)             _IQ1atan2(A, _IQ1(1.0))

/**
 * @brief Computes the inverse tangent of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of the inverse tangent.
 */
#if GLOBAL_IQ == 29
#define _IQatan(A)              _IQ29atan2(A, _IQ29(1.0))
#endif
#if GLOBAL_IQ == 28
#define _IQatan(A)              _IQ28atan2(A, _IQ28(1.0))
#endif
#if GLOBAL_IQ == 27
#define _IQatan(A)              _IQ27atan2(A, _IQ27(1.0))
#endif
#if GLOBAL_IQ == 26
#define _IQatan(A)              _IQ26atan2(A, _IQ26(1.0))
#endif
#if GLOBAL_IQ == 25
#define _IQatan(A)              _IQ25atan2(A, _IQ25(1.0))
#endif
#if GLOBAL_IQ == 24
#define _IQatan(A)              _IQ24atan2(A, _IQ24(1.0))
#endif
#if GLOBAL_IQ == 23
#define _IQatan(A)              _IQ23atan2(A, _IQ23(1.0))
#endif
#if GLOBAL_IQ == 22
#define _IQatan(A)              _IQ22atan2(A, _IQ22(1.0))
#endif
#if GLOBAL_IQ == 21
#define _IQatan(A)              _IQ21atan2(A, _IQ21(1.0))
#endif
#if GLOBAL_IQ == 20
#define _IQatan(A)              _IQ20atan2(A, _IQ20(1.0))
#endif
#if GLOBAL_IQ == 19
#define _IQatan(A)              _IQ19atan2(A, _IQ19(1.0))
#endif
#if GLOBAL_IQ == 18
#define _IQatan(A)              _IQ18atan2(A, _IQ18(1.0))
#endif
#if GLOBAL_IQ == 17
#define _IQatan(A)              _IQ17atan2(A, _IQ17(1.0))
#endif
#if GLOBAL_IQ == 16
#define _IQatan(A)              _IQ16atan2(A, _IQ16(1.0))
#endif
#if GLOBAL_IQ == 15
#define _IQatan(A)              _IQ15atan2(A, _IQ15(1.0))
#endif
#if GLOBAL_IQ == 14
#define _IQatan(A)              _IQ14atan2(A, _IQ14(1.0))
#endif
#if GLOBAL_IQ == 13
#define _IQatan(A)              _IQ13atan2(A, _IQ13(1.0))
#endif
#if GLOBAL_IQ == 12
#define _IQatan(A)              _IQ12atan2(A, _IQ12(1.0))
#endif
#if GLOBAL_IQ == 11
#define _IQatan(A)              _IQ11atan2(A, _IQ11(1.0))
#endif
#if GLOBAL_IQ == 10
#define _IQatan(A)              _IQ10atan2(A, _IQ10(1.0))
#endif
#if GLOBAL_IQ == 9
#define _IQatan(A)              _IQ9atan2(A, _IQ9(1.0))
#endif
#if GLOBAL_IQ == 8
#define _IQatan(A)              _IQ8atan2(A, _IQ8(1.0))
#endif
#if GLOBAL_IQ == 7
#define _IQatan(A)              _IQ7atan2(A, _IQ7(1.0))
#endif
#if GLOBAL_IQ == 6
#define _IQatan(A)              _IQ6atan2(A, _IQ6(1.0))
#endif
#if GLOBAL_IQ == 5
#define _IQatan(A)              _IQ5atan2(A, _IQ5(1.0))
#endif
#if GLOBAL_IQ == 4
#define _IQatan(A)              _IQ4atan2(A, _IQ4(1.0))
#endif
#if GLOBAL_IQ == 3
#define _IQatan(A)              _IQ3atan2(A, _IQ3(1.0))
#endif
#if GLOBAL_IQ == 2
#define _IQatan(A)              _IQ2atan2(A, _IQ2(1.0))
#endif
#if GLOBAL_IQ == 1
#define _IQatan(A)              _IQ1atan2(A, _IQ1(1.0))
#endif

//*****************************************************************************
//
// Computes the square root of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30sqrt(_iq30 A);
extern _iq29 _IQ29sqrt(_iq29 A);
extern _iq28 _IQ28sqrt(_iq28 A);
extern _iq27 _IQ27sqrt(_iq27 A);
extern _iq26 _IQ26sqrt(_iq26 A);
extern _iq25 _IQ25sqrt(_iq25 A);
extern _iq24 _IQ24sqrt(_iq24 A);
extern _iq23 _IQ23sqrt(_iq23 A);
extern _iq22 _IQ22sqrt(_iq22 A);
extern _iq21 _IQ21sqrt(_iq21 A);
extern _iq20 _IQ20sqrt(_iq20 A);
extern _iq19 _IQ19sqrt(_iq19 A);
extern _iq18 _IQ18sqrt(_iq18 A);
extern _iq17 _IQ17sqrt(_iq17 A);
extern _iq16 _IQ16sqrt(_iq16 A);
extern _iq15 _IQ15sqrt(_iq15 A);
extern _iq14 _IQ14sqrt(_iq14 A);
extern _iq13 _IQ13sqrt(_iq13 A);
extern _iq12 _IQ12sqrt(_iq12 A);
extern _iq11 _IQ11sqrt(_iq11 A);
extern _iq10 _IQ10sqrt(_iq10 A);
extern _iq9 _IQ9sqrt(_iq9 A);
extern _iq8 _IQ8sqrt(_iq8 A);
extern _iq7 _IQ7sqrt(_iq7 A);
extern _iq6 _IQ6sqrt(_iq6 A);
extern _iq5 _IQ5sqrt(_iq5 A);
extern _iq4 _IQ4sqrt(_iq4 A);
extern _iq3 _IQ3sqrt(_iq3 A);
extern _iq2 _IQ2sqrt(_iq2 A);
extern _iq1 _IQ1sqrt(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Calculate square root of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of the square root operation.
 */
#if GLOBAL_IQ == 30
#define _IQsqrt(A)              _IQ30sqrt(A)
#endif
#if GLOBAL_IQ == 29
#define _IQsqrt(A)              _IQ29sqrt(A)
#endif
#if GLOBAL_IQ == 28
#define _IQsqrt(A)              _IQ28sqrt(A)
#endif
#if GLOBAL_IQ == 27
#define _IQsqrt(A)              _IQ27sqrt(A)
#endif
#if GLOBAL_IQ == 26
#define _IQsqrt(A)              _IQ26sqrt(A)
#endif
#if GLOBAL_IQ == 25
#define _IQsqrt(A)              _IQ25sqrt(A)
#endif
#if GLOBAL_IQ == 24
#define _IQsqrt(A)              _IQ24sqrt(A)
#endif
#if GLOBAL_IQ == 23
#define _IQsqrt(A)              _IQ23sqrt(A)
#endif
#if GLOBAL_IQ == 22
#define _IQsqrt(A)              _IQ22sqrt(A)
#endif
#if GLOBAL_IQ == 21
#define _IQsqrt(A)              _IQ21sqrt(A)
#endif
#if GLOBAL_IQ == 20
#define _IQsqrt(A)              _IQ20sqrt(A)
#endif
#if GLOBAL_IQ == 19
#define _IQsqrt(A)              _IQ19sqrt(A)
#endif
#if GLOBAL_IQ == 18
#define _IQsqrt(A)              _IQ18sqrt(A)
#endif
#if GLOBAL_IQ == 17
#define _IQsqrt(A)              _IQ17sqrt(A)
#endif
#if GLOBAL_IQ == 16
#define _IQsqrt(A)              _IQ16sqrt(A)
#endif
#if GLOBAL_IQ == 15
#define _IQsqrt(A)              _IQ15sqrt(A)
#endif
#if GLOBAL_IQ == 14
#define _IQsqrt(A)              _IQ14sqrt(A)
#endif
#if GLOBAL_IQ == 13
#define _IQsqrt(A)              _IQ13sqrt(A)
#endif
#if GLOBAL_IQ == 12
#define _IQsqrt(A)              _IQ12sqrt(A)
#endif
#if GLOBAL_IQ == 11
#define _IQsqrt(A)              _IQ11sqrt(A)
#endif
#if GLOBAL_IQ == 10
#define _IQsqrt(A)              _IQ10sqrt(A)
#endif
#if GLOBAL_IQ == 9
#define _IQsqrt(A)              _IQ9sqrt(A)
#endif
#if GLOBAL_IQ == 8
#define _IQsqrt(A)              _IQ8sqrt(A)
#endif
#if GLOBAL_IQ == 7
#define _IQsqrt(A)              _IQ7sqrt(A)
#endif
#if GLOBAL_IQ == 6
#define _IQsqrt(A)              _IQ6sqrt(A)
#endif
#if GLOBAL_IQ == 5
#define _IQsqrt(A)              _IQ5sqrt(A)
#endif
#if GLOBAL_IQ == 4
#define _IQsqrt(A)              _IQ4sqrt(A)
#endif
#if GLOBAL_IQ == 3
#define _IQsqrt(A)              _IQ3sqrt(A)
#endif
#if GLOBAL_IQ == 2
#define _IQsqrt(A)              _IQ2sqrt(A)
#endif
#if GLOBAL_IQ == 1
#define _IQsqrt(A)              _IQ1sqrt(A)
#endif

//*****************************************************************************
//
// Computes 1 over the square root of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30isqrt(_iq30 A);
extern _iq29 _IQ29isqrt(_iq29 A);
extern _iq28 _IQ28isqrt(_iq28 A);
extern _iq27 _IQ27isqrt(_iq27 A);
extern _iq26 _IQ26isqrt(_iq26 A);
extern _iq25 _IQ25isqrt(_iq25 A);
extern _iq24 _IQ24isqrt(_iq24 A);
extern _iq23 _IQ23isqrt(_iq23 A);
extern _iq22 _IQ22isqrt(_iq22 A);
extern _iq21 _IQ21isqrt(_iq21 A);
extern _iq20 _IQ20isqrt(_iq20 A);
extern _iq19 _IQ19isqrt(_iq19 A);
extern _iq18 _IQ18isqrt(_iq18 A);
extern _iq17 _IQ17isqrt(_iq17 A);
extern _iq16 _IQ16isqrt(_iq16 A);
extern _iq15 _IQ15isqrt(_iq15 A);
extern _iq14 _IQ14isqrt(_iq14 A);
extern _iq13 _IQ13isqrt(_iq13 A);
extern _iq12 _IQ12isqrt(_iq12 A);
extern _iq11 _IQ11isqrt(_iq11 A);
extern _iq10 _IQ10isqrt(_iq10 A);
extern _iq9 _IQ9isqrt(_iq9 A);
extern _iq8 _IQ8isqrt(_iq8 A);
extern _iq7 _IQ7isqrt(_iq7 A);
extern _iq6 _IQ6isqrt(_iq6 A);
extern _iq5 _IQ5isqrt(_iq5 A);
extern _iq4 _IQ4isqrt(_iq4 A);
extern _iq3 _IQ3isqrt(_iq3 A);
extern _iq2 _IQ2isqrt(_iq2 A);
extern _iq1 _IQ1isqrt(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes 1 over the square root of a global IQ format number.
 *
 * @param A              Global IQ format input.
 *
 * @return               Global IQ type result of inverse square root operation.
 */
#if GLOBAL_IQ == 30
#define _IQisqrt(A)             _IQ30isqrt(A)
#endif
#if GLOBAL_IQ == 29
#define _IQisqrt(A)             _IQ29isqrt(A)
#endif
#if GLOBAL_IQ == 28
#define _IQisqrt(A)             _IQ28isqrt(A)
#endif
#if GLOBAL_IQ == 27
#define _IQisqrt(A)             _IQ27isqrt(A)
#endif
#if GLOBAL_IQ == 26
#define _IQisqrt(A)             _IQ26isqrt(A)
#endif
#if GLOBAL_IQ == 25
#define _IQisqrt(A)             _IQ25isqrt(A)
#endif
#if GLOBAL_IQ == 24
#define _IQisqrt(A)             _IQ24isqrt(A)
#endif
#if GLOBAL_IQ == 23
#define _IQisqrt(A)             _IQ23isqrt(A)
#endif
#if GLOBAL_IQ == 22
#define _IQisqrt(A)             _IQ22isqrt(A)
#endif
#if GLOBAL_IQ == 21
#define _IQisqrt(A)             _IQ21isqrt(A)
#endif
#if GLOBAL_IQ == 20
#define _IQisqrt(A)             _IQ20isqrt(A)
#endif
#if GLOBAL_IQ == 19
#define _IQisqrt(A)             _IQ19isqrt(A)
#endif
#if GLOBAL_IQ == 18
#define _IQisqrt(A)             _IQ18isqrt(A)
#endif
#if GLOBAL_IQ == 17
#define _IQisqrt(A)             _IQ17isqrt(A)
#endif
#if GLOBAL_IQ == 16
#define _IQisqrt(A)             _IQ16isqrt(A)
#endif
#if GLOBAL_IQ == 15
#define _IQisqrt(A)             _IQ15isqrt(A)
#endif
#if GLOBAL_IQ == 14
#define _IQisqrt(A)             _IQ14isqrt(A)
#endif
#if GLOBAL_IQ == 13
#define _IQisqrt(A)             _IQ13isqrt(A)
#endif
#if GLOBAL_IQ == 12
#define _IQisqrt(A)             _IQ12isqrt(A)
#endif
#if GLOBAL_IQ == 11
#define _IQisqrt(A)             _IQ11isqrt(A)
#endif
#if GLOBAL_IQ == 10
#define _IQisqrt(A)             _IQ10isqrt(A)
#endif
#if GLOBAL_IQ == 9
#define _IQisqrt(A)             _IQ9isqrt(A)
#endif
#if GLOBAL_IQ == 8
#define _IQisqrt(A)             _IQ8isqrt(A)
#endif
#if GLOBAL_IQ == 7
#define _IQisqrt(A)             _IQ7isqrt(A)
#endif
#if GLOBAL_IQ == 6
#define _IQisqrt(A)             _IQ6isqrt(A)
#endif
#if GLOBAL_IQ == 5
#define _IQisqrt(A)             _IQ5isqrt(A)
#endif
#if GLOBAL_IQ == 4
#define _IQisqrt(A)             _IQ4isqrt(A)
#endif
#if GLOBAL_IQ == 3
#define _IQisqrt(A)             _IQ3isqrt(A)
#endif
#if GLOBAL_IQ == 2
#define _IQisqrt(A)             _IQ2isqrt(A)
#endif
#if GLOBAL_IQ == 1
#define _IQisqrt(A)             _IQ1isqrt(A)
#endif

//*****************************************************************************
//
// Computes e^x of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30exp(_iq30 A);
extern _iq29 _IQ29exp(_iq29 A);
extern _iq28 _IQ28exp(_iq28 A);
extern _iq27 _IQ27exp(_iq27 A);
extern _iq26 _IQ26exp(_iq26 A);
extern _iq25 _IQ25exp(_iq25 A);
extern _iq24 _IQ24exp(_iq24 A);
extern _iq23 _IQ23exp(_iq23 A);
extern _iq22 _IQ22exp(_iq22 A);
extern _iq21 _IQ21exp(_iq21 A);
extern _iq20 _IQ20exp(_iq20 A);
extern _iq19 _IQ19exp(_iq19 A);
extern _iq18 _IQ18exp(_iq18 A);
extern _iq17 _IQ17exp(_iq17 A);
extern _iq16 _IQ16exp(_iq16 A);
extern _iq15 _IQ15exp(_iq15 A);
extern _iq14 _IQ14exp(_iq14 A);
extern _iq13 _IQ13exp(_iq13 A);
extern _iq12 _IQ12exp(_iq12 A);
extern _iq11 _IQ11exp(_iq11 A);
extern _iq10 _IQ10exp(_iq10 A);
extern _iq9 _IQ9exp(_iq9 A);
extern _iq8 _IQ8exp(_iq8 A);
extern _iq7 _IQ7exp(_iq7 A);
extern _iq6 _IQ6exp(_iq6 A);
extern _iq5 _IQ5exp(_iq5 A);
extern _iq4 _IQ4exp(_iq4 A);
extern _iq3 _IQ3exp(_iq3 A);
extern _iq2 _IQ2exp(_iq2 A);
extern _iq1 _IQ1exp(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the exponential of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of exponential.
 */
#if GLOBAL_IQ == 30
#define _IQexp(A)               _IQ30exp(A)
#endif
#if GLOBAL_IQ == 29
#define _IQexp(A)               _IQ29exp(A)
#endif
#if GLOBAL_IQ == 28
#define _IQexp(A)               _IQ28exp(A)
#endif
#if GLOBAL_IQ == 27
#define _IQexp(A)               _IQ27exp(A)
#endif
#if GLOBAL_IQ == 26
#define _IQexp(A)               _IQ26exp(A)
#endif
#if GLOBAL_IQ == 25
#define _IQexp(A)               _IQ25exp(A)
#endif
#if GLOBAL_IQ == 24
#define _IQexp(A)               _IQ24exp(A)
#endif
#if GLOBAL_IQ == 23
#define _IQexp(A)               _IQ23exp(A)
#endif
#if GLOBAL_IQ == 22
#define _IQexp(A)               _IQ22exp(A)
#endif
#if GLOBAL_IQ == 21
#define _IQexp(A)               _IQ21exp(A)
#endif
#if GLOBAL_IQ == 20
#define _IQexp(A)               _IQ20exp(A)
#endif
#if GLOBAL_IQ == 19
#define _IQexp(A)               _IQ19exp(A)
#endif
#if GLOBAL_IQ == 18
#define _IQexp(A)               _IQ18exp(A)
#endif
#if GLOBAL_IQ == 17
#define _IQexp(A)               _IQ17exp(A)
#endif
#if GLOBAL_IQ == 16
#define _IQexp(A)               _IQ16exp(A)
#endif
#if GLOBAL_IQ == 15
#define _IQexp(A)               _IQ15exp(A)
#endif
#if GLOBAL_IQ == 14
#define _IQexp(A)               _IQ14exp(A)
#endif
#if GLOBAL_IQ == 13
#define _IQexp(A)               _IQ13exp(A)
#endif
#if GLOBAL_IQ == 12
#define _IQexp(A)               _IQ12exp(A)
#endif
#if GLOBAL_IQ == 11
#define _IQexp(A)               _IQ11exp(A)
#endif
#if GLOBAL_IQ == 10
#define _IQexp(A)               _IQ10exp(A)
#endif
#if GLOBAL_IQ == 9
#define _IQexp(A)               _IQ9exp(A)
#endif
#if GLOBAL_IQ == 8
#define _IQexp(A)               _IQ8exp(A)
#endif
#if GLOBAL_IQ == 7
#define _IQexp(A)               _IQ7exp(A)
#endif
#if GLOBAL_IQ == 6
#define _IQexp(A)               _IQ6exp(A)
#endif
#if GLOBAL_IQ == 5
#define _IQexp(A)               _IQ5exp(A)
#endif
#if GLOBAL_IQ == 4
#define _IQexp(A)               _IQ4exp(A)
#endif
#if GLOBAL_IQ == 3
#define _IQexp(A)               _IQ3exp(A)
#endif
#if GLOBAL_IQ == 2
#define _IQexp(A)               _IQ2exp(A)
#endif
#if GLOBAL_IQ == 1
#define _IQexp(A)               _IQ1exp(A)
#endif

//*****************************************************************************
//
// Computes log(x) of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30log(_iq30 A);
extern _iq29 _IQ29log(_iq29 A);
extern _iq28 _IQ28log(_iq28 A);
extern _iq27 _IQ27log(_iq27 A);
extern _iq26 _IQ26log(_iq26 A);
extern _iq25 _IQ25log(_iq25 A);
extern _iq24 _IQ24log(_iq24 A);
extern _iq23 _IQ23log(_iq23 A);
extern _iq22 _IQ22log(_iq22 A);
extern _iq21 _IQ21log(_iq21 A);
extern _iq20 _IQ20log(_iq20 A);
extern _iq19 _IQ19log(_iq19 A);
extern _iq18 _IQ18log(_iq18 A);
extern _iq17 _IQ17log(_iq17 A);
extern _iq16 _IQ16log(_iq16 A);
extern _iq15 _IQ15log(_iq15 A);
extern _iq14 _IQ14log(_iq14 A);
extern _iq13 _IQ13log(_iq13 A);
extern _iq12 _IQ12log(_iq12 A);
extern _iq11 _IQ11log(_iq11 A);
extern _iq10 _IQ10log(_iq10 A);
extern _iq9 _IQ9log(_iq9 A);
extern _iq8 _IQ8log(_iq8 A);
extern _iq7 _IQ7log(_iq7 A);
extern _iq6 _IQ6log(_iq6 A);
extern _iq5 _IQ5log(_iq5 A);
extern _iq4 _IQ4log(_iq4 A);
extern _iq3 _IQ3log(_iq3 A);
extern _iq2 _IQ2log(_iq2 A);
extern _iq1 _IQ1log(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the base-e logarithm of a global IQ format input.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ type result of base-e logarithm.
 */
#if GLOBAL_IQ == 30
#define _IQlog(A)               _IQ30log(A)
#endif
#if GLOBAL_IQ == 29
#define _IQlog(A)               _IQ29log(A)
#endif
#if GLOBAL_IQ == 28
#define _IQlog(A)               _IQ28log(A)
#endif
#if GLOBAL_IQ == 27
#define _IQlog(A)               _IQ27log(A)
#endif
#if GLOBAL_IQ == 26
#define _IQlog(A)               _IQ26log(A)
#endif
#if GLOBAL_IQ == 25
#define _IQlog(A)               _IQ25log(A)
#endif
#if GLOBAL_IQ == 24
#define _IQlog(A)               _IQ24log(A)
#endif
#if GLOBAL_IQ == 23
#define _IQlog(A)               _IQ23log(A)
#endif
#if GLOBAL_IQ == 22
#define _IQlog(A)               _IQ22log(A)
#endif
#if GLOBAL_IQ == 21
#define _IQlog(A)               _IQ21log(A)
#endif
#if GLOBAL_IQ == 20
#define _IQlog(A)               _IQ20log(A)
#endif
#if GLOBAL_IQ == 19
#define _IQlog(A)               _IQ19log(A)
#endif
#if GLOBAL_IQ == 18
#define _IQlog(A)               _IQ18log(A)
#endif
#if GLOBAL_IQ == 17
#define _IQlog(A)               _IQ17log(A)
#endif
#if GLOBAL_IQ == 16
#define _IQlog(A)               _IQ16log(A)
#endif
#if GLOBAL_IQ == 15
#define _IQlog(A)               _IQ15log(A)
#endif
#if GLOBAL_IQ == 14
#define _IQlog(A)               _IQ14log(A)
#endif
#if GLOBAL_IQ == 13
#define _IQlog(A)               _IQ13log(A)
#endif
#if GLOBAL_IQ == 12
#define _IQlog(A)               _IQ12log(A)
#endif
#if GLOBAL_IQ == 11
#define _IQlog(A)               _IQ11log(A)
#endif
#if GLOBAL_IQ == 10
#define _IQlog(A)               _IQ10log(A)
#endif
#if GLOBAL_IQ == 9
#define _IQlog(A)               _IQ9log(A)
#endif
#if GLOBAL_IQ == 8
#define _IQlog(A)               _IQ8log(A)
#endif
#if GLOBAL_IQ == 7
#define _IQlog(A)               _IQ7log(A)
#endif
#if GLOBAL_IQ == 6
#define _IQlog(A)               _IQ6log(A)
#endif
#if GLOBAL_IQ == 5
#define _IQlog(A)               _IQ5log(A)
#endif
#if GLOBAL_IQ == 4
#define _IQlog(A)               _IQ4log(A)
#endif
#if GLOBAL_IQ == 3
#define _IQlog(A)               _IQ3log(A)
#endif
#if GLOBAL_IQ == 2
#define _IQlog(A)               _IQ2log(A)
#endif
#if GLOBAL_IQ == 1
#define _IQlog(A)               _IQ1log(A)
#endif

//*****************************************************************************
//
// Returns the integer portion of an IQ number.
//
//*****************************************************************************
/**
 * @brief Returns the integer portion of an IQ30 type number.
 *
 * @param A               IQ30 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ30int(A)             ((A) >> 30)
/**
 * @brief Returns the integer portion of an IQ29 type number.
 *
 * @param A               IQ29 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ29int(A)             ((A) >> 29)
/**
 * @brief Returns the integer portion of an IQ28 type number.
 *
 * @param A               IQ28 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ28int(A)             ((A) >> 28)
/**
 * @brief Returns the integer portion of an IQ27 type number.
 *
 * @param A               IQ27 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ27int(A)             ((A) >> 27)
/**
 * @brief Returns the integer portion of an IQ26 type number.
 *
 * @param A               IQ26 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ26int(A)             ((A) >> 26)
/**
 * @brief Returns the integer portion of an IQ25 type number.
 *
 * @param A               IQ25 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ25int(A)             ((A) >> 25)
/**
 * @brief Returns the integer portion of an IQ24 type number.
 *
 * @param A               IQ24 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ24int(A)             ((A) >> 24)
/**
 * @brief Returns the integer portion of an IQ23 type number.
 *
 * @param A               IQ23 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ23int(A)             ((A) >> 23)
/**
 * @brief Returns the integer portion of an IQ22 type number.
 *
 * @param A               IQ22 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ22int(A)             ((A) >> 22)
/**
 * @brief Returns the integer portion of an IQ21 type number.
 *
 * @param A               IQ21 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ21int(A)             ((A) >> 21)
/**
 * @brief Returns the integer portion of an IQ20 type number.
 *
 * @param A               IQ20 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ20int(A)             ((A) >> 20)
/**
 * @brief Returns the integer portion of an IQ19 type number.
 *
 * @param A               IQ19 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ19int(A)             ((A) >> 19)
/**
 * @brief Returns the integer portion of an IQ18 type number.
 *
 * @param A               IQ18 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ18int(A)             ((A) >> 18)
/**
 * @brief Returns the integer portion of an IQ17 type number.
 *
 * @param A               IQ17 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ17int(A)             ((A) >> 17)
/**
 * @brief Returns the integer portion of an IQ16 type number.
 *
 * @param A               IQ16 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ16int(A)             ((A) >> 16)
/**
 * @brief Returns the integer portion of an IQ15 type number.
 *
 * @param A               IQ15 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ15int(A)             ((A) >> 15)
/**
 * @brief Returns the integer portion of an IQ14 type number.
 *
 * @param A               IQ14 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ14int(A)             ((A) >> 14)
/**
 * @brief Returns the integer portion of an IQ13 type number.
 *
 * @param A               IQ13 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ13int(A)             ((A) >> 13)
/**
 * @brief Returns the integer portion of an IQ12 type number.
 *
 * @param A               IQ12 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ12int(A)             ((A) >> 12)
/**
 * @brief Returns the integer portion of an IQ11 type number.
 *
 * @param A               IQ11 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ11int(A)             ((A) >> 11)
/**
 * @brief Returns the integer portion of an IQ10 type number.
 *
 * @param A               IQ10 type input.
 *
 * @return                Iinteger portion of input.
 */
#define _IQ10int(A)             ((A) >> 10)
/**
 * @brief Returns the integer portion of an IQ9 type number.
 *
 * @param A               IQ9 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ9int(A)              ((A) >> 9)
/**
 * @brief Returns the integer portion of an IQ8 type number.
 *
 * @param A               IQ8 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ8int(A)              ((A) >> 8)
/**
 * @brief Returns the integer portion of an IQ7 type number.
 *
 * @param A               IQ7 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ7int(A)              ((A) >> 7)
/**
 * @brief Returns the integer portion of an IQ6 type number.
 *
 * @param A               IQ6 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ6int(A)              ((A) >> 6)
/**
 * @brief Returns the integer portion of an IQ5 type number.
 *
 * @param A               IQ5 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ5int(A)              ((A) >> 5)
/**
 * @brief Returns the integer portion of an IQ4 type number.
 *
 * @param A               IQ4 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ4int(A)              ((A) >> 4)
/**
 * @brief Returns the integer portion of an IQ3 type number.
 *
 * @param A               IQ3 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ3int(A)              ((A) >> 3)
/**
 * @brief Returns the integer portion of an IQ2 type number.
 *
 * @param A               IQ2 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ2int(A)              ((A) >> 2)
/**
 * @brief Returns the integer portion of an IQ1 type number.
 *
 * @param A               IQ1 type input.
 *
 * @return                Integer portion of input.
 */
#define _IQ1int(A)              ((A) >> 1)
/**
 * @brief Returns the integer portion of a global IQ format number.
 *
 * @param A               Global IQ format input.
 *
 * @return                Integer portion of input.
 */
#define _IQint(A)               ((A) >> GLOBAL_IQ)

//*****************************************************************************
//
// Computes the fractional portion of an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _IQ30frac(_iq30 A);
extern _iq29 _IQ29frac(_iq29 A);
extern _iq28 _IQ28frac(_iq28 A);
extern _iq27 _IQ27frac(_iq27 A);
extern _iq26 _IQ26frac(_iq26 A);
extern _iq25 _IQ25frac(_iq25 A);
extern _iq24 _IQ24frac(_iq24 A);
extern _iq23 _IQ23frac(_iq23 A);
extern _iq22 _IQ22frac(_iq22 A);
extern _iq21 _IQ21frac(_iq21 A);
extern _iq20 _IQ20frac(_iq20 A);
extern _iq19 _IQ19frac(_iq19 A);
extern _iq18 _IQ18frac(_iq18 A);
extern _iq17 _IQ17frac(_iq17 A);
extern _iq16 _IQ16frac(_iq16 A);
extern _iq15 _IQ15frac(_iq15 A);
extern _iq14 _IQ14frac(_iq14 A);
extern _iq13 _IQ13frac(_iq13 A);
extern _iq12 _IQ12frac(_iq12 A);
extern _iq11 _IQ11frac(_iq11 A);
extern _iq10 _IQ10frac(_iq10 A);
extern _iq9 _IQ9frac(_iq9 A);
extern _iq8 _IQ8frac(_iq8 A);
extern _iq7 _IQ7frac(_iq7 A);
extern _iq6 _IQ6frac(_iq6 A);
extern _iq5 _IQ5frac(_iq5 A);
extern _iq4 _IQ4frac(_iq4 A);
extern _iq3 _IQ3frac(_iq3 A);
extern _iq2 _IQ2frac(_iq2 A);
extern _iq1 _IQ1frac(_iq1 A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the fractional portion a global IQ format number.
 *
 * @param A               Global IQ format input.
 *
 * @return                Global IQ format fractional portion of input.
 */
#if GLOBAL_IQ == 30
#define _IQfrac(A)              _IQ30frac(A)
#endif
#if GLOBAL_IQ == 29
#define _IQfrac(A)              _IQ29frac(A)
#endif
#if GLOBAL_IQ == 28
#define _IQfrac(A)              _IQ28frac(A)
#endif
#if GLOBAL_IQ == 27
#define _IQfrac(A)              _IQ27frac(A)
#endif
#if GLOBAL_IQ == 26
#define _IQfrac(A)              _IQ26frac(A)
#endif
#if GLOBAL_IQ == 25
#define _IQfrac(A)              _IQ25frac(A)
#endif
#if GLOBAL_IQ == 24
#define _IQfrac(A)              _IQ24frac(A)
#endif
#if GLOBAL_IQ == 23
#define _IQfrac(A)              _IQ23frac(A)
#endif
#if GLOBAL_IQ == 22
#define _IQfrac(A)              _IQ22frac(A)
#endif
#if GLOBAL_IQ == 21
#define _IQfrac(A)              _IQ21frac(A)
#endif
#if GLOBAL_IQ == 20
#define _IQfrac(A)              _IQ20frac(A)
#endif
#if GLOBAL_IQ == 19
#define _IQfrac(A)              _IQ19frac(A)
#endif
#if GLOBAL_IQ == 18
#define _IQfrac(A)              _IQ18frac(A)
#endif
#if GLOBAL_IQ == 17
#define _IQfrac(A)              _IQ17frac(A)
#endif
#if GLOBAL_IQ == 16
#define _IQfrac(A)              _IQ16frac(A)
#endif
#if GLOBAL_IQ == 15
#define _IQfrac(A)              _IQ15frac(A)
#endif
#if GLOBAL_IQ == 14
#define _IQfrac(A)              _IQ14frac(A)
#endif
#if GLOBAL_IQ == 13
#define _IQfrac(A)              _IQ13frac(A)
#endif
#if GLOBAL_IQ == 12
#define _IQfrac(A)              _IQ12frac(A)
#endif
#if GLOBAL_IQ == 11
#define _IQfrac(A)              _IQ11frac(A)
#endif
#if GLOBAL_IQ == 10
#define _IQfrac(A)              _IQ10frac(A)
#endif
#if GLOBAL_IQ == 9
#define _IQfrac(A)              _IQ9frac(A)
#endif
#if GLOBAL_IQ == 8
#define _IQfrac(A)              _IQ8frac(A)
#endif
#if GLOBAL_IQ == 7
#define _IQfrac(A)              _IQ7frac(A)
#endif
#if GLOBAL_IQ == 6
#define _IQfrac(A)              _IQ6frac(A)
#endif
#if GLOBAL_IQ == 5
#define _IQfrac(A)              _IQ5frac(A)
#endif
#if GLOBAL_IQ == 4
#define _IQfrac(A)              _IQ4frac(A)
#endif
#if GLOBAL_IQ == 3
#define _IQfrac(A)              _IQ3frac(A)
#endif
#if GLOBAL_IQ == 2
#define _IQfrac(A)              _IQ2frac(A)
#endif
#if GLOBAL_IQ == 1
#define _IQfrac(A)              _IQ1frac(A)
#endif

//*****************************************************************************
//
// Multiplies two IQ numbers in the specified iQ formats, returning the result
// in another IQ format.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern int32_t _IQ30mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ29mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ28mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ27mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ26mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ25mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ24mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ23mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ22mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ21mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ20mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ19mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ18mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ17mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ16mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ15mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ14mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ13mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ12mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ11mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ10mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ9mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ8mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ7mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ6mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ5mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ4mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ3mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ2mpyIQX(int32_t A, int n1, int32_t B, int n2);
extern int32_t _IQ1mpyIQX(int32_t A, int n1, int32_t B, int n2);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Multiply two IQ numbers in different IQ formats,
 * returning the product in global IQ format.
 *
 * @param A               IQN1 format input to be multiplied.
 * @param n1              IQ format for first value.
 * @param B               IQN2 format input to be multiplied.
 * @param n2              IQ format for second value.
 *
 *
 * @return                Global IQ format result of the multiplication.
 */
#if GLOBAL_IQ == 30
#define _IQmpyIQX(A, n1, B, n2)              _IQ30mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 29
#define _IQmpyIQX(A, n1, B, n2)               _IQ29mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 28
#define _IQmpyIQX(A, n1, B, n2)              _IQ28mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 27
#define _IQmpyIQX(A, n1, B, n2)              _IQ27mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 26
#define _IQmpyIQX(A, n1, B, n2)              _IQ26mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 25
#define _IQmpyIQX(A, n1, B, n2)              _IQ25mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 24
#define _IQmpyIQX(A, n1, B, n2)              _IQ24mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 23
#define _IQmpyIQX(A, n1, B, n2)              _IQ23mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 22
#define _IQmpyIQX(A, n1, B, n2)              _IQ22mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 21
#define _IQmpyIQX(A, n1, B, n2)              _IQ21mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 20
#define _IQmpyIQX(A, n1, B, n2)              _IQ20mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 19
#define _IQmpyIQX(A, n1, B, n2)              _IQ19mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 18
#define _IQmpyIQX(A, n1, B, n2)              _IQ18mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 17
#define _IQmpyIQX(A, n1, B, n2)              _IQ17mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 16
#define _IQmpyIQX(A, n1, B, n2)              _IQ16mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 15
#define _IQmpyIQX(A, n1, B, n2)              _IQ15mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 14
#define _IQmpyIQX(A, n1, B, n2)              _IQ14mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 13
#define _IQmpyIQX(A, n1, B, n2)              _IQ13mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 12
#define _IQmpyIQX(A, n1, B, n2)              _IQ12mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 11
#define _IQmpyIQX(A, n1, B, n2)              _IQ11mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 10
#define _IQmpyIQX(A, n1, B, n2)              _IQ10mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 9
#define _IQmpyIQX(A, n1, B, n2)              _IQ9mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 8
#define _IQmpyIQX(A, n1, B, n2)              _IQ8mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 7
#define _IQmpyIQX(A, n1, B, n2)              _IQ7mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 6
#define _IQmpyIQX(A, n1, B, n2)              _IQ6mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 5
#define _IQmpyIQX(A, n1, B, n2)              _IQ5mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 4
#define _IQmpyIQX(A, n1, B, n2)              _IQ4mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 3
#define _IQmpyIQX(A, n1, B, n2)              _IQ3mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 2
#define _IQmpyIQX(A, n1, B, n2)              _IQ2mpyIQX(A, n1, B, n2)
#endif
#if GLOBAL_IQ == 1
#define _IQmpyIQX(A, n1, B, n2)              _IQ1mpyIQX(A, n1, B, n2)
#endif

#ifndef DOXYGEN_SHOULD_SKIP_THIS
//*****************************************************************************
//
// Multiplies an IQ number by an integer.
//
//*****************************************************************************
#define _IQ30mpyI32(A, B)       ((A) * (B))
#define _IQ29mpyI32(A, B)       ((A) * (B))
#define _IQ28mpyI32(A, B)       ((A) * (B))
#define _IQ27mpyI32(A, B)       ((A) * (B))
#define _IQ26mpyI32(A, B)       ((A) * (B))
#define _IQ25mpyI32(A, B)       ((A) * (B))
#define _IQ24mpyI32(A, B)       ((A) * (B))
#define _IQ23mpyI32(A, B)       ((A) * (B))
#define _IQ22mpyI32(A, B)       ((A) * (B))
#define _IQ21mpyI32(A, B)       ((A) * (B))
#define _IQ20mpyI32(A, B)       ((A) * (B))
#define _IQ19mpyI32(A, B)       ((A) * (B))
#define _IQ18mpyI32(A, B)       ((A) * (B))
#define _IQ17mpyI32(A, B)       ((A) * (B))
#define _IQ16mpyI32(A, B)       ((A) * (B))
#define _IQ15mpyI32(A, B)       ((A) * (B))
#define _IQ14mpyI32(A, B)       ((A) * (B))
#define _IQ13mpyI32(A, B)       ((A) * (B))
#define _IQ12mpyI32(A, B)       ((A) * (B))
#define _IQ11mpyI32(A, B)       ((A) * (B))
#define _IQ10mpyI32(A, B)       ((A) * (B))
#define _IQ9mpyI32(A, B)        ((A) * (B))
#define _IQ8mpyI32(A, B)        ((A) * (B))
#define _IQ7mpyI32(A, B)        ((A) * (B))
#define _IQ6mpyI32(A, B)        ((A) * (B))
#define _IQ5mpyI32(A, B)        ((A) * (B))
#define _IQ4mpyI32(A, B)        ((A) * (B))
#define _IQ3mpyI32(A, B)        ((A) * (B))
#define _IQ2mpyI32(A, B)        ((A) * (B))
#define _IQ1mpyI32(A, B)        ((A) * (B))
#define _IQmpyI32(A, B)         ((A) * (B))

//*****************************************************************************
//
// Multiplies an IQ number by an integer, and returns the integer portion.
//
//*****************************************************************************
#define _IQ30mpyI32int(A, B)   _IQ30int(_IQ30mpyI32(A, B))
#define _IQ29mpyI32int(A, B)   _IQ29int(_IQ29mpyI32(A, B))
#define _IQ28mpyI32int(A, B)   _IQ28int(_IQ28mpyI32(A, B))
#define _IQ27mpyI32int(A, B)   _IQ27int(_IQ27mpyI32(A, B))
#define _IQ26mpyI32int(A, B)   _IQ26int(_IQ26mpyI32(A, B))
#define _IQ25mpyI32int(A, B)   _IQ25int(_IQ25mpyI32(A, B))
#define _IQ24mpyI32int(A, B)   _IQ24int(_IQ24mpyI32(A, B))
#define _IQ23mpyI32int(A, B)   _IQ23int(_IQ23mpyI32(A, B))
#define _IQ22mpyI32int(A, B)   _IQ22int(_IQ22mpyI32(A, B))
#define _IQ21mpyI32int(A, B)   _IQ21int(_IQ21mpyI32(A, B))
#define _IQ20mpyI32int(A, B)   _IQ20int(_IQ20mpyI32(A, B))
#define _IQ19mpyI32int(A, B)   _IQ19int(_IQ19mpyI32(A, B))
#define _IQ18mpyI32int(A, B)   _IQ18int(_IQ18mpyI32(A, B))
#define _IQ17mpyI32int(A, B)   _IQ17int(_IQ17mpyI32(A, B))
#define _IQ16mpyI32int(A, B)   _IQ16int(_IQ16mpyI32(A, B))
#define _IQ15mpyI32int(A, B)   _IQ15int(_IQ15mpyI32(A, B))
#define _IQ14mpyI32int(A, B)   _IQ14int(_IQ14mpyI32(A, B))
#define _IQ13mpyI32int(A, B)   _IQ13int(_IQ13mpyI32(A, B))
#define _IQ12mpyI32int(A, B)   _IQ12int(_IQ12mpyI32(A, B))
#define _IQ11mpyI32int(A, B)   _IQ11int(_IQ11mpyI32(A, B))
#define _IQ10mpyI32int(A, B)   _IQ10int(_IQ10mpyI32(A, B))
#define _IQ9mpyI32int(A, B)   _IQ9int(_IQ9mpyI32(A, B))
#define _IQ8mpyI32int(A, B)   _IQ8int(_IQ8mpyI32(A, B))
#define _IQ7mpyI32int(A, B)   _IQ7int(_IQ7mpyI32(A, B))
#define _IQ6mpyI32int(A, B)   _IQ6int(_IQ6mpyI32(A, B))
#define _IQ5mpyI32int(A, B)   _IQ5int(_IQ5mpyI32(A, B))
#define _IQ4mpyI32int(A, B)   _IQ4int(_IQ4mpyI32(A, B))
#define _IQ3mpyI32int(A, B)   _IQ3int(_IQ3mpyI32(A, B))
#define _IQ2mpyI32int(A, B)   _IQ2int(_IQ2mpyI32(A, B))
#define _IQ1mpyI32int(A, B)   _IQ1int(_IQ1mpyI32(A, B))

#if GLOBAL_IQ == 30
#define _IQmpyI32int(A, B)      _IQ30mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQmpyI32int(A, B)      _IQ29mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQmpyI32int(A, B)      _IQ28mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQmpyI32int(A, B)      _IQ27mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQmpyI32int(A, B)      _IQ26mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQmpyI32int(A, B)      _IQ25mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQmpyI32int(A, B)      _IQ24mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQmpyI32int(A, B)      _IQ23mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQmpyI32int(A, B)      _IQ22mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQmpyI32int(A, B)      _IQ21mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQmpyI32int(A, B)      _IQ20mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQmpyI32int(A, B)      _IQ19mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQmpyI32int(A, B)      _IQ18mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQmpyI32int(A, B)      _IQ17mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQmpyI32int(A, B)      _IQ16mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQmpyI32int(A, B)      _IQ15mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQmpyI32int(A, B)      _IQ14mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQmpyI32int(A, B)      _IQ13mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQmpyI32int(A, B)      _IQ12mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQmpyI32int(A, B)      _IQ11mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQmpyI32int(A, B)      _IQ10mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQmpyI32int(A, B)      _IQ9mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQmpyI32int(A, B)      _IQ8mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQmpyI32int(A, B)      _IQ7mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQmpyI32int(A, B)      _IQ6mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQmpyI32int(A, B)      _IQ5mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQmpyI32int(A, B)      _IQ4mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQmpyI32int(A, B)      _IQ3mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQmpyI32int(A, B)      _IQ2mpyI32int(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQmpyI32int(A, B)      _IQ1mpyI32int(A, B)
#endif

//*****************************************************************************
//
// Multiplies an IQ number by an integer, and returns the fractional portion.
//
//*****************************************************************************
#define _IQ30mpyI32frac(A, B)   _IQ30frac(_IQ30mpyI32(A, B))
#define _IQ29mpyI32frac(A, B)   _IQ29frac(_IQ29mpyI32(A, B))
#define _IQ28mpyI32frac(A, B)   _IQ28frac(_IQ28mpyI32(A, B))
#define _IQ27mpyI32frac(A, B)   _IQ27frac(_IQ27mpyI32(A, B))
#define _IQ26mpyI32frac(A, B)   _IQ26frac(_IQ26mpyI32(A, B))
#define _IQ25mpyI32frac(A, B)   _IQ25frac(_IQ25mpyI32(A, B))
#define _IQ24mpyI32frac(A, B)   _IQ24frac(_IQ24mpyI32(A, B))
#define _IQ23mpyI32frac(A, B)   _IQ23frac(_IQ23mpyI32(A, B))
#define _IQ22mpyI32frac(A, B)   _IQ22frac(_IQ22mpyI32(A, B))
#define _IQ21mpyI32frac(A, B)   _IQ21frac(_IQ21mpyI32(A, B))
#define _IQ20mpyI32frac(A, B)   _IQ20frac(_IQ20mpyI32(A, B))
#define _IQ19mpyI32frac(A, B)   _IQ19frac(_IQ19mpyI32(A, B))
#define _IQ18mpyI32frac(A, B)   _IQ18frac(_IQ18mpyI32(A, B))
#define _IQ17mpyI32frac(A, B)   _IQ17frac(_IQ17mpyI32(A, B))
#define _IQ16mpyI32frac(A, B)   _IQ16frac(_IQ16mpyI32(A, B))
#define _IQ15mpyI32frac(A, B)   _IQ15frac(_IQ15mpyI32(A, B))
#define _IQ14mpyI32frac(A, B)   _IQ14frac(_IQ14mpyI32(A, B))
#define _IQ13mpyI32frac(A, B)   _IQ13frac(_IQ13mpyI32(A, B))
#define _IQ12mpyI32frac(A, B)   _IQ12frac(_IQ12mpyI32(A, B))
#define _IQ11mpyI32frac(A, B)   _IQ11frac(_IQ11mpyI32(A, B))
#define _IQ10mpyI32frac(A, B)   _IQ10frac(_IQ10mpyI32(A, B))
#define _IQ9mpyI32frac(A, B)   _IQ9frac(_IQ9mpyI32(A, B))
#define _IQ8mpyI32frac(A, B)   _IQ8frac(_IQ8mpyI32(A, B))
#define _IQ7mpyI32frac(A, B)   _IQ7frac(_IQ7mpyI32(A, B))
#define _IQ6mpyI32frac(A, B)   _IQ6frac(_IQ6mpyI32(A, B))
#define _IQ5mpyI32frac(A, B)   _IQ5frac(_IQ5mpyI32(A, B))
#define _IQ4mpyI32frac(A, B)   _IQ4frac(_IQ4mpyI32(A, B))
#define _IQ3mpyI32frac(A, B)   _IQ3frac(_IQ3mpyI32(A, B))
#define _IQ2mpyI32frac(A, B)   _IQ2frac(_IQ2mpyI32(A, B))
#define _IQ1mpyI32frac(A, B)   _IQ1frac(_IQ1mpyI32(A, B))

#if GLOBAL_IQ == 30
#define _IQmpyI32frac(A, B)     _IQ30mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQmpyI32frac(A, B)     _IQ29mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQmpyI32frac(A, B)     _IQ28mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQmpyI32frac(A, B)     _IQ27mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQmpyI32frac(A, B)     _IQ26mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQmpyI32frac(A, B)     _IQ25mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQmpyI32frac(A, B)     _IQ24mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQmpyI32frac(A, B)     _IQ23mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQmpyI32frac(A, B)     _IQ22mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQmpyI32frac(A, B)     _IQ21mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQmpyI32frac(A, B)     _IQ20mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQmpyI32frac(A, B)     _IQ19mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQmpyI32frac(A, B)     _IQ18mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQmpyI32frac(A, B)     _IQ17mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQmpyI32frac(A, B)     _IQ16mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQmpyI32frac(A, B)     _IQ15mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQmpyI32frac(A, B)     _IQ14mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQmpyI32frac(A, B)     _IQ13mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQmpyI32frac(A, B)     _IQ12mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQmpyI32frac(A, B)     _IQ11mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQmpyI32frac(A, B)     _IQ10mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQmpyI32frac(A, B)     _IQ9mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQmpyI32frac(A, B)     _IQ8mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQmpyI32frac(A, B)     _IQ7mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQmpyI32frac(A, B)     _IQ6mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQmpyI32frac(A, B)     _IQ5mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQmpyI32frac(A, B)     _IQ4mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQmpyI32frac(A, B)     _IQ3mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQmpyI32frac(A, B)     _IQ2mpyI32frac(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQmpyI32frac(A, B)     _IQ1mpyI32frac(A, B)
#endif

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

//*****************************************************************************
//
// Computes the square root of A^2 + B^2 using IQ numbers.
//
//*****************************************************************************
extern int32_t _IQmag(int32_t A, int32_t B);
/**
 * @brief Computes the square root of A^2 + B^2 using IQ30 numbers.
 *
 * @param A               IQ30 type input.
 * @param B               IQ30 type input
 *
 * @return                IQ30 result of magnitude operation.
 */
#define _IQ30mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ29 numbers.
 *
 * @param A               IQ29 type input.
 * @param B               IQ29 type input
 *
 * @return                IQ29 result of magnitude operation.
 */
#define _IQ29mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ28 numbers.
 *
 * @param A               IQ28 type input.
 * @param B               IQ28 type input
 *
 * @return                IQ28 result of magnitude operation.
 */
#define _IQ28mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ27 numbers.
 *
 * @param A               IQ27 type input.
 * @param B               IQ27 type input
 *
 * @return                IQ27 result of magnitude operation.
 */
#define _IQ27mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ26 numbers.
 *
 * @param A               IQ26 type input.
 * @param B               IQ26 type input
 *
 * @return                IQ26 result of magnitude operation.
 */
#define _IQ26mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ25 numbers.
 *
 * @param A               IQ25 type input.
 * @param B               IQ25 type input
 *
 * @return                IQ25 result of magnitude operation.
 */
#define _IQ25mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ24 numbers.
 *
 * @param A               IQ24 type input.
 * @param B               IQ24 type input
 *
 * @return                IQ24 result of magnitude operation.
 */
#define _IQ24mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ23 numbers.
 *
 * @param A               IQ23 type input.
 * @param B               IQ23 type input
 *
 * @return                IQ23 result of magnitude operation.
 */
#define _IQ23mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ22 numbers.
 *
 * @param A               IQ22 type input.
 * @param B               IQ22 type input
 *
 * @return                IQ22 result of magnitude operation.
 */
#define _IQ22mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ21 numbers.
 *
 * @param A               IQ21 type input.
 * @param B               IQ21 type input
 *
 * @return                IQ21 result of magnitude operation.
 */
#define _IQ21mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ20 numbers.
 *
 * @param A               IQ20 type input.
 * @param B               IQ20 type input
 *
 * @return                IQ20 result of magnitude operation.
 */
#define _IQ20mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ19 numbers.
 *
 * @param A               IQ19 type input.
 * @param B               IQ19 type input
 *
 * @return                IQ19 result of magnitude operation.
 */
#define _IQ19mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ18 numbers.
 *
 * @param A               IQ18 type input.
 * @param B               IQ18 type input
 *
 * @return                IQ18 result of magnitude operation.
 */
#define _IQ18mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ17 numbers.
 *
 * @param A               IQ17 type input.
 * @param B               IQ17 type input
 *
 * @return                IQ17 result of magnitude operation.
 */
#define _IQ17mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ16 numbers.
 *
 * @param A               IQ16 type input.
 * @param B               IQ16 type input
 *
 * @return                IQ16 result of magnitude operation.
 */
#define _IQ16mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ15 numbers.
 *
 * @param A               IQ15 type input.
 * @param B               IQ15 type input
 *
 * @return                IQ15 result of magnitude operation.
 */
#define _IQ15mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ14 numbers.
 *
 * @param A               IQ14 type input.
 * @param B               IQ14 type input
 *
 * @return                IQ14 result of magnitude operation.
 */
#define _IQ14mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ13 numbers.
 *
 * @param A               IQ13 type input.
 * @param B               IQ13 type input
 *
 * @return                IQ13 result of magnitude operation.
 */
#define _IQ13mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ12 numbers.
 *
 * @param A               IQ12 type input.
 * @param B               IQ12 type input
 *
 * @return                IQ12 result of magnitude operation.
 */
#define _IQ12mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ11 numbers.
 *
 * @param A               IQ11 type input.
 * @param B               IQ11 type input
 *
 * @return                IQ11 result of magnitude operation.
 */
#define _IQ11mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ10 numbers.
 *
 * @param A               IQ10 type input.
 * @param B               IQ10 type input
 *
 * @return                IQ10 result of magnitude operation.
 */
#define _IQ10mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ9 numbers.
 *
 * @param A               IQ9 type input.
 * @param B               IQ9 type input
 *
 * @return                IQ9 result of magnitude operation.
 */
#define _IQ9mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ8 numbers.
 *
 * @param A               IQ8 type input.
 * @param B               IQ8 type input
 *
 * @return                IQ8 result of magnitude operation.
 */
#define _IQ8mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ7 numbers.
 *
 * @param A               IQ7 type input.
 * @param B               IQ7 type input
 *
 * @return                IQ7 result of magnitude operation.
 */
#define _IQ7mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ6 numbers.
 *
 * @param A               IQ6 type input.
 * @param B               IQ6 type input
 *
 * @return                IQ6 result of magnitude operation.
 */
#define _IQ6mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ5 numbers.
 *
 * @param A               IQ5 type input.
 * @param B               IQ5 type input
 *
 * @return                IQ5 result of magnitude operation.
 */
#define _IQ5mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ4 numbers.
 *
 * @param A               IQ4 type input.
 * @param B               IQ4 type input
 *
 * @return                IQ4 result of magnitude operation.
 */
#define _IQ4mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ3 numbers.
 *
 * @param A               IQ3 type input.
 * @param B               IQ3 type input
 *
 * @return                IQ3 result of magnitude operation.
 */
#define _IQ3mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ2 numbers.
 *
 * @param A               IQ2 type input.
 * @param B               IQ2 type input
 *
 * @return                IQ2 result of magnitude operation.
 */
#define _IQ2mag(A, B)             _IQmag(A, B)
/**
 * @brief Computes the square root of A^2 + B^2 using IQ1 numbers.
 *
 * @param A               IQ1 type input.
 * @param B               IQ1 type input
 *
 * @return                IQ1 result of magnitude operation.
 */
#define _IQ1mag(A, B)             _IQmag(A, B)

//*****************************************************************************
//
// Computes the inverse square root of A^2 + B^2 using IQ numbers.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq29 _IQ30imag(_iq30 A, _iq30 B);
extern _iq29 _IQ29imag(_iq29 A, _iq29 B);
extern _iq28 _IQ28imag(_iq28 A, _iq28 B);
extern _iq27 _IQ27imag(_iq27 A, _iq27 B);
extern _iq26 _IQ26imag(_iq26 A, _iq26 B);
extern _iq25 _IQ25imag(_iq25 A, _iq25 B);
extern _iq24 _IQ24imag(_iq24 A, _iq24 B);
extern _iq23 _IQ23imag(_iq23 A, _iq23 B);
extern _iq22 _IQ22imag(_iq22 A, _iq22 B);
extern _iq21 _IQ21imag(_iq21 A, _iq21 B);
extern _iq20 _IQ20imag(_iq20 A, _iq20 B);
extern _iq19 _IQ19imag(_iq19 A, _iq19 B);
extern _iq18 _IQ18imag(_iq18 A, _iq18 B);
extern _iq17 _IQ17imag(_iq17 A, _iq17 B);
extern _iq16 _IQ16imag(_iq16 A, _iq16 B);
extern _iq15 _IQ15imag(_iq15 A, _iq15 B);
extern _iq14 _IQ14imag(_iq14 A, _iq14 B);
extern _iq13 _IQ13imag(_iq13 A, _iq13 B);
extern _iq12 _IQ12imag(_iq12 A, _iq12 B);
extern _iq11 _IQ11imag(_iq11 A, _iq11 B);
extern _iq10 _IQ10imag(_iq10 A, _iq10 B);
extern _iq9 _IQ9imag(_iq9 A, _iq9 B);
extern _iq8 _IQ8imag(_iq8 A, _iq8 B);
extern _iq7 _IQ7imag(_iq7 A, _iq7 B);
extern _iq6 _IQ6imag(_iq6 A, _iq6 B);
extern _iq5 _IQ5imag(_iq5 A, _iq5 B);
extern _iq4 _IQ4imag(_iq4 A, _iq4 B);
extern _iq3 _IQ3imag(_iq3 A, _iq3 B);
extern _iq2 _IQ2imag(_iq2 A, _iq2 B);
extern _iq1 _IQ1imag(_iq1 A, _iq1 B);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Computes the inverse square root of A^2 + B^2 using IQ1 numbers.
 *
 * @param A               Global IQ format input.
 * @param B               Global IQ format input
 *
 * @return                Global IQ  format result of inverse
 *                        magnitude operation.
 */
#if GLOBAL_IQ == 30
#define _IQimag(A, B)          _IQ30imag(A, B)
#endif
#if GLOBAL_IQ == 29
#define _IQimag(A, B)          _IQ29imag(A, B)
#endif
#if GLOBAL_IQ == 28
#define _IQimag(A, B)          _IQ28imag(A, B)
#endif
#if GLOBAL_IQ == 27
#define _IQimag(A, B)          _IQ27imag(A, B)
#endif
#if GLOBAL_IQ == 26
#define _IQimag(A, B)          _IQ26imag(A, B)
#endif
#if GLOBAL_IQ == 25
#define _IQimag(A, B)          _IQ25imag(A, B)
#endif
#if GLOBAL_IQ == 24
#define _IQimag(A, B)          _IQ24imag(A, B)
#endif
#if GLOBAL_IQ == 23
#define _IQimag(A, B)          _IQ23imag(A, B)
#endif
#if GLOBAL_IQ == 22
#define _IQimag(A, B)          _IQ22imag(A, B)
#endif
#if GLOBAL_IQ == 21
#define _IQimag(A, B)          _IQ21imag(A, B)
#endif
#if GLOBAL_IQ == 20
#define _IQimag(A, B)          _IQ20imag(A, B)
#endif
#if GLOBAL_IQ == 19
#define _IQimag(A, B)          _IQ19imag(A, B)
#endif
#if GLOBAL_IQ == 18
#define _IQimag(A, B)          _IQ18imag(A, B)
#endif
#if GLOBAL_IQ == 17
#define _IQimag(A, B)          _IQ17imag(A, B)
#endif
#if GLOBAL_IQ == 16
#define _IQimag(A, B)          _IQ16imag(A, B)
#endif
#if GLOBAL_IQ == 15
#define _IQimag(A, B)          _IQ15imag(A, B)
#endif
#if GLOBAL_IQ == 14
#define _IQimag(A, B)          _IQ14imag(A, B)
#endif
#if GLOBAL_IQ == 13
#define _IQimag(A, B)          _IQ13imag(A, B)
#endif
#if GLOBAL_IQ == 12
#define _IQimag(A, B)          _IQ12imag(A, B)
#endif
#if GLOBAL_IQ == 11
#define _IQimag(A, B)          _IQ11imag(A, B)
#endif
#if GLOBAL_IQ == 10
#define _IQimag(A, B)          _IQ10imag(A, B)
#endif
#if GLOBAL_IQ == 9
#define _IQimag(A, B)          _IQ9imag(A, B)
#endif
#if GLOBAL_IQ == 8
#define _IQimag(A, B)          _IQ8imag(A, B)
#endif
#if GLOBAL_IQ == 7
#define _IQimag(A, B)          _IQ7imag(A, B)
#endif
#if GLOBAL_IQ == 6
#define _IQimag(A, B)          _IQ6imag(A, B)
#endif
#if GLOBAL_IQ == 5
#define _IQimag(A, B)          _IQ5imag(A, B)
#endif
#if GLOBAL_IQ == 4
#define _IQimag(A, B)          _IQ4imag(A, B)
#endif
#if GLOBAL_IQ == 3
#define _IQimag(A, B)          _IQ3imag(A, B)
#endif
#if GLOBAL_IQ == 2
#define _IQimag(A, B)          _IQ2imag(A, B)
#endif
#if GLOBAL_IQ == 1
#define _IQimag(A, B)          _IQ1imag(A, B)
#endif

//*****************************************************************************
//
// Converts a string into an IQ number.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern _iq30 _atoIQ30(const char *A);
extern _iq29 _atoIQ29(const char *A);
extern _iq28 _atoIQ28(const char *A);
extern _iq27 _atoIQ27(const char *A);
extern _iq26 _atoIQ26(const char *A);
extern _iq25 _atoIQ25(const char *A);
extern _iq24 _atoIQ24(const char *A);
extern _iq23 _atoIQ23(const char *A);
extern _iq22 _atoIQ22(const char *A);
extern _iq21 _atoIQ21(const char *A);
extern _iq20 _atoIQ20(const char *A);
extern _iq19 _atoIQ19(const char *A);
extern _iq18 _atoIQ18(const char *A);
extern _iq17 _atoIQ17(const char *A);
extern _iq16 _atoIQ16(const char *A);
extern _iq15 _atoIQ15(const char *A);
extern _iq14 _atoIQ14(const char *A);
extern _iq13 _atoIQ13(const char *A);
extern _iq12 _atoIQ12(const char *A);
extern _iq11 _atoIQ11(const char *A);
extern _iq10 _atoIQ10(const char *A);
extern _iq9 _atoIQ9(const char *A);
extern _iq8 _atoIQ8(const char *A);
extern _iq7 _atoIQ7(const char *A);
extern _iq6 _atoIQ6(const char *A);
extern _iq5 _atoIQ5(const char *A);
extern _iq4 _atoIQ4(const char *A);
extern _iq3 _atoIQ3(const char *A);
extern _iq2 _atoIQ2(const char *A);
extern _iq1 _atoIQ1(const char *A);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Converts a string into a global IQ format number.
 *
 * @param A               String input.
 *
 * @return                Global IQ  format result of conversion.
 */
#if GLOBAL_IQ == 30
#define _atoIQ(A)     _atoIQ30(A)
#endif
#if GLOBAL_IQ == 29
#define _atoIQ(A)     _atoIQ29(A)
#endif
#if GLOBAL_IQ == 28
#define _atoIQ(A)     _atoIQ28(A)
#endif
#if GLOBAL_IQ == 27
#define _atoIQ(A)     _atoIQ27(A)
#endif
#if GLOBAL_IQ == 26
#define _atoIQ(A)     _atoIQ26(A)
#endif
#if GLOBAL_IQ == 25
#define _atoIQ(A)     _atoIQ25(A)
#endif
#if GLOBAL_IQ == 24
#define _atoIQ(A)     _atoIQ24(A)
#endif
#if GLOBAL_IQ == 23
#define _atoIQ(A)     _atoIQ23(A)
#endif
#if GLOBAL_IQ == 22
#define _atoIQ(A)     _atoIQ22(A)
#endif
#if GLOBAL_IQ == 21
#define _atoIQ(A)     _atoIQ21(A)
#endif
#if GLOBAL_IQ == 20
#define _atoIQ(A)     _atoIQ20(A)
#endif
#if GLOBAL_IQ == 19
#define _atoIQ(A)     _atoIQ19(A)
#endif
#if GLOBAL_IQ == 18
#define _atoIQ(A)     _atoIQ18(A)
#endif
#if GLOBAL_IQ == 17
#define _atoIQ(A)     _atoIQ17(A)
#endif
#if GLOBAL_IQ == 16
#define _atoIQ(A)     _atoIQ16(A)
#endif
#if GLOBAL_IQ == 15
#define _atoIQ(A)     _atoIQ15(A)
#endif
#if GLOBAL_IQ == 14
#define _atoIQ(A)     _atoIQ14(A)
#endif
#if GLOBAL_IQ == 13
#define _atoIQ(A)     _atoIQ13(A)
#endif
#if GLOBAL_IQ == 12
#define _atoIQ(A)     _atoIQ12(A)
#endif
#if GLOBAL_IQ == 11
#define _atoIQ(A)     _atoIQ11(A)
#endif
#if GLOBAL_IQ == 10
#define _atoIQ(A)     _atoIQ10(A)
#endif
#if GLOBAL_IQ == 9
#define _atoIQ(A)     _atoIQ9(A)
#endif
#if GLOBAL_IQ == 8
#define _atoIQ(A)     _atoIQ8(A)
#endif
#if GLOBAL_IQ == 7
#define _atoIQ(A)     _atoIQ7(A)
#endif
#if GLOBAL_IQ == 6
#define _atoIQ(A)     _atoIQ6(A)
#endif
#if GLOBAL_IQ == 5
#define _atoIQ(A)     _atoIQ5(A)
#endif
#if GLOBAL_IQ == 4
#define _atoIQ(A)     _atoIQ4(A)
#endif
#if GLOBAL_IQ == 3
#define _atoIQ(A)     _atoIQ3(A)
#endif
#if GLOBAL_IQ == 2
#define _atoIQ(A)     _atoIQ2(A)
#endif
#if GLOBAL_IQ == 1
#define _atoIQ(A)     _atoIQ1(A)
#endif

//*****************************************************************************
//
// Converts an IQ number into a string.
//
//*****************************************************************************
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern int16_t _IQ30toa(char *string, const char *format, _iq30 input);
extern int16_t _IQ29toa(char *string, const char *format, _iq29 input);
extern int16_t _IQ28toa(char *string, const char *format, _iq28 input);
extern int16_t _IQ27toa(char *string, const char *format, _iq27 input);
extern int16_t _IQ26toa(char *string, const char *format, _iq26 input);
extern int16_t _IQ25toa(char *string, const char *format, _iq25 input);
extern int16_t _IQ24toa(char *string, const char *format, _iq24 input);
extern int16_t _IQ23toa(char *string, const char *format, _iq23 input);
extern int16_t _IQ22toa(char *string, const char *format, _iq22 input);
extern int16_t _IQ21toa(char *string, const char *format, _iq21 input);
extern int16_t _IQ20toa(char *string, const char *format, _iq20 input);
extern int16_t _IQ19toa(char *string, const char *format, _iq19 input);
extern int16_t _IQ18toa(char *string, const char *format, _iq18 input);
extern int16_t _IQ17toa(char *string, const char *format, _iq17 input);
extern int16_t _IQ16toa(char *string, const char *format, _iq16 input);
extern int16_t _IQ15toa(char *string, const char *format, _iq15 input);
extern int16_t _IQ14toa(char *string, const char *format, _iq14 input);
extern int16_t _IQ13toa(char *string, const char *format, _iq13 input);
extern int16_t _IQ12toa(char *string, const char *format, _iq12 input);
extern int16_t _IQ11toa(char *string, const char *format, _iq11 input);
extern int16_t _IQ10toa(char *string, const char *format, _iq10 input);
extern int16_t _IQ9toa(char *string, const char *format, _iq9 input);
extern int16_t _IQ8toa(char *string, const char *format, _iq8 input);
extern int16_t _IQ7toa(char *string, const char *format, _iq7 input);
extern int16_t _IQ6toa(char *string, const char *format, _iq6 input);
extern int16_t _IQ5toa(char *string, const char *format, _iq5 input);
extern int16_t _IQ4toa(char *string, const char *format, _iq4 input);
extern int16_t _IQ3toa(char *string, const char *format, _iq3 input);
extern int16_t _IQ2toa(char *string, const char *format, _iq2 input);
extern int16_t _IQ1toa(char *string, const char *format, _iq1 input);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Converts a global IQ format input into a string.
 *
 * @param A               Pointer to the buffer to store the converted IQ number.
 * @param B               The format string specifying how to convert the IQ number.
 * @param C               Global IQ format input.
 *
 * @return                Returns 0 if there is no error, 1 if the width is too small to hold the integer
 *                        characters, and 2 if an illegal format was specified.
 */
#if GLOBAL_IQ == 30
#define _IQtoa(A, B, C)     _IQ30toa(A, B, C)
#endif
#if GLOBAL_IQ == 29
#define _IQtoa(A, B, C)     _IQ29toa(A, B, C)
#endif
#if GLOBAL_IQ == 28
#define _IQtoa(A, B, C)     _IQ28toa(A, B, C)
#endif
#if GLOBAL_IQ == 27
#define _IQtoa(A, B, C)     _IQ27toa(A, B, C)
#endif
#if GLOBAL_IQ == 26
#define _IQtoa(A, B, C)     _IQ26toa(A, B, C)
#endif
#if GLOBAL_IQ == 25
#define _IQtoa(A, B, C)     _IQ25toa(A, B, C)
#endif
#if GLOBAL_IQ == 24
#define _IQtoa(A, B, C)     _IQ24toa(A, B, C)
#endif
#if GLOBAL_IQ == 23
#define _IQtoa(A, B, C)     _IQ23toa(A, B, C)
#endif
#if GLOBAL_IQ == 22
#define _IQtoa(A, B, C)     _IQ22toa(A, B, C)
#endif
#if GLOBAL_IQ == 21
#define _IQtoa(A, B, C)     _IQ21toa(A, B, C)
#endif
#if GLOBAL_IQ == 20
#define _IQtoa(A, B, C)     _IQ20toa(A, B, C)
#endif
#if GLOBAL_IQ == 19
#define _IQtoa(A, B, C)     _IQ19toa(A, B, C)
#endif
#if GLOBAL_IQ == 18
#define _IQtoa(A, B, C)     _IQ18toa(A, B, C)
#endif
#if GLOBAL_IQ == 17
#define _IQtoa(A, B, C)     _IQ17toa(A, B, C)
#endif
#if GLOBAL_IQ == 16
#define _IQtoa(A, B, C)     _IQ16toa(A, B, C)
#endif
#if GLOBAL_IQ == 15
#define _IQtoa(A, B, C)     _IQ15toa(A, B, C)
#endif
#if GLOBAL_IQ == 14
#define _IQtoa(A, B, C)     _IQ14toa(A, B, C)
#endif
#if GLOBAL_IQ == 13
#define _IQtoa(A, B, C)     _IQ13toa(A, B, C)
#endif
#if GLOBAL_IQ == 12
#define _IQtoa(A, B, C)     _IQ12toa(A, B, C)
#endif
#if GLOBAL_IQ == 11
#define _IQtoa(A, B, C)     _IQ11toa(A, B, C)
#endif
#if GLOBAL_IQ == 10
#define _IQtoa(A, B, C)     _IQ10toa(A, B, C)
#endif
#if GLOBAL_IQ == 9
#define _IQtoa(A, B, C)     _IQ9toa(A, B, C)
#endif
#if GLOBAL_IQ == 8
#define _IQtoa(A, B, C)     _IQ8toa(A, B, C)
#endif
#if GLOBAL_IQ == 7
#define _IQtoa(A, B, C)     _IQ7toa(A, B, C)
#endif
#if GLOBAL_IQ == 6
#define _IQtoa(A, B, C)     _IQ6toa(A, B, C)
#endif
#if GLOBAL_IQ == 5
#define _IQtoa(A, B, C)     _IQ5toa(A, B, C)
#endif
#if GLOBAL_IQ == 4
#define _IQtoa(A, B, C)     _IQ4toa(A, B, C)
#endif
#if GLOBAL_IQ == 3
#define _IQtoa(A, B, C)     _IQ3toa(A, B, C)
#endif
#if GLOBAL_IQ == 2
#define _IQtoa(A, B, C)     _IQ2toa(A, B, C)
#endif
#if GLOBAL_IQ == 1
#define _IQtoa(A, B, C)     _IQ1toa(A, B, C)
#endif

//*****************************************************************************
//
// Computes the absolute value of an IQ number.
//
//*****************************************************************************
/**
 * @brief Computes the absolute value of an IQ30 number.
 *
 * @param A               IQ30 type input.
 *
 * @return                IQ30 type absolute value of input.
 */
#define _IQ30abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ29 number.
 *
 * @param A               IQ29 type input.
 *
 * @return                IQ29 type absolute value of input.
 */
#define _IQ29abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ28 number.
 *
 * @param A               IQ28 type input.
 *
 * @return                IQ28 type absolute value of input.
 */
#define _IQ28abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ27 number.
 *
 * @param A               IQ27 type input.
 *
 * @return                IQ27 type absolute value of input.
 */
#define _IQ27abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ26 number.
 *
 * @param A               IQ26 type input.
 *
 * @return                IQ26 type absolute value of input.
 */
#define _IQ26abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ25 number.
 *
 * @param A               IQ25 type input.
 *
 * @return                IQ25 type absolute value of input.
 */
#define _IQ25abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ24 number.
 *
 * @param A               IQ24 type input.
 *
 * @return                IQ24 type absolute value of input.
 */
#define _IQ24abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ23 number.
 *
 * @param A               IQ23 type input.
 *
 * @return                IQ23 type absolute value of input.
 */
#define _IQ23abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ22 number.
 *
 * @param A               IQ22 type input.
 *
 * @return                IQ22 type absolute value of input.
 */
#define _IQ22abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ21 number.
 *
 * @param A               IQ21 type input.
 *
 * @return                IQ21 type absolute value of input.
 */
#define _IQ21abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ20 number.
 *
 * @param A               IQ20 type input.
 *
 * @return                IQ20 type absolute value of input.
 */
#define _IQ20abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ19 number.
 *
 * @param A               IQ19 type input.
 *
 * @return                IQ19 type absolute value of input.
 */
#define _IQ19abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ18 number.
 *
 * @param A               IQ18 type input.
 *
 * @return                IQ18 type absolute value of input.
 */
#define _IQ18abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ17 number.
 *
 * @param A               IQ17 type input.
 *
 * @return                IQ17 type absolute value of input.
 */
#define _IQ17abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ16 number.
 *
 * @param A               IQ16 type input.
 *
 * @return                IQ16 type absolute value of input.
 */
#define _IQ16abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ15 number.
 *
 * @param A               IQ15 type input.
 *
 * @return                IQ15 type absolute value of input.
 */
#define _IQ15abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ14 number.
 *
 * @param A               IQ14 type input.
 *
 * @return                IQ14 type absolute value of input.
 */
#define _IQ14abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ13 number.
 *
 * @param A               IQ13 type input.
 *
 * @return                IQ13 type absolute value of input.
 */
#define _IQ13abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ12 number.
 *
 * @param A               IQ12 type input.
 *
 * @return                IQ12 type absolute value of input.
 */
#define _IQ12abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ11 number.
 *
 * @param A               IQ11 type input.
 *
 * @return                IQ11 type absolute value of input.
 */
#define _IQ11abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ10 number.
 *
 * @param A               IQ10 type input.
 *
 * @return                IQ10 type absolute value of input.
 */
#define _IQ10abs(A)             (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ9 number.
 *
 * @param A               IQ9 type input.
 *
 * @return                IQ9 type absolute value of input.
 */
#define _IQ9abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ8 number.
 *
 * @param A               IQ8 type input.
 *
 * @return                IQ8 type absolute value of input.
 */
#define _IQ8abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ7 number.
 *
 * @param A               IQ7 type input.
 *
 * @return                IQ7 type absolute value of input.
 */
#define _IQ7abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ6 number.
 *
 * @param A               IQ6 type input.
 *
 * @return                IQ6 type absolute value of input.
 */
#define _IQ6abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ5 number.
 *
 * @param A               IQ5 type input.
 *
 * @return                IQ5 type absolute value of input.
 */
#define _IQ5abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ4 number.
 *
 * @param A               IQ4 type input.
 *
 * @return                IQ4 type absolute value of input.
 */
#define _IQ4abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ3 number.
 *
 * @param A               IQ3 type input.
 *
 * @return                IQ3 type absolute value of input.
 */
#define _IQ3abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ2 number.
 *
 * @param A               IQ2 type input.
 *
 * @return                IQ2 type absolute value of input.
 */
#define _IQ2abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an IQ1 number.
 *
 * @param A               IQ1 type input.
 *
 * @return                IQ1 type absolute value of input.
 */
#define _IQ1abs(A)              (((A) < 0) ? - (A) : (A))
/**
 * @brief Computes the absolute value of an global IQ format number.
 *
 * @param A               Global IQ format input.
 *
 * @return                GlobalIQ format absolute value of input.
 */
#define _IQabs(A)               (((A) < 0) ? - (A) : (A))

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif // __IQMATHLIB_H__
