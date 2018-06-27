#include "typedef.h"
#include "dec_struct.h"

dec_ld8a_type		dec_ld8a0 	ALIGN(8) MEM_SECTION(".dec0");
dec_gain_type		dec_gain0 	ALIGN(8) MEM_SECTION(".dec0");
lspdec_type			lspdec0 	ALIGN(8) MEM_SECTION(".dec0");
post_pro_type		post_pro0 	ALIGN(8) MEM_SECTION(".dec0");
postfilt_type		post_filt0 	ALIGN(8) MEM_SECTION(".dec0");
dec_global_type		dec_global0 ALIGN(8) MEM_SECTION(".dec0");
dec_sid_type		dec_sid0 	ALIGN(8) MEM_SECTION(".dec0");

dec_ld8a_type		dec_ld8a1	ALIGN(8) MEM_SECTION(".dec1");
dec_gain_type		dec_gain1	ALIGN(8) MEM_SECTION(".dec1");
lspdec_type			lspdec1		ALIGN(8) MEM_SECTION(".dec1");
post_pro_type		post_pro1	ALIGN(8) MEM_SECTION(".dec1");
postfilt_type		post_filt1	ALIGN(8) MEM_SECTION(".dec1");
dec_global_type		dec_global1	ALIGN(8) MEM_SECTION(".dec1");
dec_sid_type		dec_sid1	ALIGN(8) MEM_SECTION(".dec1");

g729dec_type		g729decoder[2];
g729dec_type*		pg729dec;

int set_g729decVoIPCh(int nch)
{
	pg729dec = &g729decoder[nch];
	switch(nch)
	{
		case 0:
			pg729dec->pdec_ld8a = &dec_ld8a0;
			pg729dec->pdec_gain = &dec_gain0;
			pg729dec->plspdec = &lspdec0;
			pg729dec->ppost_pro = &post_pro0;
			pg729dec->ppost_filt = &post_filt0;
			pg729dec->pdec_sid = &dec_sid0;
			pg729dec->pdec_global = &dec_global0;
			break;
		case 1:
			pg729dec->pdec_ld8a = &dec_ld8a1;
			pg729dec->pdec_gain = &dec_gain1;
			pg729dec->plspdec = &lspdec1;
			pg729dec->ppost_pro = &post_pro1;
			pg729dec->ppost_filt = &post_filt1;
			pg729dec->pdec_sid = &dec_sid1;
			pg729dec->pdec_global = &dec_global1;
			break;
	}			
	return 0;
}