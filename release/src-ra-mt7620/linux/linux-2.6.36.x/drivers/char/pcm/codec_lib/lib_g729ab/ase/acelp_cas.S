#include "../mips.h"

#define L_SUBFR	40
#define NB_POS	8
#define MSIZE	64
#define STEP	5
#define MAX_16	0x00007fff
#define MIN_16	0xffff8000

#define _1_2    16384
#define _1_4     8192
#define _1_8     4096
#define _1_16    2048

#define i0i0	(0)
#define i1i1	(NB_POS)*2
#define i2i2	(NB_POS*2)*2
#define i3i3	(NB_POS*3)*2
#define i4i4	(NB_POS*4)*2
#define i0i1	(NB_POS*5)*2
#define i0i2	(NB_POS*5+MSIZE)*2
#define i0i3	(NB_POS*5+MSIZE*2)*2
#define i0i4	(NB_POS*5+MSIZE*3)*2
#define i1i2	(NB_POS*5+MSIZE*4)*2
#define i1i3	(NB_POS*5+MSIZE*5)*2
#define i1i4	(NB_POS*5+MSIZE*6)*2
#define i2i3	(NB_POS*5+MSIZE*7)*2
#define i2i4	(NB_POS*5+MSIZE*8)*2

/*
void Cor_h(Word16 *H, Word16 *rr)
	s0->H[]		align 8
	s1->rr[]	align 4
local:
	h->TmpVct[0]	//s2
	
*/
	.text
	//.section ".Cor_hs" , "ax"
	.set	noreorder
	.globl	Cor_hs
	.ent	Cor_hs
Cor_hs:
	subu	sp, sp, 128
	sw		ra, 20(sp)
	sw		s0, 24(sp)
	sw		s1, 28(sp)
	sw		s2, 32(sp)	;move	s0, a0//s0=H[]
	sw		s3, 36(sp)	;move	s1, a1//s1=rr[]
	sw		s4, 40(sp)
	sw		s5, 44(sp)
	sw		s6, 48(sp)
	sw		s7, 52(sp)
	/////////////////
	/* scaling h[] for maximum precision */
	mthi	zero, $ac0
	mtlo	zero, $ac0
	lw		t4, 0(a0)
	lw		t5, 4(a0)
	addu	a0, a0, 8
	li		t2, L_SUBFR/8-1
1:
	dpaq_s.w.ph $ac0, t4, t4;	 lw		t6, 0(a0)
	dpaq_s.w.ph $ac0, t5, t5;	 lw		t7, 4(a0)
	dpaq_s.w.ph $ac0, t6, t6;    lw		t4, 8(a0)
	dpaq_s.w.ph $ac0, t7, t7;	 lw		t5, 12(a0)
	addu		a0, a0, 16
	
2:
	bgtz	t2, 1b
	addi	t2, t2, -1	
	
	mfhi	t0, $ac0
	mflo	t1, $ac0
	addq_s.w v0, t0, t1	//v0=cor

	li		t4, 32000
	sra		t0, v0, 16
	sub		t0, t0, t4	
	.extern TmpVct
	la		s2, TmpVct	//s2=h->TmpVct
	
	blez	t0, 3f
	move	a0, s0	//a0->H[]
	//if(cor > 32000)
	lw		t4, 0(a0)
	lw		t5, 4(a0)
	addu	a0, a0, 8
	li		t6, 1	
	li		t8, L_SUBFR/8-1
	move	a2, s2	//a2->h[]
1:
	shrav.ph	t4, t4, t6		;lw		t0, 0(a0)
	lw			t1, 4(a0)		;lw		t2, 8(a0)
	shrav.ph	t5, t5, t6		;lw		t3, 12(a0)
	addu		a0, a0, 16		;sw		t4, 0(a2)
	sw			t5, 4(a2)		;shrav.ph	t1, t1, t6
	move		t4, t2			;sw		t0, 8(a2)
	sw			t1, 12(a2)		;move	t5, t3
	addu		a2, a2, 16
2:
	bgtz		t8, 1b
	addi		t8, t8, -1
	j			5f
	nop
3:	//else	
	//cls		a0, v0
	//cmveqz	a0, zero, v0
	clz		a0, v0
	clo		t0, v0
	addi	a0, a0, -1
	addi	t0, t0, -1
	slt		t1, v0, zero
	movn	a0, t0, t1
	movz	a0, zero, v0
	move	v0, a0
	
	sra		v0, v0, 1	//v0=k
	bltz	v0, 4f
	move	a0, s0		//a0->H[]

	lw		t4, 0(a0)
	lw		t5, 4(a0)
	addu	a0, a0, 8
	li		t8, L_SUBFR/8-1
	move	a2, s2	//a2->h[]
1:
	shllv.ph	t4, t4, v0	;lw			t0, 0(a0)
	shllv.ph	t5, t5, v0	;lw			t1, 4(a0)	
	shllv.ph	t0, t0, v0	;lw			t2,	8(a0)
	shllv.ph	t1, t1, v0	;lw			t3, 12(a0)
	addu		a0, a0, 16  ;sw			t4, 0(a2)
	move		t4, t2		;sw			t5, 4(a2)
	move		t5, t3		;sw			t0, 8(a2)
							;sw			t1, 12(a2)
	addu		a2, a2, 16	;
2:
	bgtz	t8, 1b
	addi	t8, t8, -1
	j		5f
	nop
4:	//else
	lw		t4, 0(a0)
	lw		t5, 4(a0)
	li		t6, 1
	li		t8, L_SUBFR/8-1
	sub		v0, zero, v0//v0=-k
	move	a2, s2	//a2->h[]
1:
	shrav.ph	t4, t4, v0	;lw		t0, 0(a0)
	shrav.ph	t5, t5, v0	;lw		t1, 4(a0)
	shrav.ph	t0, t0, v0	;lw		t2, 8(a0)
	shrav.ph	t1, t1, v0	;lw		t3,	12(a0)
	addu		a0 ,a0, 16	;sw		t4, 0(a2)
	move		t4, t2		;sw		t5, 4(a2)
	move		t5, t3		;sw		t0, 8(a2)
							;sw		t1, 12(a2)
	addu		a2, a2, 16	;
2:	
	bgtz	t8, 1b
	addi	t8, t8, -1

5: /*
 	(1) compute rri0i0...
	*/	
	/* Init pinters */
	/* Init pointer to last position of rrixix[] */
	addu	t0, s1, (NB_POS-1)*2	//t0=p0
	addu	t1, s1, (NB_POS*2-1)*2	//t1=p1
	addu	t2, s1, (NB_POS*3-1)*2	//t2=p2
	addu	t3, s1, (NB_POS*4-1)*2	//t3=p3
	addu	t4, s1, (NB_POS*5-1)*2	//t4=p4
	move	a0, s2		//a0->ptr_h1
	mult	zero,zero
	lh		t8, 0(a0)
	lh		t9,	2(a0)
	addu	a0, a0, 4
	li		a2, NB_POS-1
1:
	dpaq_s.w.ph	$ac0, t8, t8	;	lh		t6, 0(a0)
	mflo	v0, $ac0			;	lh		t7, 2(a0)
	sra		v0, v0, 16			;	dpaq_s.w.ph	$ac0, t9, t9
	sh		v0, 0(t4)			;	move	t8, t6
	mflo	v1, $ac0			;	move	t9, t7
	sra		v1, v1, 16			;	lh		t6, 4(a0)
	sh		v1, 0(t3)			;	dpaq_s.w.ph	$ac0, t8, t8
	move	t8, t6				;	lh		t7, 6(a0)
	mflo	v0, $ac0			;	nop
	sra		v0, v0, 16			;	dpaq_s.w.ph	$ac0, t9, t9
	sh		v0, 0(t2)			;	move	t9, t7
	mflo	v1, $ac0			;	nop
	sra		v1, v1, 16			;	lh		t6, 8(a0)
	sh		v1, 0(t1)			;	dpaq_s.w.ph	$ac0, t8, t8
	mflo	v0, $ac0			;	move	t8, t9
	sra		v0, v0, 16			;	move	t9, t6
	sh		v0, 0(t0)			;	addu	a0, a0, 10	
	addi	t4, t4, -2
	addi	t3, t3, -2
	addi	t2, t2, -2
	addi	t1, t1, -2
	addi	t0, t0, -2
2:	
	bgtz	a2, 1b
	addi	a2, a2, -1
	
	/*
	(2) compute rri2i3 ...
	*/
	li		s3, 0	//s3=k
	move	s6, s2	//s6=ptr_hd	
	li		s4, (MSIZE-1)*2	//s4=l_fin_sup
	li		s5, (MSIZE-2)*2	//s5=l_fin_inf	
	addu	s7, s2, 2		//s7=ptr_hf	
3:	//for(k=0;k<NB_POS;k++) {
	move	a0, s6	//a0->ptr_h1	
	move	a1, s7	//a1->ptr_h2			
	lh		t4, 0(a0)	
	addu	t3, s1, (NB_POS*5+MSIZE*7)*2
	lh		t5, 0(a1)
	addu	t3, t3, s4	//t3=p3
	addu	t2, s1, i1i2	;li		t9, NB_POS
	addu	t2, t2, s4		;add	t8, s3, 1//t2=p2
	addu	t1, s1, i0i1	;sub	t9, t9, t8
	addu	t1, t1, s4		;sub	t9, t9, 1//t1=p1
	addu	t0, s1, i0i4	;move	a3, t9
	addu	t0, t0, s5	//t0=p0	
	lh		t6, 2(a0)
	addu	a0, a0, 4
	mult	zero,zero	
	lh		t7, 2(a1)
	addu	a1, a1, 4
	bltz	t9, 2f	//check if k==7, then bypass loop
	nop
	//h1 t4:[3] t6:[4] t8:[2]
	//h2 t5:[3] t7:[4] t9:[2]
	.align	3
1:	
	dpaq_s.w.ph	$ac0, t4, t5		;	lh		t8, 0(a0)
	dpaq_s.w.ph	$ac0, t6, t7		;	lh		t9, 0(a1)
	mflo	v0, $ac0					;	nop
	sra		v0, v0, 16				;	lh		t4, 2(a0)
	sh		v0, 0(t3)				;	dpaq_s.w.ph	$ac0, t8, t9
	mflo		v1, $ac0					;	nop
	sra		v1, v1, 16				;	lh		t5, 2(a1)
	sh		v1, 0(t2)				;	nop
	dpaq_s.w.ph	$ac0, t4, t5		;	lh		t6, 4(a0)
	mflo		v0, $ac0					;	nop
	sra		v0, v0, 16				;	lh		t7, 4(a1)
	sh		v0, 0(t1)				;	nop
	dpaq_s.w.ph	$ac0, t6, t7		;	lh		t4, 6(a0)
	mflo		v1, $ac0					;	nop
	sra		v1, v1, 16				;	lh		t5, 6(a1)
	sh		v1, 0(t0)				;	nop
	lh		t6, 8(a0)				;	addu	a0, a0, 10
	lh		t7, 8(a1)				;	addu	a1, a1, 10
	addi	t3, t3, -((NB_POS+1)*2)
	addi	t2, t2, -((NB_POS+1)*2)
	addi	t1, t1, -((NB_POS+1)*2)
	addi	t0, t0, -((NB_POS+1)*2)
2:
	bgtz	a3,	1b
	addi	a3, a3, -1
	dpaq_s.w.ph	$ac0, t4, t5	;lh		t8, 0(a0)
	dpaq_s.w.ph	$ac0, t6, t7	;lh		t9, 0(a1)
	lh		t4, 2(a0)
	lh		t5, 2(a1)
	addu	a0, a0, 4
	addu	a1, a1, 4
	mflo	v0, $ac0		;nop
	sra		v0, v0, 16		;dpaq_s.w.ph	$ac0, t8, t9
	sh		v0, 0(t3)		;nop
	mflo	v1, $ac0		;nop
	sra		v1, v1, 16		;dpaq_s.w.ph	$ac0, t4, t5
	sh		v1, 0(t2)		;subu	s4, s4, (NB_POS*2)//l_fin_sup -=NB_POS
	mflo	v0, $ac0		;subu	s5, s5, 2	//l_fin_inf --
	sra		v0, v0, 16		;addu	s7, s7, (STEP*2)//ptr_hf += STEP
	sh		v0, 0(t1)		;sub	t0, s3, NB_POS-1
	// }	
	bltz	t0, 3b
	addu	s3, s3, 1
	
	/*
	(3) compute rri2i4 ...
	*/
	li		s3, 0	//s3=k
	move	s6, s2	//s6=ptr_hd=h
	li		s4, (MSIZE-1)*2	//s4=l_fin_sup
	li		s5, (MSIZE-2)*2	//s5=l_fin_inf	
	addu	s7, s2, 4		//s7=ptr_hf	
	.align	3
3:	//for(k=0;k<NB_POS;k++) {
	move	a0, s6	//a0->ptr_h1	
	move	a1, s7	//a1->ptr_h2			
	lh		t4, 0(a0)	
	addu	a2, s1, i2i4
	addu	a2, a2, s4	//a2=p4
	addu	t3, s1, i1i3
	lh		t5, 0(a1)
	addu	t3, t3, s4	//t3=p3
	addu	t2, s1, i0i2	;li		t9, NB_POS
	addu	t2, t2, s4		;add	t8, s3, 1//t2=p2
	addu	t1, s1, i1i4	;sub	t9, t9, t8
	addu	t1, t1, s5		;sub	t9, t9, 1//t1=p1
	addu	t0, s1, i0i3	;move	a3, t9
	addu	t0, t0, s5	//t0=p0	
	lh		t6, 2(a0)
	mult	zero,zero	
	lh		t7, 2(a1)
	addu	a0, a0, 4
	addu	a1, a1, 4
	bltz	t9, 2f	//check if k==7, then bypass loop
	nop
	//h1 t4:[3] t6:[4] t8:[2]
	//h2 t5:[3] t7:[4] t9:[2]
	.align	3
1:	
	dpaq_s.w.ph	$ac0, t4, t5		;lh	t8, 0(a0)
	mflo	v1, $ac0				;nop
	sra			v1, v1, 16			;lh	t9, 0(a1)	
	sh		v1, 0(a2)				;dpaq_s.w.ph	$ac0, t6, t7
	mflo	v0, $ac0				;nop
	sra		v0, v0, 16				;lh	t4, 2(a0)
	sh		v0, 0(t3)				;dpaq_s.w.ph	$ac0, t8, t9
	mflo	v1, $ac0				;nop
	sra		v1, v1, 16				;lh	t5, 2(a1)
	sh		v1, 0(t2)				;nop
	dpaq_s.w.ph	$ac0, t4, t5		;lh	t6, 4(a0)
	mflo	v0, $ac0				;nop
	sra		v0, v0, 16				;lh	t7, 4(a1)
	sh		v0, 0(t1)				;nop
	dpaq_s.w.ph	$ac0, t6, t7		;lh	t4, 6(a0)
	mflo	v1, $ac0				;nop
	sra		v1, v1, 16				;lh	t5, 6(a1)
	sh		v1, 0(t0)				;nop
	lh		t6, 8(a0)				;addu	a0, a0, 10
	lh		t7, 8(a1)				;addu	a1, a1, 10
	addi	a2,	a2, -((NB_POS+1)*2)
	addi	t3,	t3, -((NB_POS+1)*2)
	addi	t2,	t2, -((NB_POS+1)*2)
	addi	t1, t1, -((NB_POS+1)*2)
	addi	t0, t0, -((NB_POS+1)*2)
2:
	bgtz	a3, 1b
	addi	a3, a3, -1
	
	dpaq_s.w.ph	$ac0, t4, t5		;lh		t8, 0(a0)
	mflo		v1, $ac0			;addu	a0, a0, 2
	sra		v1, v1, 16				;lh		t9, 0(a1)	
	sh		v1, 0(a2)				;dpaq_s.w.ph	$ac0, t6, t7
	mflo		v0, $ac0			;addi	a1, a1, 2
	sra		v0, v0, 16				;dpaq_s.w.ph	$ac0, t8, t9
	sh		v0, 0(t3)				;subu	s4, s4, (NB_POS*2)//l_fin_sup -=NB_POS
	mflo		v1, $ac0			;subu	s5, s5, 2	//l_fin_inf --
	sra		v1, v1, 16				;addu	s7, s7, (STEP*2)//ptr_hf += STEP
	sh		v1, 0(t2)				;sub	t0, s3, NB_POS-1
	// }	
	bltz	t0, 3b
	addi	s3, s3, 1

	/*
	(4) compute rri1i4 ...
	*/
	li		s3, 0	//s3=k
	move	s6, s2	//s6=ptr_hd=h
	li		s4, (MSIZE-1)*2	//s4=l_fin_sup
	li		s5, (MSIZE-2)*2	//s5=l_fin_inf	
	addu	s7, s2, 6		//s7=ptr_hf	
	.align	3
3:	//for(k=0;k<NB_POS;k++) {
	move	a0, s6	//a0->ptr_h1	
	move	a1, s7	//a1->ptr_h2			
	lh		t4, 0(a0)	
	addu	a2, s1, i1i4
	addu	a2, a2, s4	//a2=p4
	addu	t3, s1, i0i3
	lh		t5, 0(a1)
	addu	t3, t3, s4	//t3=p3
	addu	t2, s1, i2i4	;li		t9, NB_POS
	addu	t2, t2, s5		;add	t8, s3, 1//t2=p2
	addu	t1, s1, i1i3	;sub	t9, t9, t8
	addu	t1, t1, s5		;sub	t9, t9, 1//t1=p1
	addu	t0, s1, i0i2	;move	a3, t9
	addu	t0, t0, s5	//t0=p0	
	lh		t6, 2(a0)
	mult	zero,zero		;addi	a0, a0, 4
	lh		t7, 2(a1)		;addi	a1, a1, 4
	
	
	bltz	t9, 2f	//check if k==7, then bypass loop
	nop
	//h1 t4:[3] t6:[4] t8:[2]
	//h2 t5:[3] t7:[4] t9:[2]
	.align	3
1:	
	dpaq_s.w.ph	$ac0, t4, t5		;lh		t8, 0(a0)
	mflo		v1, $ac0			;nop
	sra		v1, v1, 16				;lh		t9, 0(a1)	
	sh		v1, 0(a2)				;dpaq_s.w.ph	$ac0, t6, t7
	mflo		v0, $ac0			;nop
	sra		v0, v0, 16				;lh		t4, 2(a0)
	sh		v0, 0(t3)				;dpaq_s.w.ph	$ac0, t8, t9
	mflo		v1, $ac0			;nop
	sra		v1, v1, 16				;lh		t5, 2(a1)
	sh		v1, 0(t2);nop
	dpaq_s.w.ph	$ac0, t4, t5		;lh		t6, 4(a0)
	mflo		v0, $ac0			;nop
	sra		v0, v0, 16				;lh		t7, 4(a1)
	sh		v0, 0(t1);nop
	dpaq_s.w.ph	$ac0, t6, t7		;lh		t4, 6(a0)
	mflo		v1, $ac0			;nop
	sra		v1, v1, 16				;lh		t5, 6(a1)
	sh		v1, 0(t0)				;nop
	lh		t6, 8(a0)				;addu	a0, a0, 10
	lh		t7, 8(a1)				;addu	a1, a1, 10
	addi	a2, a2, -((NB_POS+1)*2)
	addi	t3, t3, -((NB_POS+1)*2)
	addi	t2, t2, -((NB_POS+1)*2)
	addi	t1, t1, -((NB_POS+1)*2)
	addi	t0, t0, -((NB_POS+1)*2)
2:
	bgtz	a3, 1b
	addi	a3, a3, -1
	
	dpaq_s.w.ph	$ac0, t4, t5		;subu	s4, s4, (NB_POS*2)//l_fin_sup -=NB_POS
	mflo		v1, $ac0			;subu	s5, s5, 2	//l_fin_inf --
	sra		v1, v1, 16				;dpaq_s.w.ph	$ac0, t6, t7
	sh		v1, 0(a2)				;addu	s7, s7, (STEP*2)//ptr_hf += STEP
	mflo		v0, $ac0			;sub	t0, s3, NB_POS-1
	sra		v0, v0, 16		
	sh		v0, 0(t3)		
	// }	
	bltz	t0, 3b
	addu	s3, s3, 1
	/*
	(5) compute rri0i4 ...
	*/
	li		s3, 0	//s3=k
	move	s6, s2	//s6=ptr_hd	
	li		s4, (MSIZE-1)*2	//s4=l_fin_sup
	li		s5, (MSIZE-2)*2	//s5=l_fin_inf	
	addu	s7, s2, 8		//s7=ptr_hf	
	.align	3
3:	//for(k=0;k<NB_POS;k++) {
	move	a0, s6	//a0->ptr_h1	
	move	a1, s7	//a1->ptr_h2			
	lh		t4, 0(a0)	
	addu	t3, s1, i0i4
	lh		t5, 0(a1)
	addu	t3, t3, s4	//t3=p3
	addu	t2, s1, i2i3	;li		t9, NB_POS
	addu	t2, t2, s5		;add	t8, s3, 1//t2=p2
	addu	t1, s1, i1i2	;sub	t9, t9, t8
	addu	t1, t1, s5		;sub	t9, t9, 1//t1=p1
	addu	t0, s1, i0i1	;move	a3, t9
	addu	t0, t0, s5	//t0=p0	
	lh		t6, 2(a0)
	mult	zero,zero	
	lh		t7, 2(a1)
	addi	a0, a0, 4
	addi	a1, a1, 4
	bltz	t9, 2f	//check if k==7, then bypass loop
	nop
	//h1 t4:[3] t6:[4] t8:[2]
	//h2 t5:[3] t7:[4] t9:[2]
	.align	3
1:	
	dpaq_s.w.ph	$ac0, t4, t5		;lh		t8, 0(a0)
	mflo	v0, $ac0				;nop
	sra		v0, v0, 16				;lh		t9, 0(a1)
	sh		v0, 0(t3)				;dpaq_s.w.ph	$ac0, t6, t7		
	dpaq_s.w.ph	$ac0, t8, t9		;lh		t4, 2(a0)
	mflo	v1, $ac0				;nop
	sra		v1, v1, 16				;lh		t5, 2(a1)
	sh		v1, 0(t2)				;nop
	dpaq_s.w.ph	$ac0, t4, t5		;lh		t6, 4(a0)
	mflo	v0, $ac0				;nop
	sra		v0, v0, 16				;lh		t7, 4(a1)
	sh		v0, 0(t1)				;nop
	dpaq_s.w.ph	$ac0, t6, t7		;lh		t4, 6(a0)
	mflo	v1, $ac0				;nop
	sra		v1, v1, 16				;lh		t5, 6(a1)
	sh		v1, 0(t0)				;nop
	lh		t6, 8(a0)				;addu	a0, a0, 10
	lh		t7, 8(a1)				;addu	a1, a1, 10
	addi	t3, t3, -((NB_POS+1)*2)
	addi	t2, t2, -((NB_POS+1)*2)
	addi	t1, t1, -((NB_POS+1)*2)
	addi	t0, t0, -((NB_POS+1)*2)
	
2:
	bgtz	a3, 1b
	addi	a3, a3, -1
	
	dpaq_s.w.ph	$ac0, t4, t5		;subu	s4, s4, (NB_POS*2)//l_fin_sup -=NB_POS
	mflo		v0, $ac0			;subu	s5, s5, 2	//l_fin_inf --
	sra		v0, v0, 16				;addu	s7, s7, (STEP*2)//ptr_hf += STEP
	sh		v0, 0(t3)				;sub	t0, s3, NB_POS-1	
	// }	
	bltz	t0, 3b
	addi	s3, s3, 1
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
	.end	Cor_hs
	
	
/*
Word16 D4i40_17_fast(
	Word16 dn[], 	align 4
	Word16 rr[], 	align 4
	Word16 h[],  	align 8
	Word16 cod[], 	align 4
	Word16 y[],		align 4
	Word16 *sign
local:
	sign_dn[]->TmpVct[0]
	sign_dn_inv[]->TmpVct[40]
	psk,alpk->TmpVct[80]
	ip0~3->TmpVct[84~87]
	tmp_vect->TmpVct[88~]
	i1_offset->TmpVct[100]
	h[]->60(sp)
	cod[]->64(sp)
	y[]->68(sp)
	sign->72(sp)
	76(sp)->backup of s0
	80(sp)->backup of s1
	84(sp)->backup of t9
	88(sp)->backup of s4
*/
	.text
	//.section ".Cor_hs" , "ax"
	.set	noreorder
	.set	noat
	.globl	D4i40_17_fasts
	.ent	D4i40_17_fasts
D4i40_17_fasts:
	lw		t0, 16(sp)	//for GNU, load y[] from stack
	lw		t1, 20(sp)	//for GNU, load *sign from stack
	nop
	subu	sp, sp, 128
	sw		ra, 20(sp)
	sw		s0, 24(sp)
	sw		s1, 28(sp)
	sw		s2, 32(sp)	;move	s0, a0//s0->dn[]
	sw		s3, 36(sp)	;move	s1, a1//s1->rr[]
	sw		s4, 40(sp)	
	sw		s5, 44(sp)	
	sw		s6, 48(sp)	
	sw		s7, 52(sp)
	sw		a2, 60(sp)	//push h[] to stack
	sw		a3, 64(sp)	//push cod[] to stack
	sw		t0, 68(sp)	//push y[] to stack
	sw		t1, 72(sp)	//push sign to stack
	////////////////
	/* chose the sign of the impulse */
	.extern	TmpVct
	la		s2, TmpVct	//s2->TmpVct[0]
	move	t7, s2		//t7->sign_dn[]
	addu	t8, s2, 40*2//t8->sign_dn_inv[]
	lh		t0, 0(a0)	//t0=dn[]
	li		a2, L_SUBFR-1
1:
	slt		t9, t0, zero;subq_s.w	t1, zero, t0//t1=-dn[0]
	li		t2, MAX_16	;li		t4, MIN_16	//t2=sign_dn[]
	li		t3, MIN_16	;lh		t6, 2(a0)//t3=sign_dn_inv[]	
	movn	t0, t1, t9	;li		t5, MAX_16
	movn	t2, t4, t9	;sh		t0, 0(a0)
	movn	t3, t5, t9	;sh		t2, 0(t7)
	sh		t3, 0(t8)	;move	t0, t6	
	addu	a0, a0, 2
	addu	t7, t7, 2
	addu	t8, t8, 2
2:
	bgtz	a2, 1b
	addi	a2, a2, -1
	
	sw		s0, 76(sp)	;nop//backup s0
	
	
	
	/*
	Modification of rrixiy[] to take signs into account 
	*/
	// (1)
	addu	a0, s1, i0i1;addu	a1, s1, i0i2//a0=p0
	addu	a2, s1, i0i3;addu	a3, s1, i0i4//a2=p2
	li		t9, 0	//t9=i0
	move	t7, s2	//t7=sign_dn[]	
3:	
	lh		v0, 0(t7)	;move	t8, s2
	addu	t7, t7, (STEP*2)
	li		t4, L_SUBFR/(STEP*2)-1	;addu	t5, s2, 40*2
	slt		t6, v0, zero			;move	at, t4		//at=loop count
	movn	t8, t5, t6			//t8=psign	
	// psign: t4:[10 11] t5:[12 13] t6:[10 11]
	// s3:p0[3 2], s4:p1[2 3], s5:p2[1 0],s6:p3[0 1]	
	lw		s3, 0(a0)	
	lw		s4, 0(a1)
	lw		t4, 0(t8)				;sra		t4, t4, 16	
	lw		t5, 4(t8)				//t4=s[1], t5=s[3]|s[2]//;	packrl.ph	s3, s3, s3
	addu	a0, a0, 4
	addu	a1, a1, 4
	addu	t8, t8, 8
	.align	3
1:	
	muleq_s.w.phr	t0, t4, s3	//s[1]*p0[0]
	muleq_s.w.phr	t1, t5, s4	//s[2]*p1[0]
	lw		s5, 0(a2)	
	lw		s6, 0(a3)
	lw		t6, 0(t8)			//t6=s[5]|s[4]
	sra		t5, t5, 16
	muleq_s.w.phr	t2, t5, s5	//s[3]*p2[0]
	muleq_s.w.phr	t3, t6, s6	//s[4]*p3[0]	
	addu	a2, a2, 4	
	addu	a3, a3, 4	
	lw		t4, 4(t8)			//t4=s[7]|s[6]
	lw		t5, 8(t8)			//t5=s[9]|s[8]
	muleq_s.w.phl	v1, t4, s4	//s[7]*p1[1]
	sll		t4, t4, 16
	muleq_s.w.phl	v0, t4, s3	//s[6]*p0[1]
	muleq_s.w.phl	s0, t5, s6	//s[9]*p3[1]
	sll		t5, t5, 16
	muleq_s.w.phl	s7, t5, s5	//s[8]*p2[1]
	lw		s3, 0(a0)
	lw		s4, 0(a1)
	lw		t4, 12(t8)
	lw		t5, 16(t8)
	sra		t4, t4, 16
	addu	t8, t8, 20
	addu	a0, a0, 4					
	addu	a1, a1, 4
	sra		v0, v0, 16
	sra		v1, v1, 16
	sra		s7, s7, 16		
	sra		s0, s0, 16	
	packrl.ph 	t0, v0, t0					
	packrl.ph	t1, v1, t1				
	packrl.ph	t2, s7, t2	
	packrl.ph	t3, s0, t3
			
	sw		t0, -8(a0)
	sw		t1, -8(a1)				
	sw		t2, -4(a2)									
	sw		t3, -4(a3)	
2:	
	bgtz	at, 1b
	addi	at, at, -1

	subu	a0, a0, 4	;subu	a1, a1, 4		
	sub		t4, t9, L_SUBFR-5
	bltz	t4, 3b
	addu	t9, t9, STEP
	
	// (2)
	addu	a0, s1, i1i2;addu	a1, s1, i1i3//a0=p0
	addu	a2, s1, i1i4					//a2=p2
	li		t9, 1	//t9=i1
	addu	t7, s2, 2//t7=sign_dn[1]	
3:	
	lh		v0, 0(t7)				;move	t8, s2//t8->sign_dn[]
	addu	t7, t7, (STEP*2)
	li		t4, L_SUBFR/(STEP*2)-1	;addu	t5, s2, 40*2
	slt		t6, v0, zero			;move	at, t4//at=loop count
	movn	t8, t5, t6				//t8=psign		
	// psign: t4:[6 7] t5:[8 9] t6:[10 11]
	// s3:p0[0 1], s4:p1[0 1], s5:p2[0 1]
	// t3:p1[1 0]					//t8->psign[2]
	lw		s3, 0(a0)				//s3=p0[1]|p0[0]
	lw		s4, 0(a1)				//s4=p1[1]|p1[0]
	lw		t4, 4(t8)				//t4=s[3]|s[2]
	lw		t5, 8(t8)				//t5=s[5]|s[4]
	addu	a0, a0, 4
	addu	a1, a1, 4
	addu	t8, t8, 12	
	.align	3
1:	
	muleq_s.w.phr	t0, t4, s3	//s[2]*p0[0]
	lw		s5, 0(a2)			//s5=p2[1]|p2[0]
	sra		t4, t4,	16
	muleq_s.w.phr	t1, t4, s4	//s[3]*p1[0]
	muleq_s.w.phr	t2, t5, s5	//s[4]*p2[0]
	lw		t4, 0(t8)			//t4=s[7]|s[6]
	muleq_s.w.phl	t3, t4, s3	//s[7]*p0[1]
	lw		t5, 4(t8)			//t5=s[9]|s[8]
	muleq_s.w.phl	v1, t5, s5	//s[9]*p2[1]
	sll		t5, t5, 16
	muleq_s.w.phl	v0, t5, s4	//s[8]*p1[1]
	lw		s3, 0(a0)
	lw		s4, 0(a1)
	lw		t4, 12(t8)
	lw		t5, 16(t8)
	addu	a0, a0, 4
	addu	a1, a1, 4
	addu	a2, a2, 4
	addu	t8, t8, 20

	sra		t3, t3, 16	
	sra		v0, v0, 16
	sra		v1, v1, 16
	
	packrl.ph 	t0, t3, t0			
	packrl.ph	t1, v0, t1			
	packrl.ph	t2, v1, t2
	
	sw		t0, -8(a0)
	sw		t1, -8(a1)
	sw		t2, -4(a2)				
2:	
	bgtz	at, 1b
	addi	at, at, -1	
	
	subu	a0, a0, 4	;subu	a1, a1, 4		
	sub		t4, t9, L_SUBFR-4
	bltz	t4, 3b
	addu	t9, t9, STEP
	// (3)
	addu	a0, s1, i2i3;addu	a1, s1, i2i4//a0=p0,a1=p1
	li		t9, 2	//t9=i2
	addu	t7, s2, 4//t7=sign_dn[2]	
3:	
	lh		v0, 0(t7)		;move	t8, s2//t8->sign_dn[]
	addu	t7, t7, (STEP*2)
	li		t4, L_SUBFR/(STEP*2)-1	;addu	t5, s2, 40*2
	slt		t6, v0, zero		;move	at, t4//at=loop count
	movn	t8, t5, t6			//t8=psign		
	// psign: t4:[8 9] t5:[10 11] t6:[12 13]
	// s3:p0[0 1], s4:p1[0 1], s5:p2[]
	// t3:p0[1 0]
	lw		s3, 0(a0)	//s3=p0[1]|p0[0]
	lw		s4, 0(a1)	//s4=p1[1]|p1[0]	
	lw		t4, 4(t8)	//t4=s[3]|s[2]
	lw		t5, 8(t8)	//t5=s[5]|s[4]//;packrl.ph t3, s3, s3
	addu	a0, a0, 4
	addu	a1, a1, 4
	addu	t8 ,t8, 12	//t8->psign[2]
	.align	3
1:	
	sra		t4, t4, 16
	muleq_s.w.phr	t0, t4, s3		//s[3]*p0[0]
	muleq_s.w.phr	t1, t5, s4		//s[4]*p1[0]
	lw		v0, 4(t8)	//v0=s[9]|s[8]
	muleq_s.w.phl	t3, v0, s4		//s[9]*p1[1]
	sll		v0, v0, 16
	muleq_s.w.phl	t2, v0, s3		//s[8]*p0[1]
	lw		s3, 0(a0)	//s3=p0[3]|p0[2]
	lw		s4, 0(a1)	//s4=p1[3]|p1[2]
	lw		t4, 12(t8)	//t4=s[13]|s[12]
	lw		t5, 16(t8)	//t5=s[15]|s[14]
	addu	a0, a0, 4
	addu	a1, a1, 4
	addu	t8, t8, 20
	
	sra		t2, t2, 16
	sra		t3, t3, 16
	packrl.ph	t0, t2, t0
	packrl.ph	t1, t3,	t1
	sw		t0, -8(a0)
	sw		t1, -8(a1)			
2:	
	bgtz	at, 1b
	addi	at, at, -1
	
	subu	a0, a0, 4	;subu	a1, a1, 4		
	sub		t4, t9, L_SUBFR-3
	bltz	t4, 3b
	addu	t9, t9, STEP
	
	lw		s0, 76(sp)		//s0=dn[]
 /*-------------------------------------------------------------------*
  * Search the optimum positions of the four pulses which maximize    *
  *     square(correlation) / energy                                  *
  *-------------------------------------------------------------------*/
	li		t4, 0xffff0001	//t4=[-1 1] in 16 bits
	sw		t4, (160)(s2)	//TmpVct[80,81]=psk, alpk
	// ptr_rri0i3_i4=rri0i3=s1+i0i3
	// ptr_rri1i3_i4=rri1i3=s1+i1i3
	// ptr_rri2i3_i4=rri2i3=s1+i2i3
	// ptr_rri3i3_i4=rri3i3=s1+i3i3
	li		t0, 0x00000001
	li		t1, 0x00020003
	sw		t0, (168)(s2)	//TmpVct[84~87]=ip0,ip1,ip2,ip3
	sw		t1, (172)(s2)
	li		s3, 0			//s3=[ix iy]
	li		s4, 0			//s4=[x ps]
	/* search 2 times: track 3 and 4 */
	li		t9, 3			//t9 = track
9:	// for(track=3; track<5; track++) {
	/*--------------------------------------------
	 *depth first search 3, phase A: track 2 and 3/4.
	 *--------------------------------------------*/
	li		t8, 0xffff0001	//t8=[sq alp]
	/* i0 loop: 2 positions in track 2 */
	li		t7, -1			//t7=prev_i0
	li		v1, 2			//v1=i	
3:	// for i=0 i<2; i++)
	li		v0, -1			//v0=max
	addu	a0, s0, 4		//a0->dn[2]
	li		a1, 2			//a1->j
	/* search "dn[]" maximum position in track 2 */
	li		at, L_SUBFR/STEP-1
	lh		t4, 0(a0)//t4=dn[j]
	addu	a0, a0, (2*STEP)
	.align	3
1:	
	sgt		t0, t4, v0		;sne	t1, t7, a1
	and		t0, t0, t1		;lh		t5, 0(a0)
	addu	a0, a0, (2*STEP)
	movn	v0, t4, t0		;movn	s5, a1, t0	//s5=i0	
	addi	a1, a1, STEP	;move	t4, t5
2:	
	bgtz	at, 1b
	addi	at, at, -1
	
	move	t7, s5				;li		t0, 6554
	muleq_s.w.phr	t1, s5, t0	;addu	t2, s0, s5
	addu	t2, t2, s5			;//t2->dn[i0]
	lh		t6, 0(t2)			;sra	t1, t1, 16	//t1=j,t6=ps1	
	addu	t0, s1, i2i2		;li		t3, _1_4
	addu	t0, t0, t1
	addu	t0, t0, t1			//t0=p0
	lh		t0, 0(t0)			//t0=*p0
	sll		t1, t1, 3+1			//t1=j<<(3+1)
	muleq_s.w.phr	t5, t0, t3	//t5=alp1
	/* i1 loop: 8 positions in track 2 */
	addu	a2, s1, i2i3				//a2=p0
	addu	t0, s1, i2i4;sub	t2, t9, 4
	movz	a2, t0, t2	;addu	a3, s1, i3i3//a3=p1
	addu	t0, s1, i4i4;addu	a2, a2, t1
	movz	a3, t0, t2			//if track==4, ptr_i3i3_i4=rri4i4
	li		at, L_SUBFR/STEP-1
	move	s6, t9				//s6=i1
	addu	a0, s0, t9
	addu	a0, a0, t9	//a0->dn[i1]
	.align	3
1:
	lh		t0, 0(a0)			;li		t1, _1_2//t0=dn[i1]
	lh		t2, 0(a2)			;li		t3, _1_4	//t2=*p0
	addq_s.ph	v0, t6, t0		;sll	v0, v0, 16
	sra		v0, v0, 16					
	;lh		t4, 0(a3)	//v0=ps2 t4=*p1
	sll		t1, t2, (14+1)		;li		t0, 0x00008000//t1=*p0 * _1_2
	sll		t3, t4, (13+1)		;addu	a0, a0, (2*STEP)
	muleq_s.w.phr	t2, v0, v0	;addu	a2, a2, 2
	addq_s.w	t1, t1, t3		;addu	a3, a3, 2
	nop							;sra	t2, t2, 16//t2=sq2
	muleq_s.w.phr	t4, t8, t2	;addq_s.w t1, t1, t5//t1=alp2
								;addq_s.w 	t1, t1, t0
	muleq_s.w.phl 	t0, t8, t1	;//HI(t1)=alp_16						
	packrl.ph t2, t2, t1		//t2=[sq2 alp_16]
	sll			t3, s6, 16
	subq_s.w	t0, t4, t0		;packrl.ph t4, s5, t3//t4=[i0 i1]
	sgt		t0, t0, zero
	movn	t8, t2, t0			;movn	s3, t4, t0
	movn	s4, v0, t0			;addi	s6, s6, STEP
2:	
	bgtz	at, 1b
	addi	at, at, -1
	// } end i
	sub		v1, v1, 1
	bgtz	v1, 3b
	nop
	
	/*------------------------------------------------------------------*
    * depth first search 3, phase B: track 0 and 1.                    *
    *------------------------------------------------------------------*/
	move	s5, s3					//s5=[i0 i1]
	move	t6, s4				;li		t4, _1_4//t6=ps0
	muleq_s.w.phr	t7, t8, t4	;li		t5, 0x199a199a	//6554,6554		
								;li		t8, 0xffff0001	//t8=[sq alp]	
	/* build vector for next loop to decrease complexity */		
	muleq_s.w.phl	v0, s5, t5
	muleq_s.w.phr	v1, s5, t5
	sra		v0, v0, 16
	sra		v1, v1, 16
	addu	a0, s1, i1i2	
	addu	a1, s1, i1i3		;//t7=alp0
	addu	t0, s1, i1i4		;sub	t1, t9, 4
	movz	a1, t0, t1			;nop	//if track==4, ptr_i1i3_i4=rri1i4
	addu	a2, s1, i1i1		;addu	a3, s2, 88*2//a2=p2 a3=p3
	li		s6, 0x00008000 
	sll		v0, v0, 1			//v0=mult(i0,6554)*2
	sll		v1, v1, 1			;addu	a0, a0, v0	//a0=p0 v1=mult(i1,6554)*2
	addu	a1, a1, v1			//;li		t5, (_1_8*32768+_1_8)//a1=p1
	li		at, L_SUBFR/(STEP*2)-1
	lh		t0, 0(a0)//t4=_1_4
	lh		t1, 0(a1)//t5=_1_8
	addu	a0, a0, (NB_POS*2)
	addu	a1, a1, (NB_POS*2)
	.align	3
	//t2:[p20 p21]
1:
	sll		t4,t0,(13+1)		;	lh		t2, 0(a2)
	sll		t5,t1,(13+1)		;	lh		t0, 0(a0)
	addq_s.w 	t4, t4, t5		;	lh		t1, 0(a1)
	sll		t3, t2, 12+1		;	li		t5, 0x00008000
	addq_s.w	t4, t4, t3		;	lh		t2, 2(a2)
	addq_s.w	t4, t4, t5		;	sll		t0, t0, (13+1)
	sll		t1,t1,(13+1)		;	sll		t2, t2, (12+1)
	addq_s.w	t3, t0, t1		;	addq_s.w	t2, t2, t5
	addq_s.w	t3, t3, t2		;	lh		t0, (NB_POS*2)(a0)
	packrl.ph	t3, zero, t3	;	packrl.ph t4, t3, t4
	addu	a2, a2, 4			;	lh		t1, (NB_POS*2)(a1)	
	sw		t4, 0(a3)			;	addu	a3, a3, 4
	addu	a0, a0, (NB_POS*2)*2;	addu	a1, a1, (NB_POS*2)*2
2:	
	bgtz	at, 1b
	addi	at, at, -1	
	
	
	/* i2 loop: 8 positions in track 0 */
	addu	a0, s1, i0i2;addu	a1, s1, i0i3
	addu	t0, s1, i0i4;sub	t1, t9, 4
	movz	a1, t0, t1	;nop//if track==4, ptr_i0i3_i4=rri0i4	
	addu	a0, a0, v0	;addu	a1, a1, v1	//a0=p0 a1=p1
	addu	a2, s1, i0i0;addu	a3, s1, i0i1//a2=p2 a3=p3
	li		t5, 0		;addu	s6, s0, 0//t5=i2 s6=dn[i2]
	
	sw		s1, 80(sp)	//backup s1
	sw		t9, 84(sp)	//backup t9
	sw		s4, 88(sp)	//backup s4
	.align	3
	
3:	//for(i2=0; i2<L_SUBFR; i2+=STEP) {
	lh		t0, 0(s6)
	lh		t1, 0(a0)
	lh		t2, 0(a1)			;
	addq_s.ph	t9, t0, t6		;sll	t9, t9, 16
	sra		t9, t9, 16			//t9=ps1
	lh		t3, 0(a2)			;sll		t1, t1, (12+1)	
	addu	s6, s6, (STEP*2)	;addu	a0, a0, (NB_POS*2)
	addu	a1, a1, (NB_POS*2)	;addu	a2, a2, 2
	sll		t2, t2, (12+1)		;addq_s.w		s7, t7, t1
	sll		t3, t3, (11+1)		;addq_s.w		s7, s7, t2
	addq_s.w	s7, s7, t3		//s7=alp1
	/* i3 loop: 8 positions in track 1 */
	li		t2, (L_SUBFR/STEP-1)
	lh		t1, 0(a3)			;addu	a3, a3, 2
	addu	v0, s0, 2//v0=dn[i3]	
	lh		t0, 0(v0)			;li	v1, 1//v1=i3 	
	addu	v0 ,v0, (STEP*2)
	addu	t4, s2, 88*2		;move	at, t2//at=loop count, t4=p4	
	.align	3
1:		
	lh		t2, 0(t4)			;
	addq_s.ph	s4, t9, t0		;sll	s4, s4, 16
	sra			s4, s4, 16
	//addq_s.w	s4, t9, t0//s4=ps2
	addu	t4, t4, 2
	sll		t1, t1, (12+1)		;
	addq_s.w	t1, t1, s7		;sll	t2, t2, (14+1)
	addq_s.w	t1, t1, t2		;li		t0, 0x00008000//t1=alp2
	addq_s.w	t1, t1, t0		;//t1=[alp_16 x]
	muleq_s.w.phr	t2, s4, s4
	muleq_s.w.phl	t0, t8, t1	;sra	t2, t2, 16//t2=sq2
	muleq_s.w.phr	t3, t8, t2	;packrl.ph s1, t2, t1//s1=[sq2 alp_16]
	subq_s.w t3, t3, t0			;lh		t0, 0(v0)
	addu	v0, v0, (STEP*2)
				;lh		t1, 0(a3)
	addu	a3, a3, 2			;sll	t2, v1, 16
								;packrl.ph	t2, t5, t2//t0=s t2=[i2 i3]
	sgt		t3, t3, zero		;addi	v1, v1, STEP
	movn	t8, s1, t3			;movn	s3, t2, t3
2:
	bgtz	at, 1b
	addi	at, at, -1
	
	subu	a3, a3, 2
	// } end i2
	sub		t4, t5, L_SUBFR-5
	bltz	t4, 3b
	addu	t5, t5, STEP
	
	lw		s1, 80(sp)	//load s1
	lw		t9, 84(sp)	//load t9
	lw		s4, 88(sp)	//load s4
	
	/*----------------------------------------------------------------*
    * depth first search 3: compare codevector with the best case.   *
    *----------------------------------------------------------------*/
	lw		t0, 160(s2)	//t0=[psk alpk]
	packrl.ph	t1, t8, t8	//t1=[alp sq]
	lw		t2, 168(s2)	//t2=[ip0 ip1]	
	muleq_s.w.phr	t3, t0, t1
	muleq_s.w.phl	at, t0, t1
	subq_s.w		t1, t3, at	
	lw		t3, 172(s2)	//t3=[ip2 ip3]
	sgt		t1, t1, zero
	movn	t0, t8, t1	
	movn	t2, s3, t1	;sw		t0, 160(s2)
	movn	t3, s5, t1	;nop
	sw		t2, 168(s2)
	sw		t3, 172(s2)
   /*------------------------------------------------------------------*
    * depth first search 4, phase A: track 3 and 0.                    *
    *------------------------------------------------------------------*/
	li		t8, 0xffff0001	//t8=[sq alp]
	/* i0 loop: 2 positions in track 3/4 */
	li		t7, -1			//t7=prev_i0
	li		v1, 2			//v1=i	
3:	// for i=0 i<2; i++)
	li		v0, -1			//v0=max
	sll		t0, t9, 1
	addu	a0, s0, t0		//a0->dn[track]
	move	a1, t9			//a1->j
	/* search "dn[]" maximum position in track 3/4 */
	li		at, L_SUBFR/STEP-1
	lh		t4, 0(a0)//t4=dn[j]
	addu	a0, a0, (2*STEP)
	.align	3
1:	
	sgt		t0, t4, v0	;sne	t1, t7, a1
	and		t0, t0, t1	;lh		t5, 0(a0)
	addu	a0, a0, (2*STEP)
	movn	v0, t4, t0	;movn	s5, a1, t0	//s5=i0	
	addi	a1, a1, STEP;move	t4, t5
2:	
	bgtz	at, 1b
	addi	at, at, -1

	move	t7, s5				;li		t0, 6554	//prev_i0=i0
	muleq_s.w.phr	t1, s5, t0	;addu	t2, s0, s5
	addu	t2, t2, s5			;//t2->dn[i0]
	lh		t6, 0(t2)			;sra	t1, t1, 16	//t1=j,t6=ps1	
	addu	t0, s1, i3i3		;li		t3, _1_4	
	addu	v0, s1, i4i4		;sub	t4, t9, 4
	movz	t0, v0, t4			//if track==4,ptr_i3i3_i4=rri4i4
	addu	t0, t0, t1
	addu	t0, t0, t1			//t0=p0
	lh		t0, 0(t0)			//t0=*p0
	sll		t1, t1, 1			//t1=j<<1
	muleq_s.w.phr	t5, t0, t3		//L_mult(*p0, _1_4)//t5=alp1
	/* i1 loop: 8 positions in track 0 */
	addu	a2, s1, i0i3				//a2=p0
	addu	t0, s1, i0i4		;sub	t2, t9, 4
	movz	a2, t0, t2			;nop				//if track==4,ptr_rri0i3_i4=rri0i4	
	addu	a3, s1, i0i0		;addu	a2, a2, t1	//a3=p1
	li		at, L_SUBFR/STEP-1
	li		s6, 0	//s6=i1;
	addu	a0, s0, 0//a0->dn[i1]
	.align	3
1:
	lh		t0, 0(a0)			;li		t1, _1_2	//t0=dn[i1]
	lh		t2, 0(a2)			;li		t3, _1_4	//t2=*p0
	addq_s.ph	v0, t6, t0		;sll	v0, v0, 16
	sra		v0, v0, 16			
//addq_s.w	v0, t6, t0				
	lh		t4, 0(a3)	//v0=ps2 t4=*p1
	sll		t1, t2, (14+1)			;li		t0, 0x00008000//t1=*p0 * _1_2
	sll		t3, t4, (13+1)			;addu	a0, a0, (2*STEP)
	muleq_s.w.phr	t2, v0, v0		;addu	a2, a2, (NB_POS*2)
	addq_s.w	t1, t1, t3			;addu	a3, a3, 2
	nop								;sra	t2, t2, 16//t2=sq2
	muleq_s.w.phr	t4, t8, t2		;addq_s.w t1, t1, t5//t1=alp2
									;addq_s.w t1, t1, t0
	muleq_s.w.phl 	t3, t8, t1		;//HI(t1)=alp_16,t5=alp1						
	subq_s.w 	t0, t4, t3			;packrl.ph	t2, t2, t1//t2=[sq2 alp_16]
	sll		t3, s6, 16				;packrl.ph	t4, s5, t3//t4=[i0 i1]
	sgt		t0, t0, zero
	movn	t8, t2, t0				;movn	s3, t4, t0
	movn	s4, v0, t0				;addi	s6, s6, STEP//s4=ps
	
2:	
	bgtz	at, 1b
	addi	at, at, -1

	// } end i
	sub		v1, v1, 1
	bgtz	v1, 3b
	nop		
	/*------------------------------------------------------------------*
    * depth first search 4, phase B: track 1 and 2.                    *
    *------------------------------------------------------------------*/
    .align	3
	move	s5, s3					;li	t5, 6554//s5=[i0 i1]
	muleq_s.w.phr	v1, s5, t5		//i1*6554
	sra		v1, v1, 16-3			//v1=i1_offset
	sh		v1, 200(s2)				//i1_offset	
	move	t6, s4					;li	t4, _1_4//t6=ps0
	muleq_s.w.phr	t7,t8,t4		;li	t5, 0x199a199a		//6554,6554		
									;li	t8, 0xffff0001		//t8=[sq alp]	
	/* build vector for next loop to decrease complexity */
	muleq_s.w.phl	v0, s5, t5				;addu	a0, s1, i2i3
	addu	t1, s1, i2i4			;sub	t0, t9, 4
	movz	a0, t1, t0				;nop					//if track==4, ptr_rri2i3_i4=rri2i4	
	addu	a1, s1, i0i2			;addu	a2, s1, i2i2	//a2=p2	//a1=p1
	addu	a3, s2, 88*2			;sll	v1, v1, 1		//a3=p3,v1=i1_offset*2
									;addu	a1, a1, v1		//t7=alp0
									;li		s6, 0x00008000
	sra		v0, v0, 16
	sll		v0, v0, 1				//v0=mult(i0,6554)*2
	addu	a0, a0, v0				//a0=p0 v1=mult(i1,6554)*2
	li		at, L_SUBFR/(STEP*2)-1	
	lh		t0, 0(a0)
	lh		t1, 0(a1)
	addu	a0, a0, (NB_POS*2)
	addu	a1, a1, 2
	.align	3
	//t2:[p20 p21]
1:
	sll		t4, t0,(13+1)		;lh		t2, 0(a2)
	sll		t5, t1,(13+1)		;lh		t0, 0(a0)
	addq_s.w 	t4, t4, t5		;lh		t1, 0(a1)
	sll		t3, t2, 12+1		;li		t5, 0x00008000
	addq_s.w	t4, t4, t3		;lh		t2, 2(a2)
	addq_s.w	t4, t4, t5		;sll	t0, t0, (13+1)
	sll		t1, t1,(13+1)		;sll	t2, t2, (12+1)
	addq_s.w	t3, t0, t1		;addq_s.w	t2, t2, t5
	addq_s.w	t3, t3, t2		;lh		t0, (NB_POS*2)(a0)
	sra			t3, t3, 16		;packrl.ph	t4, t3, t4			
	lh		t1, 2(a1)
	sw		t4, 0(a3)			;addu	a3, a3, 4
	addu	a0, a0, (NB_POS*2)*2;addu	a1, a1, 4
	addu	a2, a2, 4
2:	
	bgtz	at, 1b
	addi	at, at, -1
	
	/* i2 loop: 8 positions in track 1 */
	sub		t0, t9, 4	;addu	a0, s1, i1i3	
	addu	t1, s1, i1i4;addu	a1, s1, i0i1
	movz	a0, t1, t0	;nop//if track==4, ptr_rri1i3_i4=rri0i4	
	
	addu	a0, a0, v0	;addu	a1, a1, v1	//a0=p0 a1=p1
	addu	a2, s1, i1i1;addu	a3, s1, i1i2//a2=p2 a3=p3
	li		t5, 1		;addu	s6, s0, 2//t5=i2 s6=dn[i2]
	sw		s1, 80(sp)	//backup s1
	sw		t9, 84(sp)	//backup t9
	sw		s4, 88(sp)	//backup s4
	.align	3
	
3:	//for(i2=0; i2<L_SUBFR; i2+=STEP) {
	lh		t0, 0(s6)
	addu	s6, s6, (STEP*2)
	lh		t1, 0(a0)
	lh		t2, 0(a1)				;//addq_s.w	t9, t0, t6//t9=ps1
	addq_s.ph	t9, t0, t6			;sll		t9, t9, 16
	sra			t9, t9, 16
	lh		t3, 0(a2)				;sll	t1, t1, (12+1)	
	addu	a0, a0, (NB_POS*2)		;addu	a1, a1, 2
	addu	a2, a2, 2
	sll		t2, t2, (12+1)			;addq_s.w	s7, t7, t1
	sll		t3, t3, (11+1)			;addq_s.w	s7, s7, t2
	addq_s.w	s7, s7, t3			//s7=alp1
	/* i3 loop: 8 positions in track 1 */
	li		at, (L_SUBFR/STEP-1)
	lh		t1, 0(a3)				;addu	a3, a3, 2
	addu	v0, s0, 4//v0=dn[i3]	
	lh		t0, 0(v0)				;addu	v0, v0, (STEP*2)
	li		v1, 2					//v1=i3 	
	addu	t4, s2, 88*2			;//t4=p4,at=loop count,t6=ps0	
	.align	3
	
1:		
	addq_s.ph	s4, t9, t0			;sll		s4, s4, 16
	sra			s4, s4, 16						
	lh		t2, 0(t4)//s4=ps2
	sll		t1, t1, (12+1)			;muleq_s.w.phr	t3, s4, s4
	addq_s.w	t1, t1, s7			;sll	t2, t2, (14+1)
	addq_s.w	t1, t1, t2			;li		t0, 0x00008000//t1=alp2
	addq_s.w	t1, t1, t0			;//t1=[alp_16 x]
	muleq_s.w.phl	t0, t8, t1		;sra	t2, t3, 16//t2=sq2
	muleq_s.w.phr	t3, t8, t2		;packrl.ph 	s1, t2, t1//s1=[sq2 alp_16]
	subq_s.w	 t3, t3, t0			;lh		t0, 0(v0)
									;lh		t1, 0(a3)
	sll		t2, v1, 16				;packrl.ph 	t2, t5, t2//t0=s t2=[i2 i3]
	sgt		t3, t3, zero			;addi	v1, v1, STEP
	movn	t8, s1, t3				;movn	s3, t2, t3
	addu	t4, t4, 2
	addu	v0, v0,(STEP*2)
	addu	a3, a3, 2
2:
	bgtz	at, 1b
	addi	at, at, -1
	
	subu	a3, a3, 2
	// } end i2
	sub		t4, t5, L_SUBFR-5
	bltz	t4, 3b
	addu	t5, t5, STEP
	
	lw		s1, 80(sp)	//load s1
	lw		t9, 84(sp)	//load t9
	lw		s4, 88(sp)	//load s4
	
   /*----------------------------------------------------------------*
    * depth first search 1: compare codevector with the best case.   *
    *----------------------------------------------------------------*/
	lw		t0, 160(s2)	//t0=[psk alpk]
	packrl.ph	t1, t8, t8	//t1=[alp sq]
	lw		t2, 168(s2)	//t2=[ip0 ip1]
	mult	zero, zero
		
	muleq_s.w.phr	t3, t0, t1
	muleq_s.w.phl	t4, t0, t1
	subq_s.w		t1, t3, t4
	lw		t3, 172(s2)			//t3=[ip2 ip3]
	packrl.ph 	v0, s5, s3		//v0=[i1 ix]
	packrl.ph	v1, s3, s5		//v1=[iy i0]
	sgt		t1, t1, zero
	movn	t0, t8, t1	
	movn	t2, v0, t1		;sw		t0, 160(s2)
	movn	t3, v1, t1		;nop
	sw		t2, 168(s2)
	sw		t3, 172(s2)
	// } end track
	sub		t4, t9, 4
	bltz	t4, 9b
	addi	t9, t9, 1	
/*	
	Word16 dn[], 	align 4
	Word16 rr[], 	align 4
	Word16 h[],  	align 8
	Word16 cod[], 	align 4
	Word16 y[],		align 4
	Word16 *sign
local:
	sign_dn[]->TmpVct[0]
	sign_dn_inv[]->TmpVct[40]
	psk,alpk->TmpVct[80]
	ip0~3->TmpVct[84~87]
	tmp_vect->TmpVct[88~]
	i1_offset->TmpVct[100]
	h[]->60(sp)
	cod[]->64(sp)
	y[]->68(sp)
	sign->72(sp)
	76(sp)->backup of s0
	80(sp)->backup of s1
	84(sp)->backup of t9
	88(sp)->backup of s4
*/
	/* Set the sign of impulses */
	.align	3
	lw		t2, 168(s2)	//t2=[ip0 ip1] t3=[ip2 ip3]
	lw		t3, 172(s2)
	li		at, L_SUBFR/8-1
	packrl.ph	t0,zero,t2
	sll		t4, t2, 16	
	packrl.ph	t1, zero, t4
	packrl.ph	t2,zero,t3
	sll		t4, t3, 16	
	packrl.ph	t3, zero, t4
	sll		t0, t0, 1 //t0=ip0<<1
	addu	t4, t0, s2	;sll		t1, t1, 1 //t1=ip1<<1
	lh		t4, 0(t4)	;addu		t5, t1, s2
	lh		t5, 0(t5)	;sll		t2, t2, 1 //t2=ip2<<1
	addu	t6, t2, s2	;sll		t3, t3, 1 //t3=ip3<<1
	lh		t6, 0(t6)	;addu		t7, t3, s2	
	lh		t7, 0(t7)	;sra		s4, t4, 2
	lw		s3, 64(sp)	;sra		s5, t5, 2//s3->cod[]	
	sra		s6, t6, 2	;sra		s7, t7, 2
1:
	sw		zero, 0(s3)
	sw		zero, 4(s3)
	sw		zero, 8(s3)
	sw		zero, 12(s3)
	addu	s3, s3, 16
2:
	bgtz	at, 1b
	addi	at, at, -1
	
	subu	s3, s3, L_SUBFR*2
	addu	a0, t0, s3	//a0->cod[ip0]
	addu	a1, t1, s3	;sh		s4, 0(a0)//a1->cod[ip1]
	addu	a2, t2, s3	;sh		s5, 0(a1)//a2->cod[ip2]
	addu	a3, t3, s3	;sh		s6, 0(a2)//a3->cod[ip3]
	sh		s7, 0(a3)	;sra	t0, t0, 1//t0=ip0
	sra		t1, t1, 1	;sra	t2, t2, 1//t1=ip1 t2=ip2
	sra		t3, t3, 1	;lw		s4, 68(sp)//s4->y[] t3=ip3
										//t4=i0 t5=i1
										//t6=i2 t7=i3
										
	/* find the filtered codeword */
	sub		a2, t0, 1
	bltz	a2, 3f		//if a2<0, skip this loop
	li		v0, 0
1:
	sh		zero, 0(s4)
	addu	v0, v0, 2
	addu	s4, s4, 2
2:
	bgtz	a2, 1b
	addi	a2, a2, -1
	
	subu	s4, s4, v0	//s4->y[0]
	.align	3
3:	//i0	
	//s5->h[]
	lw		s5, 60(sp)	;sgt	t8, t4, zero//check i0 > 0 ?
	li		a2, L_SUBFR
	lh		t9, 0(s5)	;sub	a2, a2, t0	//t0=ip0
	sub		a2, a2, 1	;addu	s5, s5, 2
	sll		v1, t0, 1
	addu	a1, s4, v1	;nop//a1->y[ip0]	
1:		
	lh		v1, 0(s5)		;move	a3, t9
	sub		v0, zero, t9	;addu	s5, s5, 2
	movz	a3, v0, t8	
	sh		a3, 0(a1)		;move	t9, v1
	addu	a1, a1, 2
2:
	bgtz	a2, 1b
	addi	a2, a2, -1
	
	//i1
	//s5->h[]
	lw		s5, 60(sp)	;sgt	t8, t5, zero//check i1 > 0 ?
						;li		a2, L_SUBFR
	lh		t9, 0(s5)	;sub	a2, a2, t1	//t1=ip1
	addu	s5, s5, 2	;sub	a2, a2, 1	
						;sll	v1, t1, 1
	addu	a1, s4, v1	//a1->y[ip1]
	lh		s0, 0(a1)	//s0=y[ip1]
1:		
	lh		v1, 0(s5)	;addq_s.ph a3, s0, t9//y[i]+h[j]
	subq_s.ph v0, s0, t9	;lh		s1, 2(a1)
	movz	a3, v0, t8	;move	t9, v1
	sh		a3, 0(a1)	;move	s0, s1
	addu	s5, s5, 2	;addu	a1, a1, 2
2:
	bgtz	a2, 1b
	addi	a2, a2, -1
	//i2
	//s5->h[]
	lw		s5, 60(sp)	;sgt	t8, t6, zero//check i2 > 0 ?
	li		a2, L_SUBFR
	lh		t9, 0(s5)	;sub	a2, a2, t2	//t2=ip2
	addu	s5, s5, 2	;sub		a2, a2, 1	
	sll		v1, t2, 1
	addu	a1, s4, v1	//a1->y[ip2]
	lh		s0, 0(a1)	//s0=y[ip2]
1:		
	lh		v1, 0(s5)	;addq_s.ph a3, s0, t9//y[i]+h[j]
	subq_s.ph v0, s0, t9	;lh		s1, 2(a1)
	movz	a3, v0, t8	;move	t9, v1
	sh		a3, 0(a1)	;move	s0, s1
	addu	s5, s5, 2	;addu	a1, a1, 2
2:
	bgtz	a2, 1b
	addi	a2, a2, -1
	//i3
	//s5->h[]
	lw		s5, 60(sp)	;sgt	t8, t7, zero//check i3 > 0 ?
						;li		a2, L_SUBFR
	lh		t9, 0(s5)	;sub	a2, a2, t3	//t3=ip3
	addu	s5, s5, 2	;sub	a2, a2, 1	
	sll		v1, t3, 1
	addu	a1, s4, v1	//a1->y[ip3]
	lh		s0, 0(a1)	//s0=y[ip3]
1:		
	lh		v1, 0(s5)	;addq_s.ph a3, s0, t9//y[i]+h[j]
	subq_s.ph v0, s0, t9	;lh		s1, 2(a1)
	movz	a3, v0, t8	;move	t9, v1
	sh		a3, 0(a1)	;move	s0, s1
	addu	s5, s5, 2	;addu	a1, a1, 2
2:		
	bgtz	a2, 1b
	addi	a2, a2, -1
	
	/* find codebook index: 17-bit address */
	li		v0, 0		;addi	v1, v0, 1//v0=i
	sgt		t8, t4, zero;nop
	movn	v0, v1, t8	;sgt	t9, t5, zero
	addi	v1, v0, 2	;sgt	t8, t6, zero
	movn	v0, v1, t9	;nop
	addi	v1, v0, 4	;sgt	t9, t7, zero
	movn	v0, v1, t8	;lw		s0, 72(sp)	//s0->sign[]
	addi	v1, v0, 8	;li		t4, 0x199a199a//t4=[6554 6554]
	movn	v0, v1, t9	
	sll		v1, t1, 16
	packrl.ph	t5, t0, v1//t5=[ip0 ip1]
	sh		v0, 0(s0)	
	sll		v1, t3, 16
	packrl.ph	t6, t2, v1// *sign=i
	
	muleq_s.w.phl	t0, t4, t5
	muleq_s.w.phr 	t1, t4, t5
	muleq_s.w.phl	t2, t4, t6
	muleq_s.w.phr	v0, t4, t6
	
	sra		t0, t0, 16
	sra		t1, t1, 16
	sra		t2, t2, 16
	sra		v0, v0, 16
	
	sll		v1, v0, 2
	add		v1, v0, v1	;sll	t5, v0, 1//v1=j
	add		t4, v1, 3	;sll	t1, t1, 3	//ip1<<3
	sub		v1, t3, t4	;sll	t2, t2, 6	//ip2<<6
	add		t3, t5, v1	;add	v0, t0, t1//t3=ip3
	sll		t3, t3, 9	//ip3<<9	
	add		t2, t2, t3
	add		v0, v0, t2	//v0=i									
	////////////////
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
	.end	D4i40_17_fasts
	