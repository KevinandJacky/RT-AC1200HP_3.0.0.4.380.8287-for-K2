#include "../mips.h"

#ifdef RALINK_VOIP_CVERSION
#else
/*
void L_Extract(Word32 L_32, Word16 *hi, Word16 *lo)
	NOTE.
	can't use t0~t9
*/
	.text
	.section ".oper_32bs" , "ax"
	.set	noreorder
	.ent	L_Extract
	.globl	L_Extract
L_Extract:
	.align	3
	sra		v0, a0, 16	;sra	v1, a0, 1//v0=t4 v1=t6
	sh		v0, 0(a1)	;sll	a3, v0, 14+1	//a3=t5
	subq_s.w	v1, v1, a3
	jr		ra
	sh		v1, 0(a2)
	/*sra		t4, a0, 16	;sra	t6, a0, 1
	sh		t4, 0(a1)	;sll	t5, t4, 14+1 
	subq_s.w	t6, t6, t5
	jr		ra
	sh		t6, 0(a2)*/
	.set	reorder
	.end	L_Extract
	
/*
Word32 L_Comp(Word16 hi, Word16 lo)
*/
	.text
	.section ".oper_32bs" , "ax"
	.set	noreorder
	.ent	L_Comp
	.globl	L_Comp
	.align	3
L_Comp:
	sll		a0, a0, 16
	sll		a1, a1, 1
	jr		ra
	addq_s.w	v0, a0, a1	
	.set	reorder
	.end	L_Comp
	
/*
Word32 Mpy_32(Word16 hi1, Word16 lo1, Word16 hi2, Word16 lo2)
	NOTE.
	can't use t0~t9
*/
	.text
	.section ".oper_32bs" , "ax"
	.set	noreorder
	.ent	Mpy_32
	.globl	Mpy_32
	.align	3
Mpy_32:
	muleq_s.w.phr  	v0, a0, a2 								
	muleq_s.w.phr  	a0, a0, a3
	muleq_s.w.phr  	a1, a1, a2								
	sra				a0, a0, 16
	sll				a0, a0, 1
	sra				a1, a1, 16
	sll				a1, a1, 1
	addq_s.w		v0, a0, v0
	addq_s.w		v0, a1, v0
	jr				ra
	nop
	.set	reorder	
	.end	Mpy_32
	
/*
Word32 Mpy_32_16(Word16 hi, Word16 lo, Word16 n)
NOTE.
	can't use t0~t9, m2, m3
*/
	.text
	.section ".oper_32bs" , "ax"
	.set	noreorder
	.ent	Mpy_32_16
	.globl	Mpy_32_16
	.align	3
Mpy_32_16:
	muleq_s.w.phr  	v0, a0, a2 	
	muleq_s.w.phr	a1, a1, a2	
	sra				a1, a1, 16
	sll				a1, a1, 1
	jr		ra
	addq_s.w		v0, a1, v0
	.set	reorder
	.end	Mpy_32_16		
#endif	