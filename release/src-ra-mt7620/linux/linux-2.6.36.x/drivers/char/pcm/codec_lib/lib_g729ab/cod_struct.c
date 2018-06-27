#include "typedef.h"
#include "cod_struct.h"

cod_ld8a_type			cod_ld8a0	ALIGN(8) MEM_SECTION(".cod0");
lpc_type				lpc0		ALIGN(8) MEM_SECTION(".cod0");
pre_proc_type			pre_proc0	ALIGN(8) MEM_SECTION(".cod0");
qua_gain_type			qua_gain0	ALIGN(8) MEM_SECTION(".cod0");
qua_lsp_type			qua_lsp0	ALIGN(8) MEM_SECTION(".cod0");
tamping_type			tamping0	ALIGN(8) MEM_SECTION(".cod0");
vad_type				vad0		ALIGN(8) MEM_SECTION(".cod0");
dtx_type				dtx0		ALIGN(8) MEM_SECTION(".cod0");
cod_global_type			cod_global0	ALIGN(8) MEM_SECTION(".cod0");
qsidlsf_type			qsidlsf0	ALIGN(8) MEM_SECTION(".cod0");

cod_ld8a_type			cod_ld8a1	ALIGN(8) MEM_SECTION(".cod1");
lpc_type				lpc1		ALIGN(8) MEM_SECTION(".cod1");
pre_proc_type			pre_proc1	ALIGN(8) MEM_SECTION(".cod1");
qua_gain_type			qua_gain1	ALIGN(8) MEM_SECTION(".cod1");
qua_lsp_type			qua_lsp1	ALIGN(8) MEM_SECTION(".cod1");
tamping_type			tamping1	ALIGN(8) MEM_SECTION(".cod1");
vad_type				vad1		ALIGN(8) MEM_SECTION(".cod1");
dtx_type				dtx1		ALIGN(8) MEM_SECTION(".cod1");
cod_global_type			cod_global1	ALIGN(8) MEM_SECTION(".cod1");
qsidlsf_type			qsidlsf1	ALIGN(8) MEM_SECTION(".cod1");

g729enc_type*	pg729enc;
g729enc_type	g729encoder[2];

int set_g729encVoIPCh(int nch)
{
	pg729enc = &g729encoder[nch];
	switch(nch)
	{
		case 0:
			pg729enc->pcod_ld8a = &cod_ld8a0;
			pg729enc->plpc = &lpc0;
			pg729enc->ppre_proc = &pre_proc0;
			pg729enc->pqua_gain = &qua_gain0;
			pg729enc->pqua_lsp = &qua_lsp0;
			pg729enc->ptamping = &tamping0;
			pg729enc->pvad = &vad0;
			pg729enc->pdtx = &dtx0;
			pg729enc->pqsidlsf = &qsidlsf0;
			pg729enc->pcod_global = &cod_global0;
		
			break;
		case 1:
			pg729enc->pcod_ld8a = &cod_ld8a1;
			pg729enc->plpc = &lpc1;
			pg729enc->ppre_proc = &pre_proc1;
			pg729enc->pqua_gain = &qua_gain1;
			pg729enc->pqua_lsp = &qua_lsp1;
			pg729enc->ptamping = &tamping1;
			pg729enc->pvad = &vad1;
			pg729enc->pdtx = &dtx1;
			pg729enc->pqsidlsf = &qsidlsf1;
			pg729enc->pcod_global = &cod_global1;
			break;
	}			
	return 0;
}