#include "../mips.h"

#define MAX_32	0x7fffffff
#define MIN_32	0x80000000
#define MAX_16	0x00007fff
#define MIN_16	0x00008000

#if 0
/*
Word16 shl(Word16 var1, var2)
assume: var2 >= 0
*/
	.text
	#.section ".basic_ops" , "ax"
	.set	noat
	.set 	noreorder
	.globl 	shl
	.ent	shl
shl:
	move	t0, a1
	move	t1, a0
	
	clz		t2, t1			//t1>=0
	clo		t3, t1			//t1<0
	
	slt		t4, t1, zero	//if t1<0, t4=1
	movn	t2, t3, t4
	
	li		t4, 16
	sub		t2, t2, t4
	sllv	t3, t1, t0
	sub		t2, t2, t0		//t2=leading-shift
	blez	t2, shl_bltz
	nop

	slt		t5, t0, t4		//t0<16, t5=1
	movn	t4,  zero, t1	//if t1!=0, t4=0

	or		t5, t5, t4
	beqz	t5, shl_bltz
	nop

	xor		t5, t1, t3//xor		t5, a0, t3
	bgez	t5, shl_exit
	nop

shl_bltz:
	bltz	t1, shl_min
	nop
shl_max:
	li		t3, 0x00007fff
	j shl_exit
	nop
shl_min:
	li		t3, 0x00008000
shl_exit:
	move	v0, t3
	jr		ra
	nop
	.end	shl
#endif
#ifdef RALINK_VOIP_CVERSION 
#else
/*
Word32 L_shl(Word32 L_var1, var2)
assume: var2 >= 0
*/
	.text
	#.section ".basic_ops" , "ax"
	.set	noat
	.set 	noreorder
	.globl 	L_shl
	.ent	L_shl
L_shl:
	absq_s.w	t0, a1
	slti	t1, t0, 31		#
	bgtz	t1, 1f
	li		v0, 0
	li		t1, -1
	slti	t2, a1, 0
	jr		ra
	movn	v0, t1, t2		# out=max
1:
	bgtz	a1, 2f
	nop
	jr		ra
	sra		v0, a0, t0
2:
	clz		t1, a0
	clo		t5, a0
	slt		t6, a0, zero
	movn	t1, t5, t6
	sub		t2, t1, a1
	bgtz	t2, 3f
	li		t3, MIN_32
	li		t4, MAX_32
	slti	t1, a0, 0
	movn	v0, t3, t1
	slt		t1, $0, a0
	movn	v0, t4, t1
	jr		ra
	nop
3:
	jr		ra
	sllv	v0, a0, a1
	.end	L_shl

#endif
