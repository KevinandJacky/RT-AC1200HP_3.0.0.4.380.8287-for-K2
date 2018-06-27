#include "typedef.h"
#include "ld8a.h"
#include "basic_op.h"
#include "cod_struct.h"

static Word16 encout[L_FRAME+2];
static int g729_nframe = 0;
static Word16 serial[SERIAL_SIZE];

//extern Word16 *new_speech;  

void pack2bit(Word16 *serial, Word16 enc_bytes, char *pEncOutBuff)
{
	short i;

	/* clear output buffer, because we only update output buffer with bitmask */
	for(i=0; i<PRM_SIZE; i++)			
	  pEncOutBuff[i] = 0;
	
	switch(enc_bytes)
	{
	case 80:	/* active	*/
		pEncOutBuff[0] = 0x1;
		serial += 2;
		for (i=0; i<enc_bytes; i++)
		   pEncOutBuff[1 + (i>>3)] ^= (*serial++ << ( 7 - (i & 0x0007)));
		break;
	case 0:		/* not transmitted */
		pEncOutBuff[0] = 0x00;
		pEncOutBuff[1] = 0;
		break;
	case 16:	/* sid */
		pEncOutBuff[0] = 0x2;
		serial += 2;		
		for (i=0;i<enc_bytes;i++)			
		   pEncOutBuff[1 + (i>>3)] ^= (*serial++ << ( 7 - (i & 0x0007)));				
		break;
	}
	
	return;
}	

int g729ab_encode_init()
{
	int val;
#ifdef RALINK_VOIP_CVERSION
#else		
	set_ase();
#endif	
	Init_Pre_Process();
	
	Init_Coder_ld8a();
	Set_zero(encout, PRM_SIZE+1); 
	
	/* for G.729B */
	Init_Cod_cng();
	
	return 0;
}

int g729ab_encode_frame(short* pSrc, short* pBit, int bVAD)
{
	int nb_words;
	
	Copy(pSrc, pg729enc->pcod_ld8a->new_speech, L_FRAME);

	Pre_Process(pg729enc->pcod_ld8a->new_speech, L_FRAME);
	
	if (g729_nframe == 32767) g729_nframe = 256;
    else g729_nframe++;
    Set_zero(encout, L_FRAME+2);
    Coder_ld8a(encout, g729_nframe, bVAD);
    Set_zero(serial, SERIAL_SIZE);
    prm2bits_ld8k(encout, serial);
#if defined(RALINK_G729AB_VERIFY)
	nb_words = serial[1]*2 + (Word16)2;
	Copy(serial, (short*)pBit, nb_words);
	return nb_words;
#else
    nb_words = serial[1];
    pack2bit(serial, nb_words, pBit);
#endif    
   
    return nb_words/8+1;;
}
