/*___________________________________________________________________________
 |                                                                           |
 |   Constants and Globals                                                   |
 |___________________________________________________________________________|
*/
extern Flag Overflow;
extern Flag Carry;

#define MAX_32 (Word32)0x7fffffffL
#define MIN_32 (Word32)0x80000000L

#define MAX_16 (Word16)0x7fff
#define MIN_16 (Word16)0x8000

#ifdef RALINK_VOIP_CVERSION
#else
//============== hc+ for G168 ==================
#define sature(L_var1) (Word16)( (L_var1 > 0X00007fffL) ? MAX_16 : ( (L_var1 < (Word32)0xffff8000L) ? (Word16)MIN_16 : extract_l(L_var1) ) )
//=================== end ======================
#define extract_h(a)	(Word16)(a>>16)
#define extract_l(a)	(Word16)(a)
#define L_deposit_h(a)	(Word32)(((Word32)a)<<16)
#define L_deposit_l(a)	(Word32)(a)
#endif
/*___________________________________________________________________________
 |                                                                           |
 |   Operators prototypes                                                    |
 |___________________________________________________________________________|
*/

#ifndef RALINK_VOIP_CVERSION
#include "basic_op_inline.h"
#define add add_inline
#define sub sub_inline
#define abs_s abs_s_inline
#define shl shl_inline
#define shr shr_inline
#define L_add L_add_inline
#define L_sub L_sub_inline
#define L_mac L_mac_inline
#define L_msu L_msu_inline
#define mult	mult_inline
#define mult_r	mult_r_inline
#define L_mult L_mult_inline
#define negate	negate_inline
#define L_negate L_negate_inline
#define round	round_inline
#define L_abs	L_abs_inline
//#define L_shl	L_shl_inline
#define L_shr	L_shr_inline
#else
Word16 sature(Word32 L_var1);             /* Limit to 16 bits,    1 */
Word16 add(Word16 var1, Word16 var2);     /* Short add,           1 */
Word16 sub(Word16 var1, Word16 var2);     /* Short sub,           1 */
Word16 abs_s(Word16 var1);                /* Short abs,           1 */
Word16 shl(Word16 var1, Word16 var2);     /* Short shift left,    1 */
Word16 shr(Word16 var1, Word16 var2);     /* Short shift right,   1 */
Word16 mult(Word16 var1, Word16 var2);    /* Short mult,          1 */
Word32 L_mult(Word16 var1, Word16 var2);  /* Long mult,           1 */
Word16 negate(Word16 var1);               /* Short negate,        1 */
Word16 extract_h(Word32 L_var1);          /* Extract high,        1 */
Word16 extract_l(Word32 L_var1);          /* Extract low,         1 */
Word16 round(Word32 L_var1);              /* Round,               1 */
Word32 L_mac(Word32 L_var3, Word16 var1, Word16 var2); /* Mac,    1 */
Word32 L_msu(Word32 L_var3, Word16 var1, Word16 var2); /* Msu,    1 */
Word32 L_add(Word32 L_var1, Word32 L_var2);   /* Long add,        2 */
Word32 L_sub(Word32 L_var1, Word32 L_var2);   /* Long sub,        2 */
Word32 L_negate(Word32 L_var1);               /* Long negate,     2 */
Word16 mult_r(Word16 var1, Word16 var2);  /* Mult with round,     2 */
Word32 L_shr(Word32 L_var1, Word16 var2); /* Long shift right,    2 */
Word32 L_abs(Word32 L_var1);            /* Long abs,              3 */
Word32 L_deposit_h(Word16 var1);       /* 16 bit var1 -> MSB,     2 */
Word32 L_deposit_l(Word16 var1);       /* 16 bit var1 -> LSB,     2 */
#endif

Word32 L_macNs(Word32 L_var3, Word16 var1, Word16 var2);/* Mac without sat, 1*/
Word32 L_msuNs(Word32 L_var3, Word16 var1, Word16 var2);/* Msu without sat, 1*/
Word32 L_add_c(Word32 L_var1, Word32 L_var2); /*Long add with c,  2 */
Word32 L_sub_c(Word32 L_var1, Word32 L_var2); /*Long sub with c,  2 */
Word32 L_shl(Word32 L_var1, Word16 var2); /* Long shift left,     2 */
Word16 shr_r(Word16 var1, Word16 var2);/* Shift right with round, 2 */
Word16 mac_r(Word32 L_var3, Word16 var1, Word16 var2);/* Mac with rounding, 2*/
Word16 msu_r(Word32 L_var3, Word16 var1, Word16 var2);/* Msu with rounding, 2*/
Word32 L_shr_r(Word32 L_var1, Word16 var2);/* Long shift right with round,  3*/
Word32 L_sat(Word32 L_var1);            /* Long saturation,       4 */
Word16 norm_s(Word16 var1);             /* Short norm,           15 */
Word16 div_s(Word16 var1, Word16 var2); /* Short division,       18 */
Word16 norm_l(Word32 L_var1);           /* Long norm,            30 */

