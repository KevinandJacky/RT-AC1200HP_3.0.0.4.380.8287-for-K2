#ifndef __DEC_STRUCT_H_
#define __DEC_STRUCT_H_

#include "typedef.h"
#include "ld8a.h"

typedef struct
{
	Word16 old_exc[L_FRAME+PIT_MAX+L_INTERPOL] ALIGN(8);
	Word16 *exc;								
	Word16 lsp_old[M] ;						
	Word16 mem_syn[M] ;							
	Word16 sharp;								
	Word16 old_T0;								
	Word16 gain_code;							
	Word16 gain_pitch;							
	Word16 seed_fer;							
	Word16 past_ftyp;							
	Word16 seed;								
	Word16 sid_sav; 
	Word16 sh_sid_sav;
	Word16 bad_lsf;					
} dec_ld8a_type;

typedef struct 
{
	Word16 past_qua_en[4] ALIGN(4);				
} dec_gain_type;

typedef struct
{	
	Word16 noise_fg[MODE][MA_NP][M] ALIGN(4);
	Word16 lspSid[M];						
	Word16 cur_gain;						
	Word16 sid_gain;						
	
} dec_sid_type;

typedef struct
{
	Word16 freq_prev[MA_NP][M] ALIGN(4);					
	Word16 prev_ma;
	Word16 prev_lsp[M];
} lspdec_type;


typedef struct
{
	Word16 y2_hi ALIGN(4); 
	Word16 y2_lo; 
	Word16 y1_hi; 
	Word16 y1_lo; 
	Word16 x0; 
	Word16 x1;
} post_pro_type;

typedef struct
{
	Word16 res2_buf[PIT_MAX+L_SUBFR+1] ALIGN(4); 	
	Word16 *res2;								
	Word16 scal_res2_buf[PIT_MAX+L_SUBFR+1];	
	Word16 *scal_res2;							
	Word16 mem_syn_pst[M];						
	Word16 mem_pre;								
	Word16 past_gain;							
	
} postfilt_type;

typedef struct
{				
	Word16 synth_buf[L_FRAME+M] ALIGN(8);
	Word16 *synth;					
} dec_global_type;



typedef struct
{
	dec_ld8a_type*		pdec_ld8a;
	dec_gain_type*		pdec_gain;			
	lspdec_type*		plspdec;			
	post_pro_type*		ppost_pro;			
	postfilt_type*		ppost_filt;				
	dec_global_type*	pdec_global;				
	dec_sid_type* 		pdec_sid;
} g729dec_type;

extern g729dec_type		g729decoder[2];
extern g729dec_type*	pg729dec;

#endif /* __DEC_STRUCT_H_ */