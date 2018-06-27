#ifndef _PERF_H_
#define _PERF_H_

//#include <inttypes.h>
#ifdef __KERNEL__
#include <linux/types.h>
#endif 
#include "perf_index.h"

#define PERF_M_BIT			0x80000000
#define PERF_IE_EN			0x10
#define PERF_EN_UMODE		0x8			
#define PERF_EN_SMODE		0x4
#define PERF_EN_KMODE		0x2
#define PERF_EN_EXL			0x1


/* Performance counter event numbers for 
   PerfCnt Registers (CP0 Register 25, Select 0 and 1) */
#define PERFCNT_EV_CYCLES	0
#define PERFCNT_EV_ICOMPLETE	1
#define PERFCNT_EV_BRANCH_0	2
#define PERFCNT_EV_BMISS_1	2
#define PERFCNT_EV_JR31_0	3
#define PERFCNT_EV_JR31MISS_1	3
#define PERFCNT_EV_JRN31_0	4
#define PERFCNT_EV_ITLB_0	5
#define PERFCNT_EV_ITLBMISS_1	5
#define PERFCNT_EV_DTLB_0	6
#define PERFCNT_EV_DTLBMISS_1	6
#define PERFCNT_EV_JTLBI_0	7
#define PERFCNT_EV_JTLBIMISS_1	7
#define PERFCNT_EV_JTLBD_0	8
#define PERFCNT_EV_JTLBDMISS_1	8
#define PERFCNT_EV_ICACHE_0	9
#define PERFCNT_EV_ICACHEMISS_1	9
#define PERFCNT_EV_DCACHE_0	10
#define PERFCNT_EV_DCACHEWB_1	10
#define PERFCNT_EV_DCACHEMISS	11
#define PERFCNT_EV_EIV		12
#define PERFCNT_EV_EIV_DIRTY_0	13
#define PERFCNT_EV_EIV_CLEAN_1	13
#define PERFCNT_EV_INT_COMPL_0	14
#define PERFCNT_EV_FP_COMPL_1	14
#define PERFCNT_EV_LD_COMPL_0	15
#define PERFCNT_EV_ST_COMPL_1	15
#define PERFCNT_EV_J_COMPL_0	16
#define PERFCNT_EV_M16_COMPL_1	16
#define PERFCNT_EV_NOP_COMPL_0	17
#define PERFCNT_EV_MD_COMPL_1	17
#define PERFCNT_EV_STALLS_0	18
#define PERFCNT_EV_REPLAYS_1	18
#define PERFCNT_EV_SC_COMPL_0	19
#define PERFCNT_EV_SC_FAIL_1	19
#define PERFCNT_EV_PF_COMPL_0	20
#define PERFCNT_EV_PF_HIT_1	20
#define PERFCNT_EV_L2_WB_0	21
#define PERFCNT_EV_L2_ACC_1	21
#define PERFCNT_EV_L2_MISS	22
#define PERFCNT_EV_EXC_0	23
#define PERFCNT_EV_FIXUP_0	24

#define PERFCNT_EV_MPIPE_STCYC_1				25
#define PERFCNT_EV_DSPICOMPLETE_0				26
#define PERFCNT_EV_ALUDSP_SADONE_1				26
#define PERFCNT_EV_MDUDSP_SADONE_1				27

#define PERFCNT_EV_UNCACHE_LOAD_0				33
#define PERFCNT_EV_UNCACHE_STORE_1				33

#define PERFCNT_EV_ICACHEMISS_BLKCYC_0			37
#define PERFCNT_EV_DCACHEMISS_BLKCYC_1			37

#define PERFCNT_EV_L2IMISS_STCYC_0				38
#define PERFCNT_EV_L2DMISS_STCYC_1				38

#define PERFCNT_EV_UNCACHEACCESS_BLKCYC_0		40

#define PERFCNT_EV_MDU_STCYC_0					41
#define PERFCNT_EV_FPU_STCYC_1					41
	
#define PERFCNT_EV_READCP0_STALL_0				46
#define PERFCNT_EV_BR_MISPREDCYC_1				46	

#define PERFCNT_EV_FSB_LOW_0						50
#define PERFCNT_EV_FSB_MED_1						50
#define PERFCNT_EV_FSB_HIGH_0						51
#define PERFCNT_EV_FSB_FULL_1						51
#define PERFCNT_EV_LDQ_LOW_0						52
#define PERFCNT_EV_LDQ_MED_1						52
#define PERFCNT_EV_LDQ_HIGH_0						53
#define PERFCNT_EV_LDQ_FULL_1						53
#define PERFCNT_EV_WBB_LOW_0						54
#define PERFCNT_EV_WBB_MED_1						54
#define PERFCNT_EV_WBB_HIGH_0						55
#define PERFCNT_EV_WBB_FULL_1						55


#define uint64 unsigned long long
#define uint32 unsigned long

#define TRUE 1
#define FALSE 0
#define SUCCESS 0
#define FAILED	1
#define NULL 0

struct profile_stat_s {
	uint32 maxCycle[2];
	uint64 accCycle[2];
	uint32 tempCycle[2];
	uint32 executedNum;
	uint32 hasTempCycle; /* true if tempCycle is valid. */
	uint32 per_count;
	int		valid;
	char *desc;
};

typedef struct profile_stat_s profile_stat_t;

int ProfileInit( void );
int ProfileEnterPoint( uint32 index );
int ProfileExitPoint( uint32 index );
int ProfileDump( uint32 start, uint32 end, uint32 period );
int ProfilePerDump( uint32 start, uint32 period );

#endif /* _PERF_H_ */