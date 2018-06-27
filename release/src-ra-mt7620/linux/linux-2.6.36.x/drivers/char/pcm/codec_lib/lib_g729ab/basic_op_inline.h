#ifndef _BASIC_OP_INLINE__
#define _BASIC_OP_INLINE__

#include "mips.h"

#define __asm__ asm
#define ASM __asm__ volatile

#define set_ase()	\
({	\
	ASM ("mfc0	$15, $12" : /* no outputs */ : /* no inputs */ : "$15");	\
	ASM ("move	$14, $0" : /* no outputs */ : /* no inputs */);	\
	ASM ("lui	$14, 0x0100" : /* no outputs */ : /* no inputs */ : "$15");	\
	ASM ("or	$15, $15, $14" : /* no outputs */ : /* no inputs */ : "$15");	\
	ASM ("mtc0	$15, $12" : /* no outputs */ : /* no inputs */ : "$15");	\
})

#define add_inline(var1,var2)	\
({	\
	long __result;	\
	ASM ("addq_s.ph	$15, %0, %1" : /* no outputs */ : "d" ((short)(var1)), "d" ((short)(var2)):"$15");	\
	ASM ("sll		$15, $15, 16" : /* no outputs */ : /* no inputs */ :"$15");	\
	ASM ("sra		%0, $15, 16" : "=d" ((long)__result) : /* no inputs */:"$15");	\
	__result; \
})

#define sub_inline(var1,var2)	\
({	\
	short __result;	\
	ASM ("subq_s.ph	$15, %0, %1" : /* no outputs */ : "d" ((short)(var1)), "d" ((short)(var2)):"$15"); \
	ASM ("sll		$15, $15, 16": /* no outputs */ : /* no inputs */ :"$15"); \
	ASM ("sra		%0, $15, 16" : "=d" ((short)__result) : /* no inputs */:"$15"); \
	__result; \
})

#define abs_s_inline(var1)	\
({	\
	short __result;	\
	ASM ("absq_s.ph	$15, %0" : /* no outputs */ : "d" ((short)(var1)):"$15"); \
	ASM ("sll		$15, $15, 16" : /* no outputs */ : /* no inputs */ :"$15"); \
	ASM ("sra		%0, $15, 16" : "=d" ((short)__result) : /* no inputs */:"$15"); \
	__result; \
})

#define shl_inline(var1, var2)	\
({	\
	short __result;	\
	ASM ("move	$10, %0" : /* no outputs */ : "r" ((long)(var2)):"$10"); \
	ASM ("move	$11, %0" : /* no outputs */ : "r" ((long)(var1)):"$11"); \
	ASM ("clz	$12, $11" : /* no outputs */ : /* no inputs */:"$12"); \
	ASM ("clo	$13, $11" : /* no outputs */ : /* no inputs */:"$13"); \
	ASM ("slt	$14, $11, $0" : /* no outputs */ : /* no inputs */ : "$14"); \
	ASM ("movn	$12, $13, $14" : /* no outputs */ : /* no inputs */ : "$12"); \
	ASM ("li	$14, 16" : /* no outputs */ : /* no inputs */ : "$14"); \
	ASM ("sub	$12, $12, $14" : /* no outputs */ : /* no inputs */ : "$12"); \
	ASM ("sllv	$13, $11, $10" : /* no outputs */ : /* no inputs */ : "$13"); \
	ASM ("sub	$12, $12, $10" : /* no outputs */ : /* no inputs */ : "$12"); \
	ASM ("blez	$12, 1f" : /* no outputs */ : /* no inputs */ : "$12"); \
	ASM ("nop" : /* no outputs */ : /* no inputs */); \
	ASM ("slt	$15, $10, $14" : /* no outputs */ : /* no inputs */:"$15"); \
	ASM ("movn	$14, $0, $11" : /* no outputs */ : /* no inputs */:"$14"); \
	ASM ("or	$15,$15,$14" : /* no outputs */ : /* no inputs */:"$15"); \
	ASM ("beqz	$15, 1f" : /* no outputs */ : /* no inputs */:"$15"); \
	ASM ("nop" : /* no outputs */ : /* no inputs */); \
	ASM ("xor   $15,$11,$13" : /* no outputs */ : /* no inputs */:"$15"); \
	ASM ("bgez  $15, 4f" : /* no outputs */ : /* no inputs */:"$15"); \
	ASM ("nop" : /* no outputs */ : /* no inputs */); \
	ASM ("1:bltz	$11, 3f" : /* no outputs */ : /* no inputs */:"$11"); \
	ASM ("nop\n\t2:" : /* no outputs */ : /* no inputs */); \
	ASM ("li	$13, 0x00007fff" : /* no outputs */ : /* no inputs */:"$13");	\
	ASM ("j		4f\n\t3:");\
	ASM ("li	$13, 0x00008000\n\t4:" : /* no outputs */ : /* no inputs */:"$13");	\
	ASM ("move	%0, $13" : "=d" ((short)__result) : /* no inputs */); \
	__result; \
})

#define shr_inline(var1, var2)	\
({	\
	short __result;	\
	ASM ("li		$16, 15" : /* no outputs */ : /* no inputs */:"$16");	\
	ASM ("slt		$14, %0, $16" : /* no outputs */  :  "d" ((short)(var2)) : "$14");	\
	ASM ("movn		$16, %0, $14" : /* no outputs */  :  "d" ((short)(var2)) : "$16");	\
	ASM ("seh		$14, %0" : /* no outputs */ :  "d" ((short)(var1)) : "$14");	\
	ASM ("srav		%0,  $14, $16" : "=d" ((short)__result) :  /* no inputs */ : "$14");	\
	__result; \
})

#define mult_inline(var1, var2)	\
({	\
	short __result;	\
	ASM (".set	noreorder");	\
	ASM ("muleq_s.w.phr	$11, %0, %1	" : /* no outputs */ : "r" ((short)(var1)), "r" ((short)(var2)):"$11"); \
	ASM ("sra	%0, $11, 16	" : "=r" ((short)__result) : /* no inputs */ : "$11"); \
	ASM (".set	reorder");	\
	__result; \
})

#define L_mult_inline(var1, var2)	\
({	\
	long __result;	\
	ASM ("muleq_s.w.phr	%0, %1, %2	" : "=r" ((long)__result) : "r" ((long)(var1)), "r" ((long)(var2))); \
	__result; \
})

#define negate_inline(var1)	\
({	\
	short __result;	\
	ASM (".set	noreorder");	\
	ASM ("move		$14, %0" : /* no outputs */ : "r" ((short)(var1)) :"$14");	\
	ASM ("li		$15, 0x00008000" : /* no outputs */ : /* no inputs */ :"$15"); \
	ASM ("bne		$14, $15, 1f" : /* no outputs */ : /* no inputs */:"$14");	\
	ASM ("nop");	\
	ASM ("li		%0, 0x00007fff" : "=d" ((short)__result) : /* no inputs */);	\
	ASM ("b			2f");	\
	ASM ("nop");	\
	ASM ("1:");	\
	ASM ("subq_s.ph	$15, $0, $14" : /* no outputs */ : /* no inputs */:"$15");	\
	ASM ("sll		$15, $15, 16":/* no outputs */:/* no inputs */:"$15");	\
	ASM ("sra		%0, $15, 16\n\t2:" : "=d" ((short)__result) : /* no inputs */:"$15");	\
	ASM (".set	reorder");	\
	__result; \
})

#define round_inline(L_var1)	\
({	\
	short __result;	\
	ASM ("li		$15, 0x00008000" : /* no outputs */ : /* no inputs */:"$15");	\
	ASM ("addq_s.w	$15, %0, $15" : /* no outputs */ : "d" ((long)(L_var1)):"$15");	\
	ASM ("sra		%0, $15, 16" : "=d" ((short)__result) : /* no inputs */:"$15");	\
	__result; \
})

#define L_mac_inline(L_var3, var1, var2)	\
({	\
	long __result;	\
	ASM ("muleq_s.w.phr	$15, %0, %1" : /* no outputs */ : "d" ((short)(var1)), "d" ((short)(var2)):"$15");	\
	ASM ("addq_s.w	%0, %1, $15" : "=d" ((long)__result) : "d" ((long)(L_var3)):"$15");	\
	__result; \
})

#define L_msu_inline(L_var3, var1, var2)	\
({	\
	long __result;	\
	ASM ("muleq_s.w.phr	$15, %0, %1" : /* no outputs */ : "d" ((short)(var1)), "d" ((short)(var2)):"$15");	\
	ASM ("subq_s.w	%0, %1, $15" : "=d" ((long)__result) : "d" ((long)(L_var3)):"$15");	\
	__result; \
})

#define L_add_inline(var1, var2)	\
({	\
	long __result;	\
	ASM ("addq_s.w	%0, %1, %2" : "=d" ((long)__result) : "d" ((long)(var1)), "d" ((long)(var2)));	\
	__result; \
})

#define L_sub_inline(var1, var2)	\
({	\
	long __result;	\
	ASM ("subq_s.w	%0, %1, %2" : "=d" ((long)__result) : "d" ((long)(var1)), "d" ((long)(var2)));	\
	__result; \
})

#define L_negate_inline(L_var1)	\
({	\
	long __result;	\
	ASM ("move		$14, %0" : /* no outputs */ : "r" ((long)(L_var1)) :"$14");	\
	ASM ("li		$15, 0x80000000" :/* no outputs */: /* no inputs */ :"$15");	\
	ASM ("bne		$14, $15, 1f" : /* no outputs */ : /* no inputs */ :"$14");	\
	ASM ("nop");	\
	ASM ("li		%0, 0x7fffffff" :"=d" ((long)__result):/* no inputs */);	\
	ASM ("b			2f");	\
	ASM ("nop\t\n1:");	\
	ASM ("subq_s.w	%0, $0, $14 \t\n2:" : "=d" ((long)__result) : /* no inputs */);	\
	__result; \
})

#define mult_r_inline(var1, var2)	\
({	\
	short __result;	\
	ASM ("mulq_rs.ph	%0, %1, %2	" : "=d" ((short)__result) : "d" ((short)(var1)), "d" ((short)(var2))); \
	__result; \
})



#define L_shl_inline(L_var1, var2)	\
({	\
	long __result;	\
	ASM ("move		$9, %0": /* no outputs */ : "d" ((long)(L_var1)): "$9");\
	ASM ("move		$8, %0": /* no outputs */ : "d" ((short)(var2)): "$8");\
	ASM ("absq_s.w		$10, $8" 	: /* no outputs */ :  "d" ((short)(var2)) :"$10");	\
	ASM ("slti			$11, $10, 31" : /* no outputs */ : /* no inputs */ : "$11");	\
	ASM ("bnez			$11, 1f" 	: /* no outputs */ : /* no inputs */ : "$11");	\
	ASM ("li			$12,  0" 	: /* no outputs */ : /* no inputs */ : "$12");	\
	ASM ("li			$11,  -1" 	: /* no outputs */ : /* no inputs */);	\
	ASM ("slti			$12, $8, 0": /* no outputs */ : /* no inputs */);	\
	ASM ("movn			%0, $11, $12" : "=d" ((long)__result) : /* no inputs */);	\
	ASM ("j				4f" 		: /* no outputs */ : /* no inputs */);	\
	ASM ("nop" 						: /* no outputs */ : /* no inputs */);	\
	ASM ("1:bgtz		$10,  2f" 	: /* no outputs */ : /* no inputs */ : "$10");	\
	ASM ("nop" 						: /* no outputs */ : /* no inputs */);	\
	ASM ("sra			%0, %1, $10" : "=d" ((long)__result) : "d" ((long)(L_var1)) : "$10");	\
	ASM ("j				4f" 		: /* no outputs */ : /* no inputs */);	\
	ASM ("nop" 						: /* no outputs */ : /* no inputs */);	\
	ASM ("2:clz			$11, $9" : /* no outputs */ : /* no inputs */ : "$11");	\
	ASM ("clo			$15, $9" : /* no outputs */ : /* no inputs */ : "$8");	\
	ASM ("slti			$16, $9, 0" : /* no outputs */ :  /* no inputs */ : "$16");	\
	ASM ("movn			$11, $15, $16" : /* no outputs */ : /* no inputs */ : "$11");	\
	ASM ("sub			$12, $11, $10" : /* no outputs */ : /* no inputs */ : "$12");	\
	ASM ("bgtz			$12, 3f" : /* no outputs */ : /* no inputs */ : "$12");	\
	ASM ("li		 	$13, 0x80000000" : /* no outputs */ :  /* no inputs */ : "$13");	\
	ASM ("li			$14, 0x7FFFFFFF" : /* no outputs */ :  /* no inputs */ : "$14");	\
	ASM ("slti			$11, $9, 0" : /* no outputs */ : /* no inputs */ : "$11");	\
	ASM ("movn			$16, $13, $11" : /* no outputs */ : /* no inputs */ : "$16");	\
	ASM ("slt			$11, $0, $9" : /* no outputs */ : /* no inputs */ : "$11");	\
	ASM ("movn			$16, $14, $11" : /* no outputs */ : /* no inputs */ : "$16");	\
	ASM ("move			%0, $16" : "=d" ((long)__result) : /* no inputs */);	\
	ASM ("j				4f" : /* no outputs */ : /* no inputs */);	\
	ASM ("nop" 						: /* no outputs */ : /* no inputs */);	\
	ASM ("3:sllv			%0,  $9, $10\t\n4:" : "=d" ((long)__result) : /* no inputs */);	\
	ASM ("nop" 						: /* no outputs */ : /* no inputs */);	\
	__result; \
})


#define L_shr_inline(L_var1, var2)	\
({	\
	long __result;	\
	ASM ("li		$15, 31" : /* no outputs */ : /* no inputs */:"$15");	\
	ASM ("slt		$14, %0, $15" : /* no outputs */  :  "d" ((short)(var2)) : "$14");	\
	ASM ("movn		$15, %0, $14" : /* no outputs */  :  "d" ((short)(var2)) : "$15");	\
	ASM ("srav		%0,  %1, $15" : "=d" ((long)__result) :  "d" ((long)(L_var1)) : "$15");	\
	__result; \
})

#define L_abs_inline(L_var1)	\
({	\
	long __result;	\
	ASM ("absq_s.w	%0, %1" : "=d" ((long)__result) :  "d" ((long)(L_var1)));	\
	__result; \
})

#endif /* _BASIC_OP_INLINE__ */
