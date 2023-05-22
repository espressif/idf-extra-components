#ifndef _IQNTABLES_H_
#define _IQNTABLES_H_

#include <stdint.h>

/* LOG lookup and coefficient tables. */
#define _IQ30log_order  14
extern const uint_fast32_t _IQNlog_min[5];
extern const uint_fast32_t _IQ30log_coeffs[15];

/* asin and acos coefficient table. */
extern const int_fast32_t _IQ29Asin_coeffs[17][5];

/* sin and cos lookup tables. */
extern const int_fast32_t _IQ31CosLookup[52];
extern const int_fast32_t _IQ31SinLookup[52];

/* atan coefficient table. */
extern const int_fast32_t _IQ32atan_coeffs[132];

/* Tables for exp function. Min/Max and integer lookup for each q type */
#define _IQ30exp_order  10
extern const uint_fast32_t _IQNexp_min[30];
extern const uint_fast32_t _IQNexp_max[30];
extern const uint_fast16_t _IQNexp_offset[30];
extern const uint_fast32_t _IQNexp_lookup1[22];
extern const uint_fast32_t _IQNexp_lookup2[22];
extern const uint_fast32_t _IQNexp_lookup3[22];
extern const uint_fast32_t _IQNexp_lookup4[22];
extern const uint_fast32_t _IQNexp_lookup5[22];
extern const uint_fast32_t _IQNexp_lookup6[22];
extern const uint_fast32_t _IQNexp_lookup7[22];
extern const uint_fast32_t _IQNexp_lookup8[22];
extern const uint_fast32_t _IQNexp_lookup9[22];
extern const uint_fast32_t _IQNexp_lookup10[22];
extern const uint_fast32_t _IQNexp_lookup11[22];
extern const uint_fast32_t _IQNexp_lookup12[22];
extern const uint_fast32_t _IQNexp_lookup13[22];
extern const uint_fast32_t _IQNexp_lookup14[22];
extern const uint_fast32_t _IQNexp_lookup15[22];
extern const uint_fast32_t _IQNexp_lookup16[22];
extern const uint_fast32_t _IQNexp_lookup17[22];
extern const uint_fast32_t _IQNexp_lookup18[22];
extern const uint_fast32_t _IQNexp_lookup19[22];
extern const uint_fast32_t _IQNexp_lookup20[22];
extern const uint_fast32_t _IQNexp_lookup21[22];
extern const uint_fast32_t _IQNexp_lookup22[22];
extern const uint_fast32_t _IQNexp_lookup23[22];
extern const uint_fast32_t _IQNexp_lookup24[22];
extern const uint_fast32_t _IQNexp_lookup25[22];
extern const uint_fast32_t _IQNexp_lookup26[22];
extern const uint_fast32_t _IQNexp_lookup27[22];
extern const uint_fast32_t _IQNexp_lookup28[22];
extern const uint_fast32_t _IQNexp_lookup29[22];
extern const uint_fast32_t _IQNexp_lookup30[22];
extern const uint_fast32_t _IQ30exp_coeffs[11];

/*
 *  Q0.15 lookup table for 1/2x best guess.
 */
extern const uint8_t _IQ6div_lookup[65];

/*
 *  Q0.15 lookup table for 1/(2*sqrt(x)) best guess.
 *  96 entries gives us enough accuracy to only need 2 iterations.
 */
extern const uint_fast16_t _IQ14sqrt_lookup[96];

/*
 * Lookup table for shifting using the multiplier.
 * Right: Index is the shift count, result is high 32 bits.
 * Left: Index is 32 - count, result is low (and high) 32 bits.
 */
extern const uint_fast32_t _IQNshift32[32];

#endif
