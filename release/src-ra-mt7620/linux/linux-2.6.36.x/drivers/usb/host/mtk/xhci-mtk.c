#include "xhci-mtk.h"
#include "xhci-mtk-power.h"
#include "xhci.h"
#include "mtk-phy.h"
#ifdef CONFIG_C60802_SUPPORT
#include "mtk-phy-c60802.h"
#endif
#include "xhci-mtk-scheduler.h"
#include <linux/kernel.h>       /* printk() */
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

void setInitialReg(){
	__u32 __iomem *addr;
	u32 temp;

#if defined (CONFIG_MT7621_FPGA)
	//set MAC reference clock speed
	addr = SSUSB_U3_MAC_BASE+U3_UX_EXIT_LFPS_TIMING_PAR;
	temp = readl(addr);
	temp &= ~(0xff << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
	temp |= (U3_RX_UX_EXIT_LFPS_REF << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
	writel(temp, addr);
	addr = SSUSB_U3_MAC_BASE+U3_REF_CK_PAR;
	temp = readl(addr);
	temp &= ~(0xff);
	temp |= U3_REF_CK_VAL;
	writel(temp, addr);

	//set SYS_CK
	addr = SSUSB_U3_SYS_BASE+U3_TIMING_PULSE_CTRL;
	temp = readl(addr);
	temp &= ~(0xff);
	temp |= CNT_1US_VALUE;
	writel(temp, addr);
	addr = SSUSB_U2_SYS_BASE+USB20_TIMING_PARAMETER;
	temp &= ~(0xff);
	temp |= TIME_VALUE_1US;
	writel(temp, addr);

	//set LINK_PM_TIMER=3
	addr = SSUSB_U3_SYS_BASE+LINK_PM_TIMER;
	temp = readl(addr);
	temp &= ~(0xf);
	temp |= PM_LC_TIMEOUT_VALUE;
	writel(temp, addr);
#elif defined (CONFIG_MT7621_ASIC)
	/* set SSUSB DMA burst size to 128B */
	addr = SSUSB_U3_XHCI_BASE + SSUSB_HDMA_CFG;
	temp = SSUSB_HDMA_CFG_MT7621_VALUE;
	writel(temp, addr);

	/* extend U3 LTSSM Polling.LFPS timeout value */
	addr = SSUSB_U3_XHCI_BASE + U3_LTSSM_TIMING_PARAMETER3;
	temp = U3_LTSSM_TIMING_PARAMETER3_VALUE;
	writel(temp, addr);

	/* EOF */
	addr = SSUSB_U3_XHCI_BASE + SYNC_HS_EOF;
	temp = SYNC_HS_EOF_VALUE;
	writel(temp, addr);

#if defined (CONFIG_PERIODIC_ENP)
	/* HSCH_CFG1: SCH2_FIFO_DEPTH */
	addr = SSUSB_U3_XHCI_BASE + HSCH_CFG1;
	temp = readl(addr);
	temp &= ~(0x3 << SCH2_FIFO_DEPTH_OFFSET);
	writel(temp, addr);
#endif

	/* Doorbell handling */
	addr = SIFSLV_IPPC + SSUSB_IP_SPAR0;
	temp = 0x1;
	writel(temp, addr);

	/* Set SW PLL Stable mode to 1 for U2 LPM device remote wakeup */
	/* Port 0 */
	addr = U2_PHY_BASE + U2_PHYD_CR1;
	temp = readl(addr);
	temp &= ~(0x3 << 18);
	temp |= (1 << 18);
	writel(temp, addr);

	/* Port 1 */
	addr = U2_PHY_BASE_P1 + U2_PHYD_CR1;
	temp = readl(addr);
	temp &= ~(0x3 << 18);
	temp |= (1 << 18);
	writel(temp, addr);

#endif
}


void setLatchSel(void){
	__u32 __iomem *latch_sel_addr;
	u32 latch_sel_value;
	latch_sel_addr = U3_PIPE_LATCH_SEL_ADD;
	latch_sel_value = ((U3_PIPE_LATCH_TX)<<2) | (U3_PIPE_LATCH_RX);
	writel(latch_sel_value, latch_sel_addr);
}

void reinitIP(void){
	__u32 __iomem *ip_reset_addr;
	u32 ip_reset_value;

	enableAllClockPower();

#ifdef CONFIG_MT7621_FPGA
	setLatchSel();
#endif
	mtk_xhci_scheduler_init();
#if PERF_PROBE
	mtk_probe_init(0x38383838);
	mtk_probe_out(0x0);
#endif
#if WEB_CAM_PROBE
	mtk_probe_init(0x70707070);
	mtk_probe_out(0x0);
#endif
}

void dbg_prb_out(void){
	mtk_probe_init(0x0f0f0f0f);
	mtk_probe_out(0xffffffff);
	mtk_probe_out(0x01010101);
	mtk_probe_out(0x02020202);
	mtk_probe_out(0x04040404);
	mtk_probe_out(0x08080808);
	mtk_probe_out(0x10101010);
	mtk_probe_out(0x20202020);
	mtk_probe_out(0x40404040);
	mtk_probe_out(0x80808080);
	mtk_probe_out(0x55555555);
	mtk_probe_out(0xaaaaaaaa);
}



///////////////////////////////////////////////////////////////////////////////

#define RET_SUCCESS 0
#define RET_FAIL 1

static int dbg_u3w(int argc, char**argv)
{
	int u4TimingValue;
	char u1TimingValue;
	int u4TimingAddress;

	if (argc<3)
    {
        printk(KERN_ERR "Arg: address value\n");
        return RET_FAIL;
    }
	u3phy_init();
	
	u4TimingAddress = (int)simple_strtol(argv[1], &argv[1], 16);
	u4TimingValue = (int)simple_strtol(argv[2], &argv[2], 16);
	u1TimingValue = u4TimingValue & 0xff;
#ifdef CONFIG_MT7621_ASIC
/* access MMIO directly */
	writel(u1TimingValue, u4TimingAddress);
#else
/* access through I2C or GPIO window */
	_U3Write_Reg(u4TimingAddress, u1TimingValue);
#endif
	printk(KERN_ERR "Write done\n");
	return RET_SUCCESS;
	
}

static int dbg_u3r(int argc, char**argv)
{
	char u1ReadTimingValue;
	int u4TimingAddress;
	if (argc<2)
    {
        printk(KERN_ERR "Arg: address\n");
        return 0;
    }
	u3phy_init();
	mdelay(500);
	u4TimingAddress = (int)simple_strtol(argv[1], &argv[1], 16);
#ifdef CONFIG_MT7621_ASIC
/* access MMIO directly */
	u1ReadTimingValue = readl(u4TimingAddress);
#else
/* access through I2C or GPIO window */
	u1ReadTimingValue = _U3Read_Reg(u4TimingAddress);
#endif
	printk(KERN_ERR "Value = 0x%x\n", u1ReadTimingValue);
	return 0;
}

static int dbg_u3init(int argc, char**argv)
{
	int ret;
	ret = u3phy_init();
	printk(KERN_ERR "phy registers and operations initial done\n");
	if(u3phy_ops->u2_slew_rate_calibration){
		u3phy_ops->u2_slew_rate_calibration(u3phy);
	}
	else{
		printk(KERN_ERR "WARN: PHY doesn't implement u2 slew rate calibration function\n");
	}
	if(u3phy_ops->init(u3phy) == PHY_TRUE)
		return RET_SUCCESS;
	return RET_FAIL;
}

void dbg_setU1U2(int argc, char**argv){
	struct xhci_hcd *xhci;
	int u1_value;
	int u2_value;
	u32 port_id, temp;
	u32 __iomem *addr;
	
	if (argc<3)
    {
        printk(KERN_ERR "Arg: u1value u2value\n");
        return RET_FAIL;
    }

	u1_value = (int)simple_strtol(argv[1], &argv[1], 10);
	u2_value = (int)simple_strtol(argv[2], &argv[2], 10);
	addr = (SSUSB_U3_XHCI_BASE + 0x424);
	temp = readl(addr);
	temp = temp & (~(0x0000ffff));
	temp = temp | u1_value | (u2_value<<8);
	writel(temp, addr);
}
///////////////////////////////////////////////////////////////////////////////

int call_function(char *buf)
{
	int i;
	int argc;
	char *argv[80];

	argc = 0;
	do
	{
		argv[argc] = strsep(&buf, " ");
		printk(KERN_DEBUG "[%d] %s\r\n", argc, argv[argc]);
		argc++;
	} while (buf);
	if (!strcmp("dbg.r", argv[0]))
		dbg_prb_out();
	else if (!strcmp("dbg.u3w", argv[0]))
		dbg_u3w(argc, argv);
	else if (!strcmp("dbg.u3r", argv[0]))
		dbg_u3r(argc, argv);
	else if (!strcmp("dbg.u3i", argv[0]))
		dbg_u3init(argc, argv);
	else if (!strcmp("pw.u1u2", argv[0]))
		dbg_setU1U2(argc, argv);
	return 0;
}

char w_buf[200];
char r_buf[200] = "this is a test";

int xhci_mtk_test_unlock_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

    int len = 200;

	switch (cmd) {
		case IOCTL_READ:
			copy_to_user((char *) arg, r_buf, len);
			printk(KERN_DEBUG "IOCTL_READ: %s\r\n", r_buf);
			break;
		case IOCTL_WRITE:
			copy_from_user(w_buf, (char *) arg, len);
			printk(KERN_DEBUG "IOCTL_WRITE: %s\r\n", w_buf);

			//invoke function
			return call_function(w_buf);
			break;
		default:
			return -ENOTTY;
	}

	return len;
}

int xhci_mtk_test_open(struct inode *inode, struct file *file)
{

    printk(KERN_DEBUG "xhci_mtk_test open: successful\n");
    return 0;
}

int xhci_mtk_test_release(struct inode *inode, struct file *file)
{

    printk(KERN_DEBUG "xhci_mtk_test release: successful\n");
    return 0;
}

ssize_t xhci_mtk_test_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{

    printk(KERN_DEBUG "xhci_mtk_test read: returning zero bytes\n");
    return 0;
}

ssize_t xhci_mtk_test_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{

    printk(KERN_DEBUG "xhci_mtk_test write: accepting zero bytes\n");
    return 0;
}




