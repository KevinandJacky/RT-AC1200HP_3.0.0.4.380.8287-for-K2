#ifndef __COD_STRUCT_H_
#define __COD_STRUCT_H_

#include "typedef.h"
#include "ld8a.h"
#include "dtx.h"

typedef struct {
	Word16	old_speech[L_TOTAL];
	Word16	*speech;
	Word16	*new_speech;						
	Word16	*p_window;						
	Word16 	old_wsp[L_FRAME+PIT_MAX+1];		
	Word16	*wsp;							
	Word16 	old_exc[L_FRAME+PIT_MAX+L_INTERPOL];
	Word16 	*exc;							
	Word16 	lsp_old[M];						
	Word16 	lsp_old_q[M];					
	Word16  mem_w0[M], mem_w[M], mem_zero[M];
	Word16  sharp;							
	Word16 	pastVad;						
	Word16 	ppastVad;						
	Word16 	seed;							
} cod_ld8a_type;

typedef struct
{
	Word16 old_A[M+1];	
	Word16 old_rc[2];						
} lpc_type;

typedef struct
{
	Word16 y2_hi;
	Word16 y2_lo;
	Word16 y1_hi;
	Word16 y1_lo;
	Word16 x0;
	Word16 x1;
} pre_proc_type;

typedef struct
{
	Word16 past_qua_en[4];
	
} qua_gain_type;

typedef struct
{
	Word16 freq_prev[MA_NP][M];
	
} qua_lsp_type;

typedef struct
{
	Word32 L_exc_err[4];
	
} tamping_type;

typedef struct
{
	Word16 MeanLSF[M];							
	Word16 Min_buffer[16];						//+20
	Word16 Prev_Min, Next_Min, Min;				//+52, 54, 56
	Word16 MeanE, MeanSE, MeanSLE, MeanSZC;		//+58, 60, 62, 64
	Word16 prev_energy;							//+66
	Word16 count_sil, count_update, count_ext;	//+68, 70, 72
	Word16 flag, v_flag, less_count;			//+74, 76, 78

} vad_type;

typedef struct 
{
	Word16 lspSid_q[M] ;					
	Word16 pastCoeff[MP1];					
	Word16 RCoeff[MP1];						
	Word16 sh_RCoeff;						
	Word16 Acf[SIZ_ACF];					
	Word16 sh_Acf[NB_CURACF];				
	Word16 sumAcf[SIZ_SUMACF];				
	Word16 sh_sumAcf[NB_SUMACF];			
	Word16 ener[NB_GAIN];					
	Word16 sh_ener[NB_GAIN];				
	Word16 fr_cur;							
	Word16 cur_gain;						
	Word16 nb_ener;							
	Word16 sid_gain;						
	Word16 flag_chang;						
	Word16 prev_energy;						
	Word16 count_fr0;					
} dtx_type;

typedef struct
{
	Word16 noise_fg[MODE][MA_NP][M];
} qsidlsf_type;	
	
typedef struct
{	
	Word16 frame;
	Word16	 vad;	
} cod_global_type;


/* pointer of coder used memory */
typedef struct
{
	cod_ld8a_type*		pcod_ld8a;
	lpc_type*			plpc;				
	pre_proc_type* 		ppre_proc;				
	qua_gain_type* 		pqua_gain;				
	qua_lsp_type*		pqua_lsp;				
	tamping_type* 		ptamping;				
	vad_type*			pvad;				
	dtx_type*			pdtx;
	qsidlsf_type*		pqsidlsf;				
	cod_global_type*	pcod_global;				
} g729enc_type;

extern g729enc_type	g729encoder[2];
extern g729enc_type*	pg729enc;

#endif /* __COD_STRUCT_H_ */