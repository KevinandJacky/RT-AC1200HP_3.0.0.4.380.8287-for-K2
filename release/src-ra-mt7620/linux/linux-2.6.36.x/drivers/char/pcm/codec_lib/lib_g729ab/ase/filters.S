#include "../mips.h"

#define	M		10
#define L_SUBFR	40
#define L_FRAME	80
#define PIT_MAX	143

#ifdef RALINK_VOIP_CVERSION
#else	
	.text	
	.section ".filters" , "ax"
	.set	noreorder
	.globl	Syn_filt
	.ent	Syn_filt
	.align	3
Syn_filt:
	lw		t0, 16(sp)
	lw		t1, 20(sp)
	subu	sp, sp, 128
	sw		ra, 20(sp)
	sw		s0, 24(sp)
	sw		s1, 28(sp)
	sw		s2, 32(sp)	;move	s0, a0//s0=a[]
	sw		s3, 36(sp)	;move	s1, a1//s1=x[]
	sw		s4, 40(sp)	;move	s2, a2//s2=y[]
	sw		s5, 44(sp)	;move	s3, a3//s3=lg
	sw		s6, 48(sp)	;move	s4, t0//s4=mem[]
	sw		s7, 52(sp)	;move	s5, t1//s5=update
	sw		s5, 56(sp)
	sw		s2, 60(sp)
	sw		s3, 64(sp)	
	/////////////////
	/* Copy mem[] to yy[] */
	.extern TmpVct
	la		a0, TmpVct	
	addu	t8, a0, 2*L_SUBFR	//t8=yy->tmp[L_SUBFR]
	addu	t9, s4, 2*(M-1)		//t9->mem[9]
	move	v0, t8				//v0=yy
	
	lh		t2, 0(t9)
	lh		t3, -2(t9)
	sll		t3, t3, 16
	packrl.ph	t2, t2, t3
	lh		t3, -4(t9)
	lh		t4, -6(t9)
	sll		t4, t4, 16
	packrl.ph	t3, t3, t4
	
	nop//t3:[m6 m7]
	lh		t4, -8(t9)
	lh		t5, -10(t9)
	sll		t5, t5, 16
	packrl.ph	t4, t4, t5
	packrl.ph	t2, t2, t2
	lh		t5, -12(t9)
	lh		t6, -14(t9)
	sll		t6, t6, 16
	packrl.ph	t5, t5, t6
	packrl.ph	t3, t3, t3
	lh		t6, -16(t9)
	lh		t7, -18(t9)
	sll		t7, t7, 16
	packrl.ph	t6, t6, t7
	packrl.ph	t4, t4, t4
	sw		t2, 0(t8)	
	packrl.ph	t5, t5, t5
	sw		t3, 4(t8)
	sw		t4, 8(t8)	
	packrl.ph	t6, t6, t6
	sw		t5, 12(t8)
	sw		t6, 16(t8)
	
	
	/* Do the filtering. */
	move	a0, s0
	addiu	a0, a0, 2
	lh		t0, 0(a0)
	lh		t1, 2(a0)
	sll		t1, t1, 16
	packrl.ph	t0, t0, t1
	lh		t1, 4(a0)
	lh		t2, 6(a0)
	sll		t2, t2, 16
	packrl.ph	t1, t1, t2
	lh		t2, 8(a0)
	lh		t3, 10(a0)
	sll		t3, t3, 16
	packrl.ph	t2, t2, t3
	lh		t3, 12(a0)
	lh		t4, 14(a0)
	sll		t4, t4, 16
	packrl.ph	t3, t3, t4
	lh		t4, 16(a0)
	lh		t5, 18(a0)
	sll		t5, t5, 16
	packrl.ph	t4, t4, t5
	
	move	a1, t8		//a1=yy
	lh		t5, 0(a1)
	lh		t6, 2(a1)
	sll		t6, t6, 16
	packrl.ph	t5, t5, t6
	lh		t6, 4(a1)
	lh		t7, 6(a1)
	sll		t7, t7, 16
	packrl.ph	t6, t6, t7
	lh		t7, 8(a1)
	lh		t8, 10(a1)
	sll		t8, t8, 16
	packrl.ph	t7, t7, t8
	lh		t8, 12(a1)
	lh		t9, 14(a1)
	sll		t9, t9, 16
	packrl.ph	t8, t8, t9
	lh		t9, 16(a1)
	lh		v0, 18(a1)
	sll		v0, v0, 16
	packrl.ph	t9, t9, v0
	
	move	s6, a1
	move	s7, s2
	
	packrl.ph	a0, t0, t1
	packrl.ph	a1, t1, t2
	packrl.ph	a2, t2, t3
	packrl.ph	a3, t3, t4
	packrl.ph	v0, t4, zero
	
	subu	s3, s3, 2
	li		v1, 0x1000
1:
	lh		s0, 0(s1)
	lh		s5, 2(s1)
	addiu	s1, s1, 4
	muleq_s.w.phr	s0, s0, v1
	muleq_s.w.phr	s5, s5, v1
	mtlo	s0, $ac0
	mtlo	s5, $ac2
	
	dpsq_s.w.ph	$ac0, t0, t5
	dpsq_s.w.ph	$ac0, t1, t6
	dpsq_s.w.ph	$ac0, t2, t7
	dpsq_s.w.ph	$ac0, t3, t8
	dpsq_s.w.ph	$ac0, t4, t9
	
	dpsq_s.w.ph	$ac2, a0, t5
	dpsq_s.w.ph	$ac2, a1, t6
	dpsq_s.w.ph	$ac2, a2, t7
	dpsq_s.w.ph	$ac2, a3, t8
	dpsq_s.w.ph	$ac2, v0, t9
	
	mflo	s0, $ac0 
	addq_s.w	s0, s0, s0
	addq_s.w	s0, s0, s0
	addq_s.w	s0, s0, s0
	sll		s2, v1, 3
	addq_s.w	s0, s0, s2
	sra		s0, s0, 16
	sh		s0, -2(s6)
	
	mflo	s5, $ac2
	sll		s2, s0, 16
	muleq_s.w.phl	s2, t0, s2
	subq_s.w	s5, s5, s2
	
	addq_s.w	s5, s5, s5
	addq_s.w	s5, s5, s5
	addq_s.w	s5, s5, s5
	
	sll		s2, v1, 3
	addq_s.w	s5, s5, s2
	sra		s5, s5, 16
	sh		s5, -4(s6)
	
	sh		s0, 0(s7)
	sh		s5, 2(s7)
	addiu	s7, s7, 4
	
	move	t9, t8
	move	t8, t7
	move	t7, t6
	move	t6, t5
	sll		s0, s0, 16
	packrl.ph	t5, s5, s0
	subu	s6, s6, 4
2:	
	bgtz	s3, 1b
	subu	s3, s3, 2
	
	/* Update of memory if update==1 */
	lw		s5, 56(sp)
	beqz	s5, 3f
	nop
	lw		s2, 60(sp)
	lw		s3, 64(sp)
	//subu	s2, s2, 2*M		//s2->y[lg-M]
	subu	s3, s3, M
	sll		s3, s3, 1
	addu	s2, s2, s3
	lh			t0, 0(s2)
	lh			t1, 2(s2)
	lh			t2, 4(s2)
	lh			t3, 6(s2)
	lh			t4, 8(s2)
	lh			t5, 10(s2)
	lh			t6, 12(s2)
	lh			t7, 14(s2)
	lh			t8, 16(s2)
	lh			t9, 18(s2)
	
	sh			t0, 0(s4)
	sh			t1, 2(s4)
	sh			t2, 4(s4)
	sh			t3, 6(s4)
	sh			t4, 8(s4)
	sh			t5, 10(s4)
	sh			t6, 12(s4)
	sh			t7, 14(s4)
	sh			t8, 16(s4)
	sh			t9, 18(s4)
	
3:
	/////////////////
	lw		ra, 20(sp)
	lw		s0, 24(sp)
	lw		s1, 28(sp)
	lw		s2, 32(sp)
	lw		s3, 36(sp)
	lw		s4, 40(sp)
	lw		s5, 44(sp)
	lw		s6, 48(sp)
	lw		s7, 52(sp)
	jr		ra
	addu	sp, sp, 128
	.set	reorder
	.end	Syn_filt
#endif				