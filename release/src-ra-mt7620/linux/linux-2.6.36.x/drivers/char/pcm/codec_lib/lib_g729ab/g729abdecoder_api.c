#include "typedef.h"
#include "ld8a.h"
#include "basic_op.h"
#include "dec_struct.h"

static Word16 read_frame(Word16 *i_serial, Word16 *parm);

static short decin[L_FRAME/8+3];
#if 0
static Word16  synth_buf[L_FRAME+M];
static Word16 *synth;
extern Word16 bad_lsf;        /* bad LSF indicator   */
#endif

void bit2parm(char *dec_in_ptr, Word16 *parm, Flag bfi)
{
	Word16 i,j = 0;	
	Word16 bl=0;
	Word16 serial[SERIAL_SIZE];
	
	if (bfi == 1)
	{
		parm[0] = 1;			// bad frame
		return ;
	}
	
	switch (dec_in_ptr[0])
	{
	case 0x00: 	// not TX
		serial[1] = 0;
		bl = 0;	// length = 0
		break;
	case 0x01:	// active
		serial[1] = 80;
		bl = 80;
		break;
	case 0x02:	// SID
		serial[1] = 16;
		bl = 16;
		break;	
	case 0x03:	// bad frame
		serial[1] = 0;
	}
  
  	//if (vad_enable == 0)
		j = 2;

  	for (i=0; i<bl;i++)
    	serial[i+j] =  (dec_in_ptr[1 + (i>>3)] >> (7 - (i & 0x0007))) & 1;
  	
  	parm[0] = 0;           /* No frame erasure */

  	bits2prm_ld8k(&serial[1], parm);

	if (*dec_in_ptr == 0x3)
		parm[0] = 1;			// bad frame
	
  	if(parm[1] == 1) {
    /* check parity and put 1 in parm[5] if parity error */
	// NOTE !! parm[5] be modified !!
    	parm[5] = Check_Parity_Pitch(parm[4], parm[5]);
  	}
}

int g729ab_decode_init()
{
	int i, val;
#ifdef RALINK_VOIP_CVERSION
#else	
	set_ase();
#endif	
	Init_Decod_ld8a();
	Init_Post_Filter();
	Init_Post_Process();

	/* for G.729b */
	Init_Dec_cng();
	
	for ( i = 0 ; i < M ; i ++ ) pg729dec->pdec_global->synth_buf[i] = 0;
  		pg729dec->pdec_global->synth = pg729dec->pdec_global->synth_buf + M;
  			
	return 0;
}


int g729ab_decode_frame(short* pBit, short* pPCM, int bErase)
{
	int Vad;
	Word16  Az_dec[MP1*2];                /* Decoded Az for post-filter  */
  	Word16  T2[2];                        /* Pitch lag for 2 subframes   */
 	static int pos = 0;
 	extern const short tstseq4_bit[];
 	 	
  	pg729dec->pdec_ld8a->bad_lsf = bErase;
  	Set_zero(decin, L_FRAME/8+3);
#if defined(RALINK_G729AB_VERIFY)  	
	//pos = read_frame(pBit, decin);
	pos+= read_frame(&tstseq4_bit[pos], decin);
	if(pos>=40000)
		pos = 0;
#else	
	bit2parm(pBit, decin, 0);
#endif	

	Decod_ld8a(decin, pg729dec->pdec_global->synth, Az_dec, T2, &Vad);
	Post_Filter(pg729dec->pdec_global->synth, Az_dec, T2, Vad);        /* Post-filter */
    Post_Process(pg729dec->pdec_global->synth, L_FRAME);
    
    Copy(pg729dec->pdec_global->synth, pPCM, L_FRAME);
    return 0;
}

static Word16 read_frame(Word16 *i_serial, Word16 *parm)
{
	int pos = 0;
	Word16  i;
	Word16  serial[SERIAL_SIZE];          /* Serial stream               */

	serial[0] = i_serial[0];
	serial[1] = i_serial[1];
	pos+=2;//*sizeof(short);

	for(i=0;i<serial[1];i++)
		serial[2+i] = i_serial[i+pos];

	pos+=(serial[1]);//*sizeof(short);

	bits2prm_ld8k(&serial[1], parm);

	/* the hardware detects frame erasures by checking if all bits
		are set to zero */
	parm[0] = 0;           /* No frame erasure */
	for (i=0; i < serial[1]; i++)
		if (serial[i+2] == 0 ) parm[0] = 1;  /* frame erased     */

	if(parm[1] == 1) {
		/* check parity and put 1 in parm[5] if parity error */
		parm[5] = Check_Parity_Pitch(parm[4], parm[5]);
	}

	return(pos);
}