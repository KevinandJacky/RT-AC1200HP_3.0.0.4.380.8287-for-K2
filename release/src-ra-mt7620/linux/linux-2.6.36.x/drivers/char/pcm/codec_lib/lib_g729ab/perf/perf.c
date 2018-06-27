//#include <inttypes.h>
#include <linux/types.h>

#include <asm/mipsregs.h>
#include "perf.h"


/* Local variables */
static volatile uint64 tempVariable64;
static volatile uint32 tempVariable32;
static volatile uint32 currCnt[2];

/* Global variables */
profile_stat_t ProfileStat[PROFILE_INDEX_MAX];
profile_stat_t TempProfStat[PROFILE_INDEX_MAX];

static uint32 profile_inited = 0;
static uint32 profile_enable = TRUE;

#define perf_print printk


static void perfcnt_init( void )
{
	unsigned long val = 0;
	
	val |= PERF_EN_KMODE;
	val |= (unsigned long)PERFCNT_EV_CYCLES<<5;
	write_c0_perfctrl0(val);
	
	val = 0;
	val |= PERF_EN_KMODE;
	val |= (unsigned long)PERFCNT_EV_ICOMPLETE<<5;
	write_c0_perfctrl1(val);
	
	write_c0_perfcntr0(0);
	write_c0_perfcntr1(0);
	
}

static void perfcnt_start( void )
{
	unsigned long val = 0;
	
	val = PERF_EN_UMODE;

	val = read_c0_perfctrl0();
	val |= PERF_EN_KMODE;
	val |= (unsigned long)PERFCNT_EV_CYCLES<<5;
	write_c0_perfctrl0(val);
	val = 0;
	val = read_c0_perfctrl0();
	val = 0;
	val = read_c0_perfctrl1();
	val |= PERF_EN_KMODE;
	val |= (unsigned long)PERFCNT_EV_ICOMPLETE<<5;
	write_c0_perfctrl1(val);
	
	write_c0_perfcntr0(0);
	write_c0_perfcntr1(0);
	
}

static void perfcnt_get( void )
{
	currCnt[0] = read_c0_perfcntr0();
	currCnt[1] = read_c0_perfcntr1();
	//printk("c0=%d c1=%d\n",currCnt[0],currCnt[1]);
}

static void perfcnt_stop( void )
{
	
	write_c0_perfctrl0(0);
	write_c0_perfctrl1(0);
	
}
int ProfileInit()
{
	int i;
	char* ptr;
	profile_inited = TRUE;
	profile_enable = TRUE;

	ptr = &ProfileStat;
	for( i = 0; i < sizeof( ProfileStat ); i++)
	{	
		*ptr = 0;
		ptr++;
	}
	ptr = &TempProfStat;
	for( i = 0; i < sizeof( TempProfStat ); i++)
	{	
		*ptr = 0;
		ptr++;
	}
	/* Temp */
	
	ProfileStat[PROFILE_INDEX_TEMP0].desc = "Temp0";
	ProfileStat[PROFILE_INDEX_TEMP0].valid = 1;

	ProfileStat[PROFILE_INDEX_TEMP1].desc = "Temp1";
	ProfileStat[PROFILE_INDEX_TEMP1].valid = 1;

	perfcnt_init();
	
	return SUCCESS;
}


int ProfileEnterPoint( uint32 index )
{
	if ( profile_inited == FALSE ||
	     profile_enable == FALSE ) return FAILED;
	if ( index >= (sizeof(ProfileStat)/sizeof(profile_stat_t)) )
		return FAILED;

	//perfcnt_init();
	//perfcnt_start();
	perfcnt_get();
	
	ProfileStat[index].tempCycle[0] = currCnt[0];
	ProfileStat[index].tempCycle[1] = currCnt[1];
	
	ProfileStat[index].hasTempCycle = TRUE;
	
	return SUCCESS;
}

int ProfileExitPoint( uint32 index )
{
	int i;
	uint32 delta_Cnt = 0;
	
	if ( profile_inited == FALSE ||
	     profile_enable == FALSE ) return FAILED;
	if ( index >= (sizeof(ProfileStat)/sizeof(profile_stat_t)) )
		return FAILED;
	if ( ProfileStat[index].hasTempCycle == FALSE )
		return FAILED;

	//perfcnt_init();	
	//perfcnt_stop();
	perfcnt_get();

	for( i = 0 ; i < 2; i ++ )
	{
		if(currCnt[i]>=ProfileStat[index].tempCycle[i])
		{	
			delta_Cnt = currCnt[i]-ProfileStat[index].tempCycle[i];
		}
		else
		{
			delta_Cnt = 0xFFFFFFFF - (ProfileStat[index].tempCycle[i]-currCnt[i]);
		}
		if(	delta_Cnt > 0x04000000)
			printk("%u, %u, %u\n", currCnt[i],ProfileStat[index].tempCycle[i],delta_Cnt);
		ProfileStat[index].accCycle[i] += (uint64)delta_Cnt;
		if(ProfileStat[index].maxCycle[i] < delta_Cnt)
			ProfileStat[index].maxCycle[i] = delta_Cnt;
	}
	
	ProfileStat[index].hasTempCycle = FALSE;
	ProfileStat[index].executedNum++;

	//perfcnt_start();
	
	return SUCCESS;
}

int ProfileDump( uint32 start, uint32 end, uint32 period )
{
	char szBuf[128];
	int i;

	profile_stat_t* statSnapShot = (profile_stat_t*)&TempProfStat[0];

	if(period > 0)
	{
		
		if( statSnapShot == NULL )
		{
			perf_print("statSnapShot mem alloc failed\n");
			return FAILED;
		}
		
		ProfileStat[start].per_count++;
			
		if(ProfileStat[start].per_count==period)
		{
	
			sprintf(szBuf, "index %15s %30s %8s %20s\n", "description", "accCycle", "totalNum", "Average" );
			perf_print(szBuf);
		
			for( i = start; i <= end; i++ )
			{
				if(ProfileStat[i].valid==0)
					continue;
				int j;
				for( j =0; j < 2; j++ )
				{
					statSnapShot[i].accCycle[j]  = ProfileStat[i].accCycle[j];
					statSnapShot[i].tempCycle[j] = ProfileStat[i].tempCycle[j];
				}
				statSnapShot[i].executedNum  = ProfileStat[i].executedNum;
				statSnapShot[i].hasTempCycle = ProfileStat[i].hasTempCycle; 
			}
		
			for( i = start; i <= end; i++ )
			{
				if(ProfileStat[i].valid==0)
					continue;
				if ( statSnapShot[i].executedNum == 0 )
				{
					sprintf(szBuf, "[%3d] %15s %30s %8s %20s\n", i, ProfileStat[i].desc, "--", "--", "--" );
					perf_print(szBuf);
				}
				else
				{
					int j;
					sprintf(szBuf, "[%3d] %15s ", i, ProfileStat[i].desc );
					perf_print(szBuf);
					for( j =0; j < sizeof(ProfileStat[i].accCycle)/sizeof(uint64); j++ )
					{
						uint32 *pAccCycle = (uint32*)&statSnapShot[i].accCycle[j];
						uint32 avrgCycle; 
						
						avrgCycle = div64_32(statSnapShot[i].accCycle[j], statSnapShot[i].executedNum);
						sprintf(szBuf,  "%30llu %8u %20u\n",
							statSnapShot[i].accCycle[j], 
							statSnapShot[i].executedNum, 
							avrgCycle );
						perf_print(szBuf);
		
						sprintf(szBuf, " %3s  %15s ", "", "" );
						perf_print(szBuf);
					}
					perf_print( "\r" );
				}
			}
			
			
			ProfileStat[start].per_count = 0;
			
			for (i=0; i<sizeof(ProfileStat[i].accCycle)/sizeof(uint64); i++)
			{
				ProfileStat[start].accCycle[i]=0;
				ProfileStat[start].executedNum=0;
				ProfileStat[start].hasTempCycle=0;
				ProfileStat[start].tempCycle[i]=0;
				
				statSnapShot[start].accCycle[i]=0;
				statSnapShot[start].tempCycle[i]=0;
				statSnapShot[start].executedNum=0;
				statSnapShot[start].hasTempCycle=0;
			}
			
			return SUCCESS;
		}
		


	}

	return SUCCESS;

}


int ProfilePerDump( uint32 start, uint32 period )
{
	
	char szBuf[128];
	int i;
	
	if(period > 0)
	{	
		if(ProfileStat[start].valid==0)
			return FAILED;

		ProfileStat[start].per_count++;
		if(ProfileStat[start].per_count==period)
		{

			for( i = start; i <= start; i++ )
			{
				int j;
				for( j =0; j < sizeof(ProfileStat[i].maxCycle)/sizeof(uint32); j++ )
				{
					sprintf(szBuf,"MAX[%d][%d]=%u\n",i,j,ProfileStat[i].maxCycle[j]);
					perf_print(szBuf);
					ProfileStat[i].maxCycle[j] = 0;

				}
			}
			ProfileStat[start].per_count = 0;
		}
		return SUCCESS;
	}


	return SUCCESS;

}

