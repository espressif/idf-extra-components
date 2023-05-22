#ifndef __RTS_SUPPORTH__
#define __RTS_SUPPORTH__

////////////////////////////////////////////////////////////
//                                                        //
//              MPY32 control functions.                  //
//                                                        //
////////////////////////////////////////////////////////////
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_start)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline void __mpy_start(uint_fast16_t *ui16IntState, uint_fast16_t *ui16MPYState)
{
    /* Do nothing. */
    return;
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_start)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline void __mpyf_start(uint_fast16_t *ui16IntState, uint_fast16_t *ui16MPYState)
{
    /* Do nothing. */
    return;
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyfs_start)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline void __mpyfs_start(uint_fast16_t *ui16IntState, uint_fast16_t *ui16MPYState)
{
    /* Do nothing. */
    return;
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_clear_ctl0)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline void __mpy_clear_ctl0(void)
{
    /* Do nothing. */
    return;
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_set_frac)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline void __mpy_set_frac(void)
{
    /* Do nothing. */
    return;
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_stop)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline void __mpy_stop(uint_fast16_t *ui16IntState, uint_fast16_t *ui16MPYState)
{
    /* Do nothing. */
    return;
}

////////////////////////////////////////////////////////////
//                                                        //
//                16-bit functions                        //
//                                                        //
////////////////////////////////////////////////////////////
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_w)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast16_t __mpy_w(int_fast16_t arg1, int_fast16_t arg2)
{
    return (arg1 * arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_uw)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast16_t __mpy_uw(uint_fast16_t arg1, uint_fast16_t arg2)
{
    return (arg1 * arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyx_w)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpyx_w(int_fast16_t arg1, int_fast16_t arg2)
{
    return ((int_fast32_t)arg1 * (int_fast32_t)arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyx_uw)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast32_t __mpyx_uw(uint_fast16_t arg1, uint_fast16_t arg2)
{
    return ((uint_fast32_t)arg1 * (uint_fast32_t)arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_w)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast16_t __mpyf_w(int_fast16_t arg1, int_fast16_t arg2)
{
    return (int_fast16_t)(((int_fast32_t)arg1 * (int_fast32_t)arg2) >> 15);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_w_reuse_arg1)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast16_t __mpyf_w_reuse_arg1(int_fast16_t arg1, int_fast16_t arg2)
{
    /* This is identical to __mpyf_w */
    return (int_fast16_t)(((int_fast32_t)arg1 * (int_fast32_t)arg2) >> 15);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_uw)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast16_t __mpyf_uw(uint_fast16_t arg1, uint_fast16_t arg2)
{
    return (uint_fast16_t)(((uint_fast32_t)arg1 * (uint_fast32_t)arg2) >> 15);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_uw_reuse_arg1)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast16_t __mpyf_uw_reuse_arg1(uint_fast16_t arg1, uint_fast16_t arg2)
{
    /* This is identical to __mpyf_uw */
    return (uint_fast16_t)(((uint_fast32_t)arg1 * (uint_fast32_t)arg2) >> 15);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyfx_w)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpyfx_w(int_fast16_t arg1, int_fast16_t arg2)
{
    return (((int_fast32_t)arg1 * (int_fast32_t)arg2) << 1);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyfx_uw)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpyfx_uw(uint_fast16_t arg1, uint_fast16_t arg2)
{
    return (((uint_fast32_t)arg1 * (uint_fast32_t)arg2) << 1);
}


////////////////////////////////////////////////////////////
//                                                        //
//                 32-bit functions                       //
//                                                        //
////////////////////////////////////////////////////////////
#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_l)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpy_l(int_fast32_t arg1, int_fast32_t arg2)
{
    return (arg1 * arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpy_ul)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast32_t __mpy_ul(uint_fast32_t arg1, uint_fast32_t arg2)
{
    return (arg1 * arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyx)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast64_t __mpyx(int_fast32_t arg1, int_fast32_t arg2)
{
    return ((int_fast64_t)arg1 * (int_fast64_t)arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyx_u)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast64_t __mpyx_u(uint_fast32_t arg1, uint_fast32_t arg2)
{
    return ((uint_fast64_t)arg1 * (uint_fast64_t)arg2);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_l)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpyf_l(int_fast32_t arg1, int_fast32_t arg2)
{
    return (int_fast32_t)(((int_fast64_t)arg1 * (int_fast64_t)arg2) >> 31);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_l_reuse_arg1)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpyf_l_reuse_arg1(int_fast32_t arg1, int_fast32_t arg2)
{
    /* This is identical to __mpyf_l */
    return (int_fast32_t)(((int_fast64_t)arg1 * (int_fast64_t)arg2) >> 31);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_ul)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast32_t __mpyf_ul(uint_fast32_t arg1, uint_fast32_t arg2)
{
    return (uint_fast32_t)(((uint_fast64_t)arg1 * (uint_fast64_t)arg2) >> 31);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyf_ul_reuse_arg1)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast32_t __mpyf_ul_reuse_arg1(uint_fast32_t arg1, uint_fast32_t arg2)
{
    /* This is identical to __mpyf_ul */
    return (uint_fast32_t)(((uint_fast64_t)arg1 * (uint_fast64_t)arg2) >> 31);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyfx)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline int_fast64_t __mpyfx(int_fast32_t arg1, int_fast32_t arg2)
{
    return (((int_fast64_t)arg1 * (int_fast64_t)arg2) << 1);
}

#if defined (__TI_COMPILER_VERSION__)
#pragma FUNC_ALWAYS_INLINE(__mpyfx_u)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma inline=forced
#endif
static inline uint_fast64_t __mpyfx_u(uint_fast32_t arg1, uint_fast32_t arg2)
{
    return (((uint_fast64_t)arg1 * (uint_fast64_t)arg2) << 1);
}

#endif //__RTS_SUPPORTH__
