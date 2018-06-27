/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
   ITU-T G.729A Speech Coder with Annex B    ANSI-C Source Code
   Version 1.5    Last modified: October 2006 

   Copyright (c) 1996,
   AT&T, France Telecom, NTT, Universite de Sherbrooke, Lucent Technologies,
   Rockwell International
   All rights reserved.
*/

/*-----------------------------------------------------------------*
 *   Functions Init_Decod_ld8a  and Decod_ld8a                     *
 *-----------------------------------------------------------------*/

//#include <stdio.h>
//#include <stdlib.h>

#include "typedef.h"
#include "basic_op.h"
#include "ld8a.h"

#include "dtx.h"
#include "sid.h"

#include "dec_struct.h"
/*---------------------------------------------------------------*
 *   Decoder constant parameters (defined in "ld8a.h")           *
 *---------------------------------------------------------------*
 *   L_FRAME     : Frame size.                                   *
 *   L_SUBFR     : Sub-frame size.                               *
 *   M           : LPC order.                                    *
 *   MP1         : LPC order+1                                   *
 *   PIT_MIN     : Minimum pitch lag.                            *
 *   PIT_MAX     : Maximum pitch lag.                            *
 *   L_INTERPOL  : Length of filter for interpolation            *
 *   PRM_SIZE    : Size of vector containing analysis parameters *
 *---------------------------------------------------------------*/

/*--------------------------------------------------------*
 *         Static memory allocation.                      *
 *--------------------------------------------------------*/

        /* Excitation vector */
#if 0
static Word16 old_exc[L_FRAME+PIT_MAX+L_INTERPOL];
static Word16 *exc;

    /* Lsp (Line spectral pairs) */

static Word16 lsp_old[M]={
         30000, 26000, 21000, 15000, 8000, 0, -8000,-15000,-21000,-26000};

    /* Filter's memory */

static Word16 mem_syn[M];

static Word16 sharp;           /* pitch sharpening of previous frame */
static Word16 old_T0;          /* integer delay of previous frame    */
static Word16 gain_code;       /* Code gain                          */
static Word16 gain_pitch;      /* Pitch gain                         */




/* for G.729B */
static Word16 seed_fer;
/* CNG variables */
static Word16 past_ftyp;
static Word16 seed;
static Word16 sid_sav, sh_sid_sav;
#endif


/*-----------------------------------------------------------------*
 *   Function Init_Decod_ld8a                                      *
 *            ~~~~~~~~~~~~~~~                                      *
 *                                                                 *
 *   ->Initialization of variables for the decoder section.        *
 *                                                                 *
 *-----------------------------------------------------------------*/

void Init_Decod_ld8a(void)
{

  /* Initialize static pointer */

  pg729dec->pdec_ld8a->exc = pg729dec->pdec_ld8a->old_exc + PIT_MAX + L_INTERPOL;

  /* Static vectors to zero */

  Set_zero(pg729dec->pdec_ld8a->old_exc, PIT_MAX+L_INTERPOL);
  Set_zero(pg729dec->pdec_ld8a->mem_syn, M);

  pg729dec->pdec_ld8a->sharp  = SHARPMIN;
  pg729dec->pdec_ld8a->old_T0 = 60;
  pg729dec->pdec_ld8a->gain_code = 0;
  pg729dec->pdec_ld8a->gain_pitch = 0;

  Lsp_decw_reset();

  /* for G.729B */
  pg729dec->pdec_ld8a->seed_fer = 21845;
  pg729dec->pdec_ld8a->past_ftyp = 1;
  pg729dec->pdec_ld8a->seed = INIT_SEED;
  pg729dec->pdec_ld8a->sid_sav = 0;
  pg729dec->pdec_ld8a->sh_sid_sav = 1;
  Init_lsfq_noise_dec();

  return;
}

#if 0
Word16 bad_lsf; /* bad LSF indicator   */
#endif

/*-----------------------------------------------------------------*
 *   Function Decod_ld8a                                           *
 *           ~~~~~~~~~~                                            *
 *   ->Main decoder routine.                                       *
 *                                                                 *
 *-----------------------------------------------------------------*/

void Decod_ld8a(
  Word16  parm[],      /* (i)   : vector of synthesis parameters
                                  parm[0] = bad frame indicator (bfi)  */
  Word16  synth[],     /* (o)   : synthesis speech                     */
  Word16  A_t[],       /* (o)   : decoded LP filter in 2 subframes     */
  Word16  *T2,         /* (o)   : decoded pitch lag in 2 subframes     */
  Word16  *Vad         /* (o)   : frame type                           */
)
{
  Word16  *Az;                  /* Pointer on A_t   */
  Word16  lsp_new[M];           /* LSPs             */
  Word16  code[L_SUBFR];        /* ACELP codevector */

  /* Scalars */

  Word16  i, j, i_subfr;
  Word16  T0, T0_frac, index;
  Word16  bfi;
  Word32  L_temp;

  Word16 bad_pitch;             /* bad pitch indicator */
  //extern Word16 bad_lsf;        /* bad LSF indicator   */

  /* for G.729B */
  Word16 ftyp;
  Word16 lsfq_mem[MA_NP][M];

	dec_ld8a_type* pdec_ld8a = pg729dec->pdec_ld8a;
	
  /* Test bad frame indicator (bfi) */

  bfi = *parm++;
  /* for G.729B */
  ftyp = *parm;

  if(bfi == 1) {
    if(pdec_ld8a->past_ftyp == 1) {
      ftyp = 1;
      parm[4] = 1;    /* G.729 maintenance */
    }
    else ftyp = 0;
    *parm = ftyp;  /* modification introduced in version V1.3 */
  }
  
  *Vad = ftyp;

  /* Processing non active frames (SID & not transmitted) */
  if(ftyp != 1) {
    
    Get_decfreq_prev(lsfq_mem);
    Dec_cng(pdec_ld8a->past_ftyp, pdec_ld8a->sid_sav, pdec_ld8a->sh_sid_sav, parm, 
    		pdec_ld8a->exc, pdec_ld8a->lsp_old, A_t, &pdec_ld8a->seed, lsfq_mem);
    Update_decfreq_prev(lsfq_mem);

    Az = A_t;
    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR) {
      Overflow = 0;
      Syn_filt(Az, &pdec_ld8a->exc[i_subfr], &synth[i_subfr], L_SUBFR, pdec_ld8a->mem_syn, 0);
      if(Overflow != 0) {
        /* In case of overflow in the synthesis          */
        /* -> Scale down vector exc[] and redo synthesis */
        
        for(i=0; i<PIT_MAX+L_INTERPOL+L_FRAME; i++)
          pdec_ld8a->old_exc[i] = shr(pdec_ld8a->old_exc[i], 2);
        
        Syn_filt(Az, &pdec_ld8a->exc[i_subfr], &synth[i_subfr], L_SUBFR, pdec_ld8a->mem_syn, 1);
      }
      else
        Copy(&synth[i_subfr+L_SUBFR-M], pdec_ld8a->mem_syn, M);
      
      Az += MP1;

      *T2++ = pdec_ld8a->old_T0;
    }
    pdec_ld8a->sharp = SHARPMIN;
    
  }
  /* Processing active frame */
  else {
    
    pdec_ld8a->seed = INIT_SEED;
    parm++;

    /* Decode the LSPs */
    
    D_lsp(parm, lsp_new, add(bfi, pdec_ld8a->bad_lsf));
    parm += 2;
    
    /*
       Note: "bad_lsf" is introduce in case the standard is used with
       channel protection.
       */
    
    /* Interpolation of LPC for the 2 subframes */
    
    Int_qlpc(pdec_ld8a->lsp_old, lsp_new, A_t);
    
    /* update the LSFs for the next frame */
    
    Copy(lsp_new, pdec_ld8a->lsp_old, M);
    
    /*------------------------------------------------------------------------*
     *          Loop for every subframe in the analysis frame                 *
     *------------------------------------------------------------------------*
     * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR  *
     *  times                                                                 *
     *     - decode the pitch delay                                           *
     *     - decode algebraic code                                            *
     *     - decode pitch and codebook gains                                  *
     *     - find the excitation and compute synthesis speech                 *
     *------------------------------------------------------------------------*/
    
    Az = A_t;            /* pointer to interpolated LPC parameters */
    
    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
      {

        index = *parm++;        /* pitch index */

        if(i_subfr == 0)
          {
            i = *parm++;        /* get parity check result */
            bad_pitch = add(bfi, i);
            if( bad_pitch == 0)
              {
                Dec_lag3(index, PIT_MIN, PIT_MAX, i_subfr, &T0, &T0_frac);
                pdec_ld8a->old_T0 = T0;
              }
            else                /* Bad frame, or parity error */
              {
                T0  =  pdec_ld8a->old_T0;
                T0_frac = 0;
                pdec_ld8a->old_T0 = add( pdec_ld8a->old_T0, 1);
                if( sub(pdec_ld8a->old_T0, PIT_MAX) > 0) {
                  pdec_ld8a->old_T0 = PIT_MAX;
                }
              }
          }
        else                    /* second subframe */
          {
            if( bfi == 0)
              {
                Dec_lag3(index, PIT_MIN, PIT_MAX, i_subfr, &T0, &T0_frac);
                pdec_ld8a->old_T0 = T0;
              }
            else
              {
                T0  =  pdec_ld8a->old_T0;
                T0_frac = 0;
                pdec_ld8a->old_T0 = add( pdec_ld8a->old_T0, 1);
                if( sub(pdec_ld8a->old_T0, PIT_MAX) > 0) {
                  pdec_ld8a->old_T0 = PIT_MAX;
                }
              }
          }
        *T2++ = T0;

        /*-------------------------------------------------*
         * - Find the adaptive codebook vector.            *
         *-------------------------------------------------*/

        Pred_lt_3(&pdec_ld8a->exc[i_subfr], T0, T0_frac, L_SUBFR);

        /*-------------------------------------------------------*
         * - Decode innovative codebook.                         *
         * - Add the fixed-gain pitch contribution to code[].    *
         *-------------------------------------------------------*/

        if(bfi != 0)            /* Bad frame */
          {

            parm[0] = Random(&pdec_ld8a->seed_fer) & (Word16)0x1fff; /* 13 bits random */
            parm[1] = Random(&pdec_ld8a->seed_fer) & (Word16)0x000f; /*  4 bits random */
          }

        Decod_ACELP(parm[1], parm[0], code);
        parm +=2;

        j = shl(pdec_ld8a->sharp, 1);      /* From Q14 to Q15 */
        if(sub(T0, L_SUBFR) <0 ) {
          for (i = T0; i < L_SUBFR; i++) {
            code[i] = add(code[i], mult(code[i-T0], j));
          }
        }

        /*-------------------------------------------------*
         * - Decode pitch and codebook gains.              *
         *-------------------------------------------------*/

        index = *parm++;        /* index of energy VQ */

        Dec_gain(index, code, L_SUBFR, bfi, &pdec_ld8a->gain_pitch, &pdec_ld8a->gain_code);

        /*-------------------------------------------------------------*
         * - Update pitch sharpening "sharp" with quantized gain_pitch *
         *-------------------------------------------------------------*/

        pdec_ld8a->sharp = pdec_ld8a->gain_pitch;
        if (sub(pdec_ld8a->sharp, SHARPMAX) > 0) { pdec_ld8a->sharp = SHARPMAX;  }
        if (sub(pdec_ld8a->sharp, SHARPMIN) < 0) { pdec_ld8a->sharp = SHARPMIN;  }

        /*-------------------------------------------------------*
         * - Find the total excitation.                          *
         * - Find synthesis speech corresponding to exc[].       *
         *-------------------------------------------------------*/

        for (i = 0; i < L_SUBFR;  i++)
          {
            /* exc[i] = gain_pitch*exc[i] + gain_code*code[i]; */
            /* exc[i]  in Q0   gain_pitch in Q14               */
            /* code[i] in Q13  gain_codeode in Q1              */
            
            L_temp = L_mult(pdec_ld8a->exc[i+i_subfr], pdec_ld8a->gain_pitch);
            L_temp = L_mac(L_temp, code[i], pdec_ld8a->gain_code);
            L_temp = L_shl(L_temp, 1);
            pdec_ld8a->exc[i+i_subfr] = round(L_temp);
          }
        
        Overflow = 0;
        Syn_filt(Az, &pdec_ld8a->exc[i_subfr], &synth[i_subfr], L_SUBFR, pdec_ld8a->mem_syn, 0);
        if(Overflow != 0)
          {
            /* In case of overflow in the synthesis          */
            /* -> Scale down vector exc[] and redo synthesis */

            for(i=0; i<PIT_MAX+L_INTERPOL+L_FRAME; i++)
              pdec_ld8a->old_exc[i] = shr(pdec_ld8a->old_exc[i], 2);

            Syn_filt(Az, &pdec_ld8a->exc[i_subfr], &synth[i_subfr], L_SUBFR, pdec_ld8a->mem_syn, 1);
          }
        else
          Copy(&synth[i_subfr+L_SUBFR-M], pdec_ld8a->mem_syn, M);

        Az += MP1;              /* interpolated LPC parameters for next subframe */
      }
  }
  
  /*------------*
   *  For G729b
   *-----------*/
  if(bfi == 0) {
    L_temp = 0L;
    for(i=0; i<L_FRAME; i++) {
      L_temp = L_mac(L_temp, pdec_ld8a->exc[i], pdec_ld8a->exc[i]);
    } /* may overflow => last level of SID quantizer */
    pdec_ld8a->sh_sid_sav = norm_l(L_temp);
    if(pdec_ld8a->sh_sid_sav>0)
    	pdec_ld8a->sid_sav = round(L_shl(L_temp, pdec_ld8a->sh_sid_sav));
    else
    	pdec_ld8a->sid_sav = round(L_shr(L_temp, abs_s(pdec_ld8a->sh_sid_sav)));	
    pdec_ld8a->sh_sid_sav = sub(16, pdec_ld8a->sh_sid_sav);
  }

 /*--------------------------------------------------*
  * Update signal for next frame.                    *
  * -> shift to the left by L_FRAME  exc[]           *
  *--------------------------------------------------*/

  Copy(&pdec_ld8a->old_exc[L_FRAME], &pdec_ld8a->old_exc[0], PIT_MAX+L_INTERPOL);

  /* for G729b */
  pdec_ld8a->past_ftyp = ftyp;

  return;
}






