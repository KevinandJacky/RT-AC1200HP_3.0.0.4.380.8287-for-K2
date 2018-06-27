/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     Interrupt routines for Ralink RT2880 solution
 *
 *  Copyright 2007 Ralink Inc. (bruce_chang@ralinktech.com.tw)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 **************************************************************************
 * May 2007 Bruce Chang
 * Initial Release
 *
 * May 2009 Bruce Chang
 * support RT2880/RT2883 PCIe
 *
 *
 **************************************************************************
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/hardirq.h>
#include <linux/preempt.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>

#include <asm/irq.h>
#include <asm/mach-ralink/surfboard.h>
#include <asm/mach-ralink/surfboardint.h>
#include <asm/mach-ralink/rt_mmap.h>

#include <asm/mach-ralink/eureka_ep430.h>

#if defined (CONFIG_IRQ_GIC)
#include <asm/gic.h>
#include <asm/gcmpregs.h>
#endif

extern int cp0_compare_irq;
void __init ralink_gpio_init_irq(void);

#if 0
#define DEBUG_INT(x...) printk(x)
#else
#define DEBUG_INT(x...)
#endif

/* cpu pipeline flush */
void static inline ramips_sync(void)
{
	__asm__ volatile ("sync");
}

#if defined (CONFIG_IRQ_GIC)
int gic_present;
int gcmp_present;
static unsigned long _gcmp_base;

#define X GIC_UNUSED
/*
 * This GIC specific tabular array defines the association between External
 * Interrupts and CPUs/Core Interrupts. The nature of the External
 * Interrupts is also defined here - polarity/trigger.
 */
static struct gic_intr_map gic_intr_map[GIC_NUM_INTRS] = {
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //0
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, 
#if 0
        { 0, GIC_CPU_INT4, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //PCIE0
        { 0, GIC_CPU_INT3, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //FE
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //USB3
#else
	{ X, X,            X,           X,              0 }, 
        { 0, GIC_CPU_INT3, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //FE
        { 0, GIC_CPU_INT4, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //PCIE0
#endif

#ifdef CONFIG_RALINK_SYSTICK
	{ 0, GIC_CPU_INT5, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },//5, aux timer(system tick)
#else
	{ X, X,            X,           X,              0 }, //5
#endif

	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, 
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },

        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //10
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, 
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, 
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ X, X,            X,           X,              0 }, //14: NFI
	
	{ X, X,            X,           X,              0 }, //15
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },

        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //20
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ X, X,            X,           X,              0 }, //23 : FIXME
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },

        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT }, //25
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },

        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },//30
        { 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
        /* The remainder of this table is initialised by fill_ipi_map */
};
#undef X

/*
 * GCMP needs to be detected before any SMP initialisation
 */
int __init gcmp_probe(unsigned long addr, unsigned long size)
{
	_gcmp_base = (unsigned long) ioremap_nocache(GCMP_BASE_ADDR, GCMP_ADDRSPACE_SZ);
        gcmp_present = (GCMPGCB(GCMPB) & GCMP_GCB_GCMPB_GCMPBASE_MSK) == GCMP_BASE_ADDR;

        if (gcmp_present)
                printk("GCMP present\n");

        return gcmp_present;

}

/* Return the number of IOCU's present */
int __init gcmp_niocu(void)
{
	return gcmp_present ?
		(GCMPGCB(GC) & GCMP_GCB_GC_NUMIOCU_MSK) >> GCMP_GCB_GC_NUMIOCU_SHF :
		0;
}

#else
void disable_surfboard_irq(unsigned int irq_nr)
{
	//printk("%s(): irq_nr = %d\n",__FUNCTION__,  irq_nr);
	if(irq_nr >5){
		*(volatile u32 *)(RALINK_INTDIS) = (1 << irq_nr);
		ramips_sync();
	}
}

void enable_surfboard_irq(unsigned int irq_nr)
{
	//printk("%s(): irq_nr = %d\n",__FUNCTION__,  irq_nr);
	if(irq_nr >5) {
		*(volatile u32 *)(RALINK_INTENA) = (1 << irq_nr);
		ramips_sync();
	}
}

static struct irq_chip surfboard_irq_type = {
 	.name           = "Ralink",
        .mask           = disable_surfboard_irq,
        .unmask         = enable_surfboard_irq,
};
#endif


#ifdef CONFIG_MIPS_MT_SMP
static int gic_resched_int_base;
static int gic_call_int_base;

#define GIC_RESCHED_INT(cpu) (gic_resched_int_base+(cpu))
#define GIC_CALL_INT(cpu) (gic_call_int_base+(cpu))

static unsigned int ipi_map[NR_CPUS];
static irqreturn_t ipi_resched_interrupt(int irq, void *dev_id)
{
        return IRQ_HANDLED;
}

#ifdef CONFIG_RALINK_SYSTICK

extern spinlock_t	ra_teststat_lock;
extern void ra_percpu_event_handler(void);

static irqreturn_t ipi_call_interrupt(int irq, void *dev_id)
{
	unsigned int cpu = smp_processor_id();
	unsigned int cd_event = 0;
	unsigned long flags;

	spin_lock_irqsave(&ra_teststat_lock, flags);

	cd_event = (*( (volatile u32 *)(RALINK_TESTSTAT)  ))  & ((0x1UL) << cpu);
	if(cd_event)
		(*((volatile u32 *)(RALINK_TESTSTAT))) &= ~cd_event;

	spin_unlock_irqrestore(&ra_teststat_lock, flags);

	// FIXME!!!
	if(cd_event){
		ra_percpu_event_handler();
	}
        smp_call_function_interrupt();

        return IRQ_HANDLED;
}
#else
static irqreturn_t ipi_call_interrupt(int irq, void *dev_id)
{
	smp_call_function_interrupt();
	return IRQ_HANDLED;
}
#endif

static struct irqaction irq_resched = {
        .handler        = ipi_resched_interrupt,
        .flags          = IRQF_DISABLED|IRQF_PERCPU,
        .name           = "IPI_resched"
};

static struct irqaction irq_call = {
        .handler        = ipi_call_interrupt,
        .flags          = IRQF_DISABLED|IRQF_PERCPU,
        .name           = "IPI_call"
};

unsigned int plat_ipi_call_int_xlate(unsigned int cpu)
{
	        return GIC_CALL_INT(cpu);
}

unsigned int plat_ipi_resched_int_xlate(unsigned int cpu)
{
	        return GIC_RESCHED_INT(cpu);
}

static void __init fill_ipi_map1(int baseintr, int cpu, int cpupin)
{
        int intr = baseintr + cpu;
        gic_intr_map[intr].cpunum = cpu;
        gic_intr_map[intr].pin = cpupin;
        gic_intr_map[intr].polarity = GIC_POL_POS;
        gic_intr_map[intr].trigtype = GIC_TRIG_EDGE;
        gic_intr_map[intr].flags = GIC_FLAG_IPI;
        ipi_map[cpu] |= (1 << (cpupin + 2));
}

static void __init fill_ipi_map(void)
{
        int cpu;

        for (cpu = 0; cpu < NR_CPUS; cpu++) {
                fill_ipi_map1(gic_resched_int_base, cpu, GIC_CPU_INT1);
                fill_ipi_map1(gic_call_int_base, cpu, GIC_CPU_INT2);
        }
}

static void mips1004k_ipi_irqdispatch(void)
{
	int irq;

	irq = gic_get_int();

	if (irq < 0)
		return;  /* interrupt has already been cleared */

	do_IRQ(MIPS_GIC_IRQ_BASE + irq);
}

void __init arch_init_ipiirq(int irq, struct irqaction *action)
{
	setup_irq(irq, action);
	set_irq_handler(irq, handle_percpu_irq);
}
#endif /* CONFIG_MIPS_MT_SMP */



static inline int ls1bit32(unsigned int x)
{
	int b = 31, s;

	s = 16; if (x << 16 == 0) s = 0; b -= s; x <<= s;
	s =  8; if (x <<  8 == 0) s = 0; b -= s; x <<= s;
	s =  4; if (x <<  4 == 0) s = 0; b -= s; x <<= s;
	s =  2; if (x <<  2 == 0) s = 0; b -= s; x <<= s;
	s =  1; if (x <<  1 == 0) s = 0; b -= s;

	return b;
}

#if defined (CONFIG_IRQ_GIC)
extern struct gic_pcpu_mask pcpu_masks[NR_CPUS];
void surfboard_hw0_irqdispatch(unsigned int prio)
{
	int irq = 0;
	int pending = 0;
	int intrmask = 0;
	int int_status = 0;

	pending = *(volatile u32 *)(0xbfbc0480);
	intrmask = *(volatile u32 *)(0xbfbc0400);
#ifdef CONFIG_MIPS_MT_SMP
	int_status = pending & intrmask & *pcpu_masks[smp_processor_id()].pcpu_mask;
#else
	int_status = pending & intrmask;
#endif


	if (int_status == 0)
		return;

	irq = ls1bit32(int_status);

	if(irq >= GIC_NUM_INTRS)
		return;

	do_IRQ(irq);

}
#else
void surfboard_hw0_irqdispatch(unsigned int prio)
{
	unsigned long int_status;
	int irq;

	if (prio)
		int_status = *(volatile u32 *)(RALINK_IRQ1STAT);
	else
		int_status = *(volatile u32 *)(RALINK_IRQ0STAT);

	/* if int_status == 0, then the interrupt has already been cleared */
	if (unlikely(int_status == 0))
		return;
	irq = ls1bit32(int_status);

	/*
	 * RT2880:
	 * bit[3] PIO Programmable IO Interrupt Status after Mask
	 * bit[2] UART Interrupt Status after Mask
	 * bit[1] WDTIMER Timer 1 Interrupt Status after Mask
	 * bit[0] TIMER0 Timer 0 Interrupt Status after Mask
	 *
	 * RT2883/RT3052:
	 * bit[17] Ethernet switch interrupt status after mask
	 * bit[6] PIO Programmable IO Interrupt Status after Mask
	 * bit[5] UART Interrupt Status after Mask
	 * bit[2] WDTIMER Timer 1 Interrupt Status after Mask
	 * bit[1] TIMER0 Timer 0 Interrupt Status after Mask
	 */
#if defined (CONFIG_RALINK_TIMER_DFS) || defined (CONFIG_RALINK_TIMER_DFS_MODULE)
#if defined (CONFIG_RALINK_RT2880_SHUTTLE) || \
    defined (CONFIG_RALINK_RT2880_MP)
        if (irq == 0) {
                irq = SURFBOARDINT_TIMER0;
        }
#else
        if (irq == 1) {
                irq = SURFBOARDINT_TIMER0;
        }else if (irq == 2) {
                irq = SURFBOARDINT_WDG;
        }
#endif
#endif

#if defined (CONFIG_RALINK_RT2880_SHUTTLE) ||   \
    defined (CONFIG_RALINK_RT2880_MP)
	if (irq == 3) {
#ifdef CONFIG_RALINK_GPIO 
		/* cause gpio registered irq 7 (see rt2880gpio_init_irq()) */
		irq = SURFBOARDINT_GPIO;
		printk("surfboard_hw0_irqdispatch(): INT #7...\n");
#else
		printk("surfboard_hw0_irqdispatch(): External INT #3... surfboard discard!\n");
#endif
	}
#else
	/* ILL_ACC */ 
	if (irq == 3) {
		irq = SURFBOARDINT_ILL_ACC;
	}
#endif
#if defined (CONFIG_RALINK_PCM) || defined (CONFIG_RALINK_PCM_MODULE)
	/* PCM */ 
	if (irq == 4) {
		irq = SURFBOARDINT_PCM;
	}
#endif

	/* UARTF */ 
	if (irq == 5) {
		irq = SURFBOARDINT_UART;
	}
	do_IRQ(irq);
	return;
}
#endif

void __init arch_init_irq(void)
{
#if (defined (CONFIG_IRQ_GIC) && defined(CONFIG_MIPS_MT_SMP)) || !defined (CONFIG_IRQ_GIC)
	int i;
#endif

	mips_cpu_irq_init();

#if defined (CONFIG_IRQ_GIC)
	set_irq_handler(SURFBOARDINT_PCIE0, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_PCIE1, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_PCIE2, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_FE, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_USB, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_SYSCTL, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_DRAMC, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_PCM, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_HSGDMA, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_GPIO, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_DMA, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_NAND, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_I2S, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_SPI, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_SPDIF, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_CRYPTO, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_SDXC, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_PCTRL, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_ESW, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_UART_LITE1, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_UART_LITE2, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_UART_LITE3, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_NAND_ECC, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_I2C, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_WDG, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_TIMER0, handle_percpu_irq);
	set_irq_handler(SURFBOARDINT_TIMER1, handle_percpu_irq);

	if (gcmp_present)  {
		GCMPGCB(GICBA) = GIC_BASE_ADDR | GCMP_GCB_GICBA_EN_MSK;
                gic_present = 1;
	}

	if (gic_present) {
#if defined(CONFIG_MIPS_MT_SMP)
                gic_call_int_base = GIC_NUM_INTRS - NR_CPUS;
                gic_resched_int_base = gic_call_int_base - NR_CPUS;
                fill_ipi_map();
#endif

		gic_init(GIC_BASE_ADDR, GIC_ADDRSPACE_SZ, gic_intr_map,
				ARRAY_SIZE(gic_intr_map), MIPS_GIC_IRQ_BASE);

#if defined(CONFIG_MIPS_MT_SMP)
                /* Argh.. this really needs sorting out.. */
                printk("CPU%d: status register was %08x\n", smp_processor_id(), read_c0_status());
                write_c0_status(read_c0_status() | STATUSF_IP3 | STATUSF_IP4);
                printk("CPU%d: status register now %08x\n", smp_processor_id(), read_c0_status());
                write_c0_status(0x1100dc00);
                printk("CPU%d: status register frc %08x\n", smp_processor_id(), read_c0_status());
                
		/* set up ipi interrupts */
                for (i = 0; i < NR_CPUS; i++) {
                        arch_init_ipiirq(MIPS_GIC_IRQ_BASE + GIC_RESCHED_INT(i), &irq_resched);
                        arch_init_ipiirq(MIPS_GIC_IRQ_BASE + GIC_CALL_INT(i), &irq_call);
                }
#endif

	}
#else
	for (i = 0; i <= SURFBOARDINT_END; i++) {
		set_irq_chip_and_handler(i, &surfboard_irq_type, handle_level_irq);
	}
	/* Enable global interrupt bit */
	*(volatile u32 *)(RALINK_INTENA) = M_SURFBOARD_GLOBAL_INT;
	ramips_sync();
#endif // CONFIG_IRQ_GIC //

#ifdef CONFIG_RALINK_GPIO
	ralink_gpio_init_irq();
#endif
	set_c0_status(IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5);
}

asmlinkage void rt_irq_dispatch(void)
{
	static unsigned int pci_order = 0;
	unsigned int irq = NR_IRQS, pending = read_c0_status() & read_c0_cause() & 0xFC00;
#if defined(CONFIG_RALINK_RT2880) || defined (CONFIG_RALINK_RT2883) || \
    defined(CONFIG_RALINK_RT3883) || defined(CONFIG_RALINK_RT6855) || \
    defined(CONFIG_RALINK_MT7620) || defined(CONFIG_RALINK_MT7628)
	unsigned long pci_status=0;
#endif

	pci_order^=1;

	if (pending & CAUSEF_IP6)
		irq = 4;
	else if (pending & CAUSEF_IP5)
		irq = 3;
	else if (pending & CAUSEF_IP4) {
#if defined (CONFIG_RALINK_RT2883)
		irq = 2;
#elif defined (CONFIG_RALINK_RT3883)
		pci_status = RALINK_PCI_PCIINT_ADDR;
		if (pci_status & 0x100000) {
			irq = 16;
		} else if (pci_status & 0x40000) {
			irq = 2;
		} else {
			irq = 15;
		}
#elif defined (CONFIG_RALINK_RT3052)
#elif defined (CONFIG_RALINK_RT3352)
#elif defined (CONFIG_RALINK_RT5350)
#elif defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
		pci_status = RALINK_PCI_PCIINT_ADDR;
		if(pci_status &(1<<20)){
			irq = RALINK_INT_PCIE0;
		}
#elif defined (CONFIG_RALINK_MT7621)
				/* do nothing */

#elif defined (CONFIG_RALINK_RT6855)
		pci_status = RALINK_PCI_PCIINT_ADDR;
		if(pci_order==0){
			if(pci_status &(1<<20)){
				//PCIe0
				irq = RALINK_INT_PCIE0;
			}else if(pci_status &(1<<21)){
				//PCIe1
				irq = RALINK_INT_PCIE1;
			}else{
				//printk("pcie inst = %x\n", (unsigned int)pci_status);
			}
		}else{
			if(pci_status &(1<<21)){
				//PCIe1
				irq = RALINK_INT_PCIE1;
			}else if(pci_status &(1<<20)){
				//PCIe0
				irq = RALINK_INT_PCIE0;
			}else{
				//printk("pcie inst = %x\n", (unsigned int)pci_status);
			}
		}
#else // 2880
#if defined(CONFIG_RALINK_RT2880) || defined(CONFIG_RALINK_RT3883)
		pci_status = RALINK_PCI_PCIINT_ADDR;
#endif
		if (pci_order == 0) {
			if (pci_status & PCI_STATUS_MASK1)
				irq = 2;
			else
				irq = 15;
		} else {
			if (pci_status & PCI_STATUS_MASK2)
				irq = 15;
			else
				irq = 2;
		}
#endif /* CONFIG_RALINK_RT2883 */
	}
	else if (pending & CAUSEF_IP3)
		surfboard_hw0_irqdispatch(1);
	else if (pending & CAUSEF_IP2)
		surfboard_hw0_irqdispatch(0);
	else
		spurious_interrupt();

	if (likely(irq < NR_IRQS))
		do_IRQ(irq);

	return;
}

asmlinkage void plat_irq_dispatch(void)
{
        unsigned int pending = read_c0_status() & read_c0_cause() & ST0_IM;

	if(pending & CAUSEF_IP7 ){
		do_IRQ(cp0_compare_irq);
	}
#ifdef CONFIG_MIPS_MT_SMP
	else if (pending & (CAUSEF_IP4 | CAUSEF_IP3)) {
		mips1004k_ipi_irqdispatch();
	}
#endif
	else {
		rt_irq_dispatch();
	}
}

