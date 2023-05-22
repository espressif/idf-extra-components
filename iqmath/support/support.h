#ifndef __SUPPORTH__
#define __SUPPORTH__

#include <math.h>
#include "esp_attr.h"
#include "RTS_support.h"

#define __STATIC_INLINE FORCE_INLINE_ATTR

/* Common value defines. */
#define q15_ln2          0x58b9
#define q13_pi           0x6488
#define q14_pi           0xc910
#define q14_halfPi       0x6488
#define q14_quarterPi    0x3244
#define q15_halfPi       0xc910
#define q15_quarterPi    0x6488
#define q15_invRoot2     0x5a82
#define q15_tanSmall     0x0021
#define q15_pointOne     0x0ccd
#define q15_oneTenth     0x0ccd
#define iq28_twoPi       0x6487ed51
#define iq29_pi          0x6487ed51
#define iq29_halfPi      0x3243f6a8
#define iq30_pi          0xc90fdaa2
#define iq30_halfPi      0x6487ed51
#define iq30_quarterPi   0x3243f6a8
#define iq31_halfPi      0xc90fdaa2
#define iq31_quarterPi   0x6487ed51
#define iq31_invRoot2    0x5a82799a
#define iq31_tanSmall    0x0020c49b
#define iq31_ln2         0x58b90bfc
#define iq31_twoThird    0x55555555
#define iq31_pointOne    0x0ccccccd
#define iq31_oneTenth    0x0ccccccd
#define iq31_one         0x7fffffff

#endif //__SUPPORTH__
