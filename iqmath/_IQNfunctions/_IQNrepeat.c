#include <stdint.h>

#include "../support/support.h"

#if ((defined (__IQMATH_USE_MATHACL__)) && (defined (__MSPM0_HAS_MATHACL__)))
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__IQNmpy)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
/**
 * @brief Repeats the last IQMath multiplication or division operation on two given parameters.
 *        Function assumes MathACL Control register has been initialized by previous function call
 *        with operation and IQ format. Using without initializing can lead to unexpected results.
 *
 * @param iqNInput1       IQN format number to be multiplied or divided.
 * @param iqNInput2       IQN format number to be multiplied or divided by.
 *
 * @return                IQN type result of operation.
 */
__STATIC_INLINE int_fast32_t __IQopRepeat(int_fast32_t iqNInput1, int_fast32_t iqNInput2)
{
    /* write operands to HWA */
    MATHACL->OP2 = iqNInput2;
    /* write trigger word last */
    MATHACL->OP1 = iqNInput1;
    /* read operation result */
    return MATHACL->RES1;
}

/**
 * @brief Repeats the last IQMath multiplication or division operation on two given parameters.
 *        Function assumes MathACL Control register has been initialized by previous function call
 *        with operation and IQ format. Using without initializing can lead to unexpected results.
 *
 * @param A               IQN format number to be multiplied or divided.
 * @param B               IQN format number to be multiplied or divided by.
 *
 * @return                IQN type result of operation.
 */
int32_t _IQrepeat(int32_t A, int32_t B)
{
    return __IQopRepeat(A, B);
}


#endif


