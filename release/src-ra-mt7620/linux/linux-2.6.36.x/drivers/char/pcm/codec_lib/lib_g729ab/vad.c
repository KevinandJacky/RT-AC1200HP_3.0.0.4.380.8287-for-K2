/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
   ITU-T G.729A Annex B     ANSI-C Source Code
   Version 1.3    Last modified: August 1997
   Copyright (c) 1996, France Telecom, Rockwell International,
                       Universite de Sherbrooke.
   All rights reserved.
*/

//#include <stdio.h>
#include "typedef.h"
#include "ld8a.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "tab_ld8a.h"
#include "vad.h"
#include "dtx.h"
#include "tab_dtx.h"

#include "cod_struct.h"

/* local function */
static Word16 MakeDec(
               Word16 dSLE,    /* (i)  : differential low band energy */
               Word16 dSE,     /* (i)  : differential full band energy */
               Word16 SD,      /* (i)  : differential spectral distortion */
               Word16 dSZC     /* (i)  : differential zero crossing rate */
);

#if 0
/* static variables */
static Word16 MeanLSF[M];
static Word16 Min_buffer[16];
static Word16 Prev_Min, Next_Min, Min;
static Word16 MeanE, MeanSE, MeanSLE, MeanSZC;
static Word16 prev_energy;
static Word16 count_sil, count_update, count_ext;
static Word16 flag, v_flag, less_count;
#endif

/*---------------------------------------------------------------------------*
 * Function  vad_init                                                        *
 * ~~~~~~~~~~~~~~~~~~                                                        *
 *                                                                           *
 * -> Initialization of variables for voice activity detection               *
 *                                                                           *
 *---------------------------------------------------------------------------*/
void vad_init(void)
{
  /* Static vectors to zero */
  Set_zero(pg729enc->pvad->MeanLSF, M);

  /* Initialize VAD parameters */
  pg729enc->pvad->MeanSE = 0;
  pg729enc->pvad->MeanSLE = 0;
  pg729enc->pvad->MeanE = 0;
  pg729enc->pvad->MeanSZC = 0;
  pg729enc->pvad->count_sil = 0;
  pg729enc->pvad->count_update = 0;
  pg729enc->pvad->count_ext = 0;
  pg729enc->pvad->less_count = 0;
  pg729enc->pvad->flag = 1;
  pg729enc->pvad->Min = MAX_16;
}


/*-----------------------------------------------------------------*
 * Functions vad                                                   *
 *           ~~~                                                   *
 * Input:                                                          *
 *   rc            : reflection coefficient                        *
 *   lsf[]         : unquantized lsf vector                        *
 *   r_h[]         : upper 16-bits of the autocorrelation vector   *
 *   r_l[]         : lower 16-bits of the autocorrelation vector   *
 *   exp_R0        : exponent of the autocorrelation vector        *
 *   sigpp[]       : preprocessed input signal                     *
 *   frm_count     : frame counter                                 *
 *   prev_marker   : VAD decision of the last frame                *
 *   pprev_marker  : VAD decision of the frame before last frame   *
 *                                                                 *
 * Output:                                                         *
 *                                                                 *
 *   marker        : VAD decision of the current frame             * 
 *                                                                 *
 *-----------------------------------------------------------------*/
void vad(
         Word16 rc,
         Word16 *lsf, 
         Word16 *r_h,
         Word16 *r_l, 
         Word16 exp_R0,
         Word16 *sigpp,
         Word16 frm_count,
         Word16 prev_marker,
         Word16 pprev_marker,
         Word16 *marker)
{
 /* scalar */
  Word32 acc0;
  Word16 i, j, exp, frac;
  Word16 ENERGY, ENERGY_low, SD, ZC, dSE, dSLE, dSZC;
  Word16 COEF, C_COEF, COEFZC, C_COEFZC, COEFSD, C_COEFSD;

	vad_type*			pvad = pg729enc->pvad;
	
  /* compute the frame energy */
  acc0 = L_Comp(r_h[0], r_l[0]);
  Log2(acc0, &exp, &frac);
  acc0 = Mpy_32_16(exp, frac, 9864);
  i = sub(exp_R0, 1);  
  i = sub(i, 1);
  acc0 = L_mac(acc0, 9864, i);
  acc0 = L_shl(acc0, 11);
  ENERGY = extract_h(acc0);
  ENERGY = sub(ENERGY, 4875);

  /* compute the low band energy */
  acc0 = 0;
  for (i=1; i<=NP; i++)
    acc0 = L_mac(acc0, r_h[i], lbf_corr[i]);
  acc0 = L_shl(acc0, 1);
  acc0 = L_mac(acc0, r_h[0], lbf_corr[0]);
  Log2(acc0, &exp, &frac);
  acc0 = Mpy_32_16(exp, frac, 9864);
  i = sub(exp_R0, 1);  
  i = sub(i, 1);
  acc0 = L_mac(acc0, 9864, i);
  acc0 = L_shl(acc0, 11);
  ENERGY_low = extract_h(acc0);
  ENERGY_low = sub(ENERGY_low, 4875);
  
  /* compute SD */
  acc0 = 0;
  for (i=0; i<M; i++){
    j = sub(lsf[i], pvad->MeanLSF[i]);
    acc0 = L_mac(acc0, j, j);
  }
  SD = extract_h(acc0);      /* Q15 */
  
  /* compute # zero crossing */
  ZC = 0;
  for (i=ZC_START+1; i<=ZC_END; i++)
    if (mult(sigpp[i-1], sigpp[i]) < 0)
      ZC = add(ZC, 410);     /* Q15 */

  /* Initialize and update Mins */
  if(sub(frm_count, 129) < 0){
    if (sub(ENERGY, pvad->Min) < 0){
      pvad->Min = ENERGY;
      pvad->Prev_Min = ENERGY;
    }
    
    if((frm_count & 0x0007) == 0){
      i = sub(shr(frm_count,3),1);
      pvad->Min_buffer[i] = pvad->Min;
      pvad->Min = MAX_16;
    }
  }

  if((frm_count & 0x0007) == 0){
    pvad->Prev_Min = pvad->Min_buffer[0];
    for (i=1; i<16; i++){
      if (sub(pvad->Min_buffer[i], pvad->Prev_Min) < 0)
        pvad->Prev_Min = pvad->Min_buffer[i];
    }
  }
  
  if(sub(frm_count, 129) >= 0){
    if(((frm_count & 0x0007) ^ (0x0001)) == 0){
      pvad->Min = pvad->Prev_Min;
      pvad->Next_Min = MAX_16;
    }
    if (sub(ENERGY, pvad->Min) < 0)
      pvad->Min = ENERGY;
    if (sub(ENERGY, pvad->Next_Min) < 0)
      pvad->Next_Min = ENERGY;
    
    if((frm_count & 0x0007) == 0){
      for (i=0; i<15; i++)
        pvad->Min_buffer[i] = pvad->Min_buffer[i+1]; 
      pvad->Min_buffer[15] = pvad->Next_Min; 
      pvad->Prev_Min = pvad->Min_buffer[0];
      for (i=1; i<16; i++) 
        if (sub(pvad->Min_buffer[i], pvad->Prev_Min) < 0)
          pvad->Prev_Min = pvad->Min_buffer[i];
    }
    
  }

  if (sub(frm_count, INIT_FRAME) <= 0)
    if(sub(ENERGY, 3072) < 0){
      *marker = NOISE;
      pvad->less_count++;
    }
    else{
      *marker = VOICE;
      acc0 = L_deposit_h(pvad->MeanE);
      acc0 = L_mac(acc0, ENERGY, 1024);
      pvad->MeanE = extract_h(acc0);
      acc0 = L_deposit_h(pvad->MeanSZC);
      acc0 = L_mac(acc0, ZC, 1024);
      pvad->MeanSZC = extract_h(acc0);
      for (i=0; i<M; i++){
        acc0 = L_deposit_h(pvad->MeanLSF[i]);
        acc0 = L_mac(acc0, lsf[i], 1024);
        pvad->MeanLSF[i] = extract_h(acc0);
      }
    }
  
  if (sub(frm_count, INIT_FRAME) >= 0){
    if (sub(frm_count, INIT_FRAME) == 0){
      acc0 = L_mult(pvad->MeanE, factor_fx[pvad->less_count]);
      acc0 = L_shl(acc0, shift_fx[pvad->less_count]);
      pvad->MeanE = extract_h(acc0);

      acc0 = L_mult(pvad->MeanSZC, factor_fx[pvad->less_count]);
      acc0 = L_shl(acc0, shift_fx[pvad->less_count]);
      pvad->MeanSZC = extract_h(acc0);

      for (i=0; i<M; i++){
        acc0 = L_mult(pvad->MeanLSF[i], factor_fx[pvad->less_count]);
        acc0 = L_shl(acc0, shift_fx[pvad->less_count]);
        pvad->MeanLSF[i] = extract_h(acc0);
      }

      pvad->MeanSE = sub(pvad->MeanE, 2048);   /* Q11 */
      pvad->MeanSLE = sub(pvad->MeanE, 2458);  /* Q11 */
    }

    dSE = sub(pvad->MeanSE, ENERGY);
    dSLE = sub(pvad->MeanSLE, ENERGY_low);
    dSZC = sub(pvad->MeanSZC, ZC);

    if(sub(ENERGY, 3072) < 0)
      *marker = NOISE;
    else 
      *marker = MakeDec(dSLE, dSE, SD, dSZC);

    pvad->v_flag = 0;
    if((prev_marker==VOICE) && (*marker==NOISE) && (add(dSE,410) < 0) 
       && (sub(ENERGY, 3072)>0)){
      *marker = VOICE;
      pvad->v_flag = 1;
    }

    if(pvad->flag == 1){
      if((pprev_marker == VOICE) && 
         (prev_marker == VOICE) && 
         (*marker == NOISE) && 
         (sub(abs_s(sub(pvad->prev_energy,ENERGY)), 614) <= 0)){
        pvad->count_ext++;
        *marker = VOICE;
        pvad->v_flag = 1;
        if(sub(pvad->count_ext, 4) <= 0)
          pvad->flag=1;
        else{
          pvad->count_ext=0;
          pvad->flag=0;
        }
      }
    }
    else
      pvad->flag=1;
    
    if(*marker == NOISE)
      pvad->count_sil++;

    if((*marker == VOICE) && (sub(pvad->count_sil, 10) > 0) && 
       (sub(sub(ENERGY, pvad->prev_energy), 614) <= 0)){
      *marker = NOISE;
      pvad->count_sil=0;
    }

    if(*marker == VOICE)
      pvad->count_sil=0;

    if ((sub(sub(ENERGY, 614), pvad->MeanSE) < 0) && (sub(frm_count, 128) > 0)
        && (!pvad->v_flag) && (sub(rc, 19661) < 0))
      *marker = NOISE;

    if ((sub(sub(ENERGY,614),pvad->MeanSE) < 0) && (sub(rc, 24576) < 0)
        && (sub(SD, 83) < 0)){ 
      pvad->count_update++;
      if (sub(pvad->count_update, INIT_COUNT) < 0){
        COEF = 24576;
        C_COEF = 8192;
        COEFZC = 26214;
        C_COEFZC = 6554;
        COEFSD = 19661;
        C_COEFSD = 13017;
      } 
      else
        if (sub(pvad->count_update, INIT_COUNT+10) < 0){
          COEF = 31130;
          C_COEF = 1638;
          COEFZC = 30147;
          C_COEFZC = 2621;
          COEFSD = 21299;
          C_COEFSD = 11469;
        }
        else
          if (sub(pvad->count_update, INIT_COUNT+20) < 0){
            COEF = 31785;
            C_COEF = 983;
            COEFZC = 30802;
            C_COEFZC = 1966;
            COEFSD = 22938;
            C_COEFSD = 9830;
          }
          else
            if (sub(pvad->count_update, INIT_COUNT+30) < 0){
              COEF = 32440;
              C_COEF = 328;
              COEFZC = 31457;
              C_COEFZC = 1311;
              COEFSD = 24576;
              C_COEFSD = 8192;
            }
            else
              if (sub(pvad->count_update, INIT_COUNT+40) < 0){
                COEF = 32604;
                C_COEF = 164;
                COEFZC = 32440;
                C_COEFZC = 328;
                COEFSD = 24576;
                C_COEFSD = 8192;
              }
              else{
                COEF = 32604;
                C_COEF = 164;
                COEFZC = 32702;
                C_COEFZC = 66;
                COEFSD = 24576;
                C_COEFSD = 8192;
              }
      

      /* compute MeanSE */
      acc0 = L_mult(COEF, pvad->MeanSE);
      acc0 = L_mac(acc0, C_COEF, ENERGY);
      pvad->MeanSE = extract_h(acc0);

      /* compute MeanSLE */
      acc0 = L_mult(COEF, pvad->MeanSLE);
      acc0 = L_mac(acc0, C_COEF, ENERGY_low);
      pvad->MeanSLE = extract_h(acc0);

      /* compute MeanSZC */
      acc0 = L_mult(COEFZC, pvad->MeanSZC);
      acc0 = L_mac(acc0, C_COEFZC, ZC);
      pvad->MeanSZC = extract_h(acc0);
      
      /* compute MeanLSF */
      for (i=0; i<M; i++){
        acc0 = L_mult(COEFSD, pvad->MeanLSF[i]);
        acc0 = L_mac(acc0, C_COEFSD, lsf[i]);
        pvad->MeanLSF[i] = extract_h(acc0);
      }
    }

    if((sub(frm_count, 128) > 0) && (((sub(pvad->MeanSE,pvad->Min) < 0) &&
                        (sub(SD, 83) < 0)) || (sub(pvad->MeanSE,pvad->Min) > 2048))){
      pvad->MeanSE = pvad->Min;
      pvad->count_update = 0;
    }
  }

  pvad->prev_energy = ENERGY;

}

/* local function */  
static Word16 MakeDec(
               Word16 dSLE,    /* (i)  : differential low band energy */
               Word16 dSE,     /* (i)  : differential full band energy */
               Word16 SD,      /* (i)  : differential spectral distortion */
               Word16 dSZC     /* (i)  : differential zero crossing rate */
               )
{
  Word32 acc0;
  
  /* SD vs dSZC */
  acc0 = L_mult(dSZC, -14680);          /* Q15*Q23*2 = Q39 */  
  acc0 = L_mac(acc0, 8192, -28521);     /* Q15*Q23*2 = Q39 */
  acc0 = L_shr(acc0, 8);                /* Q39 -> Q31 */
  acc0 = L_add(acc0, L_deposit_h(SD));
  if (acc0 > 0) return(VOICE);

  acc0 = L_mult(dSZC, 19065);           /* Q15*Q22*2 = Q38 */
  acc0 = L_mac(acc0, 8192, -19446);     /* Q15*Q22*2 = Q38 */
  acc0 = L_shr(acc0, 7);                /* Q38 -> Q31 */
  acc0 = L_add(acc0, L_deposit_h(SD));
  if (acc0 > 0) return(VOICE);

  /* dSE vs dSZC */
  acc0 = L_mult(dSZC, 20480);           /* Q15*Q13*2 = Q29 */
  acc0 = L_mac(acc0, 8192, 16384);      /* Q13*Q15*2 = Q29 */
  acc0 = L_shr(acc0, 2);                /* Q29 -> Q27 */
  acc0 = L_add(acc0, L_deposit_h(dSE));
  if (acc0 < 0) return(VOICE);

  acc0 = L_mult(dSZC, -16384);          /* Q15*Q13*2 = Q29 */
  acc0 = L_mac(acc0, 8192, 19660);      /* Q13*Q15*2 = Q29 */
  acc0 = L_shr(acc0, 2);                /* Q29 -> Q27 */
  acc0 = L_add(acc0, L_deposit_h(dSE));
  if (acc0 < 0) return(VOICE);
 
  acc0 = L_mult(dSE, 32767);            /* Q11*Q15*2 = Q27 */
  acc0 = L_mac(acc0, 1024, 30802);      /* Q10*Q16*2 = Q27 */
  if (acc0 < 0) return(VOICE);
  
  /* dSE vs SD */
  acc0 = L_mult(SD, -28160);            /* Q15*Q5*2 = Q22 */
  acc0 = L_mac(acc0, 64, 19988);        /* Q6*Q14*2 = Q22 */
  acc0 = L_mac(acc0, dSE, 512);         /* Q11*Q9*2 = Q22 */
  if (acc0 < 0) return(VOICE);

  acc0 = L_mult(SD, 32767);             /* Q15*Q15*2 = Q31 */
  acc0 = L_mac(acc0, 32, -30199);       /* Q5*Q25*2 = Q31 */
  if (acc0 > 0) return(VOICE);

  /* dSLE vs dSZC */
  acc0 = L_mult(dSZC, -20480);          /* Q15*Q13*2 = Q29 */
  acc0 = L_mac(acc0, 8192, 22938);      /* Q13*Q15*2 = Q29 */
  acc0 = L_shr(acc0, 2);                /* Q29 -> Q27 */
  acc0 = L_add(acc0, L_deposit_h(dSE));
  if (acc0 < 0) return(VOICE);

  acc0 = L_mult(dSZC, 23831);           /* Q15*Q13*2 = Q29 */
  acc0 = L_mac(acc0, 4096, 31576);      /* Q12*Q16*2 = Q29 */
  acc0 = L_shr(acc0, 2);                /* Q29 -> Q27 */
  acc0 = L_add(acc0, L_deposit_h(dSE));
  if (acc0 < 0) return(VOICE);
  
  acc0 = L_mult(dSE, 32767);            /* Q11*Q15*2 = Q27 */
  acc0 = L_mac(acc0, 2048, 17367);      /* Q11*Q15*2 = Q27 */
  if (acc0 < 0) return(VOICE);
  
  /* dSLE vs SD */
  acc0 = L_mult(SD, -22400);            /* Q15*Q4*2 = Q20 */
  acc0 = L_mac(acc0, 32, 25395);        /* Q5*Q14*2 = Q20 */
  acc0 = L_mac(acc0, dSLE, 256);        /* Q11*Q8*2 = Q20 */
  if (acc0 < 0) return(VOICE);
  
  /* dSLE vs dSE */
  acc0 = L_mult(dSE, -30427);           /* Q11*Q15*2 = Q27 */
  acc0 = L_mac(acc0, 256, -29959);      /* Q8*Q18*2 = Q27 */
  acc0 = L_add(acc0, L_deposit_h(dSLE));
  if (acc0 > 0) return(VOICE);

  acc0 = L_mult(dSE, -23406);           /* Q11*Q15*2 = Q27 */
  acc0 = L_mac(acc0, 512, 28087);       /* Q19*Q17*2 = Q27 */
  acc0 = L_add(acc0, L_deposit_h(dSLE));
  if (acc0 < 0) return(VOICE);

  acc0 = L_mult(dSE, 24576);            /* Q11*Q14*2 = Q26 */
  acc0 = L_mac(acc0, 1024, 29491);      /* Q10*Q15*2 = Q26 */
  acc0 = L_mac(acc0, dSLE, 16384);      /* Q11*Q14*2 = Q26 */
  if (acc0 < 0) return(VOICE);

  return (NOISE);
}




