#include <asm/mipsregs.h>
//#include <inttypes.h>
#include <linux/types.h>
#include "perf.h"

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define GCC_IMM_ASM "n"
#define GCC_REG_ACCUM "$0"
#else
#define GCC_IMM_ASM "rn"
#define GCC_REG_ACCUM "accum"
#endif

/*
* No traps on overflows for any of these...
*/

#define do_div64_32(res, high, low, base) ({ \
     unsigned long __quot32, __mod32; \
     unsigned long __cf, __tmp, __tmp2, __i; \
     \
     __asm__(".set   push\n\t" \
             ".set   noat\n\t" \
             ".set   noreorder\n\t" \
             "move   %2, $0\n\t" \
             "move   %3, $0\n\t" \
             "b      1f\n\t" \
             " li    %4, 0x21\n" \
             "0:\n\t" \
             "sll    $1, %0, 0x1\n\t" \
             "srl    %3, %0, 0x1f\n\t" \
             "or     %0, $1, %5\n\t" \
             "sll    %1, %1, 0x1\n\t" \
             "sll    %2, %2, 0x1\n" \
             "1:\n\t" \
             "bnez   %3, 2f\n\t" \
             " sltu  %5, %0, %z6\n\t" \
             "bnez   %5, 3f\n" \
             "2:\n\t" \
             " addiu %4, %4, -1\n\t" \
             "subu   %0, %0, %z6\n\t" \
             "addiu  %2, %2, 1\n" \
             "3:\n\t" \
             "bnez   %4, 0b\n\t" \
             " srl   %5, %1, 0x1f\n\t" \
             ".set   pop" \
             : "=&r" (__mod32), "=&r" (__tmp), \
               "=&r" (__quot32), "=&r" (__cf), \
               "=&r" (__i), "=&r" (__tmp2) \
             : "Jr" (base), "0" (high), "1" (low)); \
     \
     (res) = __quot32; \
     __mod32; })
     
uint32 div64_32(uint64 num, uint32 denum)
{
	uint32 res = 0;
	
	do_div64_32(res, num>>32, num, denum);
	
	return res;
}    