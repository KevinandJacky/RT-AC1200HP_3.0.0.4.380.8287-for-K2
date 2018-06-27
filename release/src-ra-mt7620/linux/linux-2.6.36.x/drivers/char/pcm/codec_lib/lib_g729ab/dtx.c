/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
   ITU-T G.729A Annex B     ANSI-C Source Code
   Version 1.3    Last modified: August 1997
   Copyright (c) 1996, France Telecom, Rockwell International,
                       Universite de Sherbrooke.
   All rights reserved.
*/

/* DTX and Comfort Noise Generator - Encoder part */

//#include <stdio.h>
//#include <stdlib.h>

#include "typedef.h"
#include "basic_op.h"
#include "ld8a.h"
#include "oper_32b.h"
#include "tab_ld8a.h"
#include "vad.h"
#include "dtx.h"
#include "tab_dtx.h"
#include "sid.h"

#include "cod_struct.h"

#if 0
/* Static Variables */
static Word16 lspSid_q[M] ;
static Word16 pastCoeff[MP1];
static Word16 RCoeff[MP1];
static Word16 sh_RCoeff;
static Word16 Acf[SIZ_ACF];
static Word16 sh_Acf[NB_CURACF];
static Word16 sumAcf[SIZ_SUMACF];
static Word16 sh_sumAcf[NB_SUMACF];
static Word16 ener[NB_GAIN];
static Word16 sh_ener[NB_GAIN];
static Word16 fr_cur;
static Word16 cur_gain;
static Word16 nb_ener;
static Word16 sid_gain;
static Word16 flag_chang;
static Word16 prev_energy;
static Word16 count_fr0;
#endif

/* Local functions */
static void Calc_pastfilt(Word16 *Coeff);
static void Calc_RCoeff(Word16 *Coeff, Word16 *RCoeff, Word16 *sh_RCoeff);
static Word16 Cmp_filt(Word16 *RCoeff, Word16 sh_RCoeff, Word16 *acf,
                                        Word16 alpha, Word16 Fracthresh);
static void Calc_sum_acf(Word16 *acf, Word16 *sh_acf,
                    Word16 *sum, Word16 *sh_sum, Word16 nb);
static void Update_sumAcf(void);

/*-----------------------------------------------------------*
 * procedure Init_Cod_cng:                                   *
 *           ~~~~~~~~~~~~                                    *
 *   Initialize variables used for dtx at the encoder        *
 *-----------------------------------------------------------*/
void Init_Cod_cng(void)
{
  Word16 i;

	dtx_type*			pdtx = pg729enc->pdtx;
	
  for(i=0; i<SIZ_SUMACF; i++) pdtx->sumAcf[i] = 0;
  for(i=0; i<NB_SUMACF; i++) pdtx->sh_sumAcf[i] = 40;

  for(i=0; i<SIZ_ACF; i++) pdtx->Acf[i] = 0;
  for(i=0; i<NB_CURACF; i++) pdtx->sh_Acf[i] = 40;

  for(i=0; i<NB_GAIN; i++) pdtx->sh_ener[i] = 40;
  for(i=0; i<NB_GAIN; i++) pdtx->ener[i] = 0;

  pdtx->cur_gain = 0;
  pdtx->fr_cur = 0;
  pdtx->flag_chang = 0;

  return;
}


/*-----------------------------------------------------------*
 * procedure Cod_cng:                                        *
 *           ~~~~~~~~                                        *
 *   computes DTX decision                                   *
 *   encodes SID frames                                      *
 *   computes CNG excitation for encoder update              *
 *-----------------------------------------------------------*/
void Cod_cng(
  Word16 *exc,          /* (i/o) : excitation array                     */
  Word16 pastVad,       /* (i)   : previous VAD decision                */
  Word16 *lsp_old_q,    /* (i/o) : previous quantized lsp               */
  Word16 *Aq,           /* (o)   : set of interpolated LPC coefficients */
  Word16 *ana,          /* (o)   : coded SID parameters                 */
  Word16 freq_prev[MA_NP][M],
                        /* (i/o) : previous LPS for quantization        */
  Word16 *seed          /* (i/o) : random generator seed                */
)
{

  Word16 i;

  Word16 curAcf[MP1];
  Word16 bid[M], zero[MP1];
  Word16 curCoeff[MP1];
  Word16 lsp_new[M];
  Word16 *lpcCoeff;
  Word16 cur_igain;
  Word16 energyq, temp;

	dtx_type*			pdtx = pg729enc->pdtx;
	
  /* Update Ener and sh_ener */
  for(i = NB_GAIN-1; i>=1; i--) {
    pdtx->ener[i] = pdtx->ener[i-1];
    pdtx->sh_ener[i] = pdtx->sh_ener[i-1];
  }

  /* Compute current Acfs */
  Calc_sum_acf(pdtx->Acf, pdtx->sh_Acf, curAcf, &pdtx->sh_ener[0], NB_CURACF);

  /* Compute LPC coefficients and residual energy */
  if(curAcf[0] == 0) {
    pdtx->ener[0] = 0;                /* should not happen */
  }
  else {
    Set_zero(zero, MP1);
    Levinson(curAcf, zero, curCoeff, bid, &pdtx->ener[0]);
  }

  /* if first frame of silence => SID frame */
  if(pastVad != 0) {
    ana[0] = 2;
    pdtx->count_fr0 = 0;
    pdtx->nb_ener = 1;
    Qua_Sidgain(pdtx->ener, pdtx->sh_ener, pdtx->nb_ener, &energyq, &cur_igain);

  }
  else {
    pdtx->nb_ener = add(pdtx->nb_ener, 1);
    if(sub(pdtx->nb_ener, NB_GAIN) > 0) pdtx->nb_ener = NB_GAIN;
    Qua_Sidgain(pdtx->ener, pdtx->sh_ener, pdtx->nb_ener, &energyq, &cur_igain);
      
    /* Compute stationarity of current filter   */
    /* versus reference filter                  */
    if(Cmp_filt(pdtx->RCoeff, pdtx->sh_RCoeff, curAcf, pdtx->ener[0], FRAC_THRESH1) != 0) {
      pdtx->flag_chang = 1;
    }
      
    /* compare energy difference between current frame and last frame */
    temp = abs_s(sub(pdtx->prev_energy, energyq));
    temp = sub(temp, 2);
    if (temp > 0) pdtx->flag_chang = 1;
      
    pdtx->count_fr0 = add(pdtx->count_fr0, 1);
    if(sub(pdtx->count_fr0, FR_SID_MIN) < 0) {
      ana[0] = 0;               /* no transmission */
    }
    else {
      if(pdtx->flag_chang != 0) {
        ana[0] = 2;             /* transmit SID frame */
      }
      else{
        ana[0] = 0;
      }
        
      pdtx->count_fr0 = FR_SID_MIN;   /* to avoid overflow */
    }
  }


  if(sub(ana[0], 2) == 0) {
      
    /* Reset frame count and change flag */
    pdtx->count_fr0 = 0;
    pdtx->flag_chang = 0;
      
    /* Compute past average filter */
    Calc_pastfilt(pdtx->pastCoeff);
    Calc_RCoeff(pdtx->pastCoeff, pdtx->RCoeff, &pdtx->sh_RCoeff);

    /* Compute stationarity of current filter   */
    /* versus past average filter               */


    /* if stationary */
    /* transmit average filter => new ref. filter */
    if(Cmp_filt(pdtx->RCoeff, pdtx->sh_RCoeff, curAcf, pdtx->ener[0], FRAC_THRESH2) == 0) {
      lpcCoeff = pdtx->pastCoeff;
    }

    /* else */
    /* transmit current filter => new ref. filter */
    else {
      lpcCoeff = curCoeff;
      Calc_RCoeff(curCoeff, pdtx->RCoeff, &pdtx->sh_RCoeff);
    }

    /* Compute SID frame codes */

    Az_lsp(lpcCoeff, lsp_new, lsp_old_q); /* From A(z) to lsp */

    /* LSP quantization */
    lsfq_noise(lsp_new, pdtx->lspSid_q, freq_prev, &ana[1]);

    pdtx->prev_energy = energyq;
    ana[4] = cur_igain;
    pdtx->sid_gain = tab_Sidgain[cur_igain];


  } /* end of SID frame case */

  /* Compute new excitation */
  if(pastVad != 0) {
    pdtx->cur_gain = pdtx->sid_gain;
  }
  else {
    pdtx->cur_gain = mult_r(pdtx->cur_gain, A_GAIN0);
    pdtx->cur_gain = add(pdtx->cur_gain, mult_r(pdtx->sid_gain, A_GAIN1));
  }

  Calc_exc_rand(pdtx->cur_gain, exc, seed, FLAG_COD);

  Int_qlpc(lsp_old_q, pdtx->lspSid_q, Aq);
  for(i=0; i<M; i++) {
    lsp_old_q[i]   = pdtx->lspSid_q[i];
  }

  /* Update sumAcf if fr_cur = 0 */
  if(pdtx->fr_cur == 0) {
    Update_sumAcf();
  }

  return;
}

/*-----------------------------------------------------------*
 * procedure Update_cng:                                     *
 *           ~~~~~~~~~~                                      *
 *   Updates autocorrelation arrays                          *
 *   used for DTX/CNG                                        *
 *   If Vad=1 : updating of array sumAcf                     *
 *-----------------------------------------------------------*/
void Update_cng(
  Word16 *r_h,      /* (i) :   MSB of frame autocorrelation        */
  Word16 exp_r,     /* (i) :   scaling factor associated           */
  Word16 Vad        /* (i) :   current Vad decision                */
)
{
  Word16 i;
  Word16 *ptr1, *ptr2;

	dtx_type*			pdtx = pg729enc->pdtx;
  /* Update Acf and shAcf */
  ptr1 = pdtx->Acf + SIZ_ACF - 1;
  ptr2 = ptr1 - MP1;
  for(i=0; i<(SIZ_ACF-MP1); i++) {
    *ptr1-- = *ptr2--;
  }
  for(i=NB_CURACF-1; i>=1; i--) {
    pdtx->sh_Acf[i] = pdtx->sh_Acf[i-1];
  }

  /* Save current Acf */
  pdtx->sh_Acf[0] = negate(add(16, exp_r));
  for(i=0; i<MP1; i++) {
    pdtx->Acf[i] = r_h[i];
  }

  pdtx->fr_cur = add(pdtx->fr_cur, 1);
  if(sub(pdtx->fr_cur, NB_CURACF) == 0) {
    pdtx->fr_cur = 0;
    if(Vad != 0) {
      Update_sumAcf();
    }
  }

  return;
}


/*-----------------------------------------------------------*
 *         Local procedures                                  *
 *         ~~~~~~~~~~~~~~~~                                  *
 *-----------------------------------------------------------*/

/* Compute scaled autocorr of LPC coefficients used for Itakura distance */
/*************************************************************************/
static void Calc_RCoeff(Word16 *Coeff, Word16 *RCoeff, Word16 *sh_RCoeff)
{
  Word16 i, j;
  Word16 sh1;
  Word32 L_acc;
   
  /* RCoeff[0] = SUM(j=0->M) Coeff[j] ** 2 */
  L_acc = 0L;
  for(j=0; j <= M; j++) {
    L_acc = L_mac(L_acc, Coeff[j], Coeff[j]);
  }
  
  /* Compute exponent RCoeff */
  sh1 = norm_l(L_acc);
	if(sh1 >= 0)
  L_acc = L_shl(L_acc, sh1);
	else
		L_acc = L_shr(L_acc, abs_s(sh1));
	
  RCoeff[0] = round(L_acc);
  
  /* RCoeff[i] = SUM(j=0->M-i) Coeff[j] * Coeff[j+i] */
	{
		Word16 psh1 = abs_s(sh1);
		for( i = 1 ; i <= M ; i ++ ) 
		{
    L_acc = 0L;
    for(j=0; j<=M-i; j++) {
      L_acc = L_mac(L_acc, Coeff[j], Coeff[j+i]);
    }
			if(sh1 >= 0)
    L_acc = L_shl(L_acc, sh1);
			else
				L_acc = L_shr(L_acc, psh1);	
    RCoeff[i] = round(L_acc);
  }
	}
  *sh_RCoeff = sh1;
  return;
}

/* Compute Itakura distance and compare to threshold */
/*****************************************************/
static Word16 Cmp_filt(Word16 *RCoeff, Word16 sh_RCoeff, Word16 *acf,
                                        Word16 alpha, Word16 FracThresh)
{
  Word32 L_temp0, L_temp1;
  Word16 temp1, temp2, sh[2], ind;
  Word16 i;
  Word16 diff, flag;
  extern Flag Overflow;

  sh[0] = 0;
  sh[1] = 0;
  ind = 1;
  flag = 0;
  do {
    Overflow = 0;
    temp1 = shr(RCoeff[0], sh[0]);
    temp2 = shr(acf[0], sh[1]);
    L_temp0 = L_shr(L_mult(temp1, temp2),1);
    for(i=1; i <= M; i++) {
      temp1 = shr(RCoeff[i], sh[0]);
      temp2 = shr(acf[i], sh[1]);
      L_temp0 = L_mac(L_temp0, temp1, temp2);
    }
    if(Overflow != 0) {
      sh[(int)ind] = add(sh[(int)ind], 1);
      ind = sub(1, ind);
    }
    else flag = 1;
  } while (flag == 0);
  
  
  temp1 = mult_r(alpha, FracThresh);
  L_temp1 = L_add(L_deposit_l(temp1), L_deposit_l(alpha));
  temp1 = add(sh_RCoeff, 9);  /* 9 = Lpc_justif. * 2 - 16 + 1 */
  temp2 = add(sh[0], sh[1]);
  temp1 = sub(temp1, temp2);
  if(temp1>=0)
  L_temp1 = L_shl(L_temp1, temp1);
  else
  	L_temp1 = L_shr(L_temp1, abs_s(temp1));
  
  L_temp0 = L_sub(L_temp0, L_temp1);
  if(L_temp0 > 0L) diff = 1;
  else diff = 0;

  return(diff);
}

/* Compute past average filter */
/*******************************/
static void Calc_pastfilt(Word16 *Coeff)
{
  Word16 i;
  Word16 s_sumAcf[MP1];
  Word16 bid[M], zero[MP1];
  Word16 temp;
  
  dtx_type*			pdtx = pg729enc->pdtx;
  
  Calc_sum_acf(pdtx->sumAcf, pdtx->sh_sumAcf, s_sumAcf, &temp, NB_SUMACF);
  
  if(s_sumAcf[0] == 0L) {
    Coeff[0] = 4096;
    for(i=1; i<=M; i++) Coeff[i] = 0;
    return;
  }

  Set_zero(zero, MP1);
  Levinson(s_sumAcf, zero, Coeff, bid, &temp);
  return;
}

/* Update sumAcf */
/*****************/
static void Update_sumAcf(void)
{
  Word16 *ptr1, *ptr2;
  Word16 i;

	dtx_type*			pdtx = pg729enc->pdtx;
	
  /*** Move sumAcf ***/
  ptr1 = pdtx->sumAcf + SIZ_SUMACF - 1;
  ptr2 = ptr1 - MP1;
  for(i=0; i<(SIZ_SUMACF-MP1); i++) {
    *ptr1-- = *ptr2--;
  }
  for(i=NB_SUMACF-1; i>=1; i--) {
    pdtx->sh_sumAcf[i] = pdtx->sh_sumAcf[i-1];
  }

  /* Compute new sumAcf */
  Calc_sum_acf(pdtx->Acf, pdtx->sh_Acf, pdtx->sumAcf, pdtx->sh_sumAcf, NB_CURACF);
  return;
}

/* Compute sum of acfs (curAcf, sumAcf or s_sumAcf) */
/****************************************************/
static void Calc_sum_acf(Word16 *acf, Word16 *sh_acf,
                         Word16 *sum, Word16 *sh_sum, Word16 nb)
{

  Word16 *ptr1;
  Word32 L_temp, L_tab[MP1];
  Word16 sh0, temp;
  Word16 i, j;
  
  /* Compute sum = sum of nb acfs */
  /* Find sh_acf minimum */
  sh0 = sh_acf[0];
  for(i=1; i<nb; i++) {
    if(sub(sh_acf[i], sh0) < 0) sh0 = sh_acf[i];
  }
  sh0 = add(sh0, 14);           /* 2 bits of margin */

  for(j=0; j<MP1; j++) {
    L_tab[j] = 0L;
  }
  ptr1 = acf;
  for(i=0; i<nb; i++) {
    temp = sub(sh0, sh_acf[i]);
    if(temp>=0)
    {
    for(j=0; j<MP1; j++) {
      L_temp = L_deposit_l(*ptr1++);
      L_temp = L_shl(L_temp, temp); /* shift right if temp<0 */
      L_tab[j] = L_add(L_tab[j], L_temp);
    }
  } 
	else
	{
		Word16 ptemp = abs_s(temp);
		for(j=0; j<MP1; j++) {
      		L_temp = L_deposit_l(*ptr1++);
      		L_temp = L_shr(L_temp, ptemp); /* shift right if temp<0 */
      		L_tab[j] = L_add(L_tab[j], L_temp);
    	}	
	}

  } 
  temp = norm_l(L_tab[0]);
  if(temp>=0)
  for(i=0; i<=M; i++) {
    sum[i] = extract_h(L_shl(L_tab[i], temp));
  }
  else
  {
  		Word16 ptemp = abs_s(temp);
  		for(i=0; i<=M; i++) {
	    	sum[i] = extract_h(L_shr(L_tab[i], ptemp));
	  	}
  		
  }
  temp = sub(temp, 16);
  *sh_sum = add(sh0, temp);
  return;
}
