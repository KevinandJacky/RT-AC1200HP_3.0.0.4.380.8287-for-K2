/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
   ITU-T G.729A Speech Coder    ANSI-C Source Code
   Version 1.1    Last modified: September 1996

   Copyright (c) 1996,
   AT&T, France Telecom, NTT, Universite de Sherbrooke, Lucent Technologies
   All rights reserved.
*/

/**************************************************************************
 * Taming functions.                                                      *
 **************************************************************************/

#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "ld8a.h"
#include "tab_ld8a.h"

#include "cod_struct.h"

#if 0
static Word32 L_exc_err[4];
#endif

void Init_exc_err(void)
{
  Word16 i;
  for(i=0; i<4; i++) pg729enc->ptamping->L_exc_err[i] = 0x00004000L;   /* Q14 */
}

/**************************************************************************
 * routine test_err - computes the accumulated potential error in the     *
 * adaptive codebook contribution                                         *
 **************************************************************************/

Word16 test_err(  /* (o) flag set to 1 if taming is necessary  */
 Word16 T0,       /* (i) integer part of pitch delay           */
 Word16 T0_frac   /* (i) fractional part of pitch delay        */
)
 {
    Word16 i, t1, zone1, zone2, flag;
    Word32 L_maxloc, L_acc;

	Word32* L_exc_err = pg729enc->ptamping->L_exc_err;
	
    if(T0_frac > 0) {
        t1 = add(T0, 1);
    }
    else {
        t1 = T0;
    }

    i = sub(t1, (L_SUBFR+L_INTER10));
    if(i < 0) {
        i = 0;
    }
    zone1 = tab_zone[i];

    i = add(t1, (L_INTER10 - 2));
    zone2 = tab_zone[i];

    L_maxloc = -1L;
    flag = 0 ;
    for(i=zone2; i>=zone1; i--) {
        L_acc = L_sub(L_exc_err[i], L_maxloc);
        if(L_acc > 0L) {
                L_maxloc = L_exc_err[i];
        }
    }
    L_acc = L_sub(L_maxloc, L_THRESH_ERR);
    if(L_acc > 0L) {
        flag = 1;
    }

    return(flag);
}

/**************************************************************************
 *routine update_exc_err - maintains the memory used to compute the error *
 * function due to an adaptive codebook mismatch between encoder and      *
 * decoder                                                                *
 **************************************************************************/

void update_exc_err(
 Word16 gain_pit,      /* (i) pitch gain */
 Word16 T0             /* (i) integer part of pitch delay */
)
 {

    Word16 i, zone1, zone2, n;
    Word32 L_worst, L_temp, L_acc;
    Word16 hi, lo;

	Word32* L_exc_err = pg729enc->ptamping->L_exc_err;
	
    L_worst = -1L;
    n = sub(T0, L_SUBFR);

    if(n < 0) {
        L_Extract(L_exc_err[0], &hi, &lo);
        L_temp = Mpy_32_16(hi, lo, gain_pit);
        L_temp = L_shl(L_temp, 1);
        L_temp = L_add(0x00004000L, L_temp);
        L_acc = L_sub(L_temp, L_worst);
        if(L_acc > 0L) {
                L_worst = L_temp;
        }
        L_Extract(L_temp, &hi, &lo);
        L_temp = Mpy_32_16(hi, lo, gain_pit);
        L_temp = L_shl(L_temp, 1);
        L_temp = L_add(0x00004000L, L_temp);
        L_acc = L_sub(L_temp, L_worst);
        if(L_acc > 0L) {
                L_worst = L_temp;
        }
    }

    else {

        zone1 = tab_zone[n];

        i = sub(T0, 1);
        zone2 = tab_zone[i];

        for(i = zone1; i <= zone2; i++) {
                L_Extract(L_exc_err[i], &hi, &lo);
                L_temp = Mpy_32_16(hi, lo, gain_pit);
                L_temp = L_shl(L_temp, 1);
                L_temp = L_add(0x00004000L, L_temp);
                L_acc = L_sub(L_temp, L_worst);
                if(L_acc > 0L) L_worst = L_temp;
        }
    }

    for(i=3; i>=1; i--) {
        L_exc_err[i] = L_exc_err[i-1];
    }
    L_exc_err[0] = L_worst;

    return;
}

