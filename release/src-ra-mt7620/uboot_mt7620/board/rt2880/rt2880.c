/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <asm/addrspace.h>

#define MAX_SDRAM_SIZE	(256*1024*1024)
#define MIN_SDRAM_SIZE	(8*1024*1024)

#ifdef SDRAM_CFG_USE_16BIT
#define MIN_RT2880_SDRAM_SIZE	(16*1024*1024)
#else
#define MIN_RT2880_SDRAM_SIZE	(32*1024*1024)
#endif

extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

/*
 * Check memory range for valid RAM. A simple memory test determines
 * the actually available RAM size between addresses `base' and
 * `base + maxsize'.
 */
long get_ram_size(volatile long *base, long maxsize)
{
	volatile long *addr;
	long           save[32];
	long           cnt;
	long           val;
	long           size;
	int            i = 0;

	for (cnt = (maxsize / sizeof (long)) >> 1; cnt > 0; cnt >>= 1) {
		addr = base + cnt;	/* pointer arith! */
		save[i++] = *addr;
		
		*addr = ~cnt;

		
	}

	addr = base;
	save[i] = *addr;

	*addr = 0;

	
	if ((val = *addr) != 0) {
		/* Restore the original data before leaving the function.
		 */
		*addr = save[i];
		for (cnt = 1; cnt < maxsize / sizeof(long); cnt <<= 1) {
			addr  = base + cnt;
			*addr = save[--i];
		}
		return (0);
	}

	for (cnt = 1; cnt < maxsize / sizeof (long); cnt <<= 1) {
		addr = base + cnt;	/* pointer arith! */

	//	printf("\n retrieve addr=%08X \n",addr);
			val = *addr;
		*addr = save[--i];
		if (val != ~cnt) {
			size = cnt * sizeof (long);

			if(size <= MIN_SDRAM_SIZE)
			{
				print_size(size, "\n");
				printf("addr[%08lx] val(%08lx) ~cnt(%08lx)\n", addr, val, ~cnt);
			}
		//	printf("\n The Addr[%08X],do back ring  \n",addr);
			
			/* Restore the original data before leaving the function.
			 */
			for (cnt <<= 1; cnt < maxsize / sizeof (long); cnt <<= 1) {
				addr  = base + cnt;
				*addr = save[--i];
			}
			return (size);
		}
	}

	return (maxsize);
}



long int initdram(int board_type)
{
	ulong size, max_size       = MAX_SDRAM_SIZE;
	ulong our_address;
  
	asm volatile ("move %0, $25" : "=r" (our_address) :);

	/* Can't probe for RAM size unless we are running from Flash.
	 */
#if 0	 
	#if defined(CFG_RUN_CODE_IN_RAM)

	printf("\n In RAM run \n"); 
    return MIN_SDRAM_SIZE;
	#else

	printf("\n In FLASH run \n"); 
    return MIN_RT2880_SDRAM_SIZE;
	#endif
#endif 
    
#if defined (RT2880_FPGA_BOARD) || defined (RT2880_ASIC_BOARD)
	if (PHYSADDR(our_address) < PHYSADDR(PHYS_FLASH_1))
	{
	    
		//return MIN_SDRAM_SIZE;
		//fixed to 32MB
		printf("\n In RAM run \n");
		return MIN_SDRAM_SIZE;
	}
#endif
	 


	size = get_ram_size((ulong *)CFG_SDRAM_BASE, MAX_SDRAM_SIZE);
	if (size <= MIN_SDRAM_SIZE)
	{
		printf("RAM size (0x%08x) too small !!! do_reset\n", size);
		udelay(100 * 1000);
		do_reset (NULL, 0, 0, NULL);
	}
	if (size > max_size)
	{
		max_size = size;
	//	printf("\n Return MAX size!! \n");
		return max_size;
	}
//	printf("\n Return Real size =%d !! \n",size);
	return size;
	
}

#if defined(ASUS_PRODUCT)
const char *model =
#if defined(CONFIG_RTN14U)
	"RT-N14U";
#elif defined(CONFIG_RTAC52U)
	"RT-AC52U";
#elif defined(CONFIG_RTAC51U)
	"RT-AC51U";
#elif defined(CONFIG_RTN54U)
	"RT-N54U";
#elif defined(CONFIG_RTAC54U)
	"RT-AC54U";
#elif defined(CONFIG_RTN56UB1)
	"RT-N56UB1";
#elif defined(CONFIG_RTN56UB2)
	"RT-N56UB2";
#elif defined(CONFIG_RTAC1200HP)
	"RT-AC1200HP";
#elif defined(CONFIG_RTN11P)
	"RT-N11P";
#elif defined(CONFIG_RTN300)
	"RT-N300";
#else
	"ASUS PRODUCT";
#endif

const char *blver =
#if defined(ASUS_RTN14U)
	"1001";
#elif defined(ASUS_RTAC52U)
	"1011";
#elif defined(ASUS_RTAC51U)
	"1000";
#elif defined(ASUS_RTN54U)
	"1000";
#elif defined(ASUS_RTAC54U)
	"1000";
#elif defined(ASUS_RTN56UB1)
	"1000";
#elif defined(ASUS_RTN56UB2)
	"1000";
#elif defined(ASUS_RTAC1200HP)
	"1000";
#elif defined(ASUS_RTN11P)
	"1000";
#elif defined(CONFIG_RTN300)
	"1000";
#else
#error Define bootload version.
#endif

const char *bl_stage =
#if defined(UBOOT_STAGE1)
	" stage1";
#elif defined(UBOOT_STAGE2)
	" stage2";
#else
	"";
#endif
#endif


int checkboard (void)
{
#if defined(ASUS_PRODUCT)
	const char *bv = &blver[0];

#if defined(UBOOT_STAGE1)
#define UBOOT_STAGE1_VER "1011"
	bv = UBOOT_STAGE1_VER;
#endif
	printf("%s bootloader%s version: %c.%c.%c.%c\n",
		model, bl_stage, bv[0], bv[1], bv[2], bv[3]);
#endif
	puts ("Board: Ralink APSoC ");
	return 0;
}

