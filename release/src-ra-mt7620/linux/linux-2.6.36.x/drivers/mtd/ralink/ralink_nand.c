#define DEBUG
#include <linux/device.h>
#undef DEBUG
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "ralink_nand.h"
#include "../maps/ralink-flash.h"

#define BLOCK_ALIGNED(a) ((a) & (CFG_BLOCKSIZE - 1))
#define READ_STATUS_RETRY	1000

static int nand_do_write_ops(struct ra_nand_chip *ra, loff_t to, struct mtd_oob_ops *ops);
int nfc_read_page(struct ra_nand_chip *ra, unsigned char *buf, int page, int flags);
int nfc_write_page(struct ra_nand_chip *ra, unsigned char *buf, int page, int flags);

struct mtd_info *ranfc_mtd = NULL;

int skipbbt = 0;
int ranfc_debug = 1;
static int ranfc_bbt = 1;
static int ranfc_verify = 1;

module_param(ranfc_debug, int, 0644);
module_param(ranfc_bbt, int, 0644);
module_param(ranfc_verify, int, 0644);

//#define ra_dbg(args...) do { if (ranfc_debug) printk(args); } while(0)
#define ra_dbg(args...)

#define CLEAR_INT_STATUS()	ra_outl(NFC_INT_ST, ra_inl(NFC_INT_ST))
#define NFC_TRANS_DONE()	(ra_inl(NFC_INT_ST) & INT_ST_ND_DONE)

/*
*	Chip ID list
*
*	Name. ID code, pagesize, chipsize in MegaByte, eraseblock size,
*	options
*
*	Pagesize; 0, 256, 512
*	0	get this information from the extended chip ID
+	256	256 Byte page size
*	512	512 Byte page size
*/
struct nand_flash_dev nand_flash_ids[] = {

#ifdef CONFIG_MTD_NAND_MUSEUM_IDS
	{"NAND 1MiB 5V 8-bit",		0x6e, 256, 1, 0x1000, 0},
	{"NAND 2MiB 5V 8-bit",		0x64, 256, 2, 0x1000, 0},
	{"NAND 4MiB 5V 8-bit",		0x6b, 512, 4, 0x2000, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xe8, 256, 1, 0x1000, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xec, 256, 1, 0x1000, 0},
	{"NAND 2MiB 3,3V 8-bit",	0xea, 256, 2, 0x1000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xd5, 512, 4, 0x2000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe3, 512, 4, 0x2000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe5, 512, 4, 0x2000, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xd6, 512, 8, 0x2000, 0},

	{"NAND 8MiB 1,8V 8-bit",	0x39, 512, 8, 0x2000, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xe6, 512, 8, 0x2000, 0},
	{"NAND 8MiB 1,8V 16-bit",	0x49, 512, 8, 0x2000, NAND_BUSWIDTH_16},
	{"NAND 8MiB 3,3V 16-bit",	0x59, 512, 8, 0x2000, NAND_BUSWIDTH_16},
#endif

	{"NAND 16MiB 1,8V 8-bit",	0x33, 512, 16, 0x4000, 0},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 512, 16, 0x4000, 0},
	{"NAND 16MiB 1,8V 16-bit",	0x43, 512, 16, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 16MiB 3,3V 16-bit",	0x53, 512, 16, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 32MiB 1,8V 8-bit",	0x35, 512, 32, 0x4000, 0},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 512, 32, 0x4000, 0},
	{"NAND 32MiB 1,8V 16-bit",	0x45, 512, 32, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 32MiB 3,3V 16-bit",	0x55, 512, 32, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 512, 64, 0x4000, 0},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 512, 64, 0x4000, 0},
	{"NAND 64MiB 1,8V 16-bit",	0x46, 512, 64, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 64MiB 3,3V 16-bit",	0x56, 512, 64, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 512, 128, 0x4000, 0},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 512, 128, 0x4000, 0},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 512, 128, 0x4000, 0},
	{"NAND 128MiB 1,8V 16-bit",	0x72, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 1,8V 16-bit",	0x49, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 3,3V 16-bit",	0x74, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 3,3V 16-bit",	0x59, 512, 128, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 512, 256, 0x4000, 0},

	/*
	 * These are the new chips with large page size. The pagesize and the
	 * erasesize is determined from the extended id bytes
	 */
#define LP_OPTIONS (NAND_SAMSUNG_LP_OPTIONS | NAND_NO_READRDY | NAND_NO_AUTOINCR)
#define LP_OPTIONS16 (LP_OPTIONS | NAND_BUSWIDTH_16)

	/*512 Megabit */
	{"NAND 64MiB 1,8V 8-bit",	0xA2, 0,  64, 0, LP_OPTIONS},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 0,  64, 0, LP_OPTIONS},
	{"NAND 64MiB 1,8V 16-bit",	0xB2, 0,  64, 0, LP_OPTIONS16},
	{"NAND 64MiB 3,3V 16-bit",	0xC2, 0,  64, 0, LP_OPTIONS16},

	/* 1 Gigabit */
	{"NAND 128MiB 1,8V 8-bit",	0xA1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xF1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xD1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 1,8V 16-bit",	0xB1, 0, 128, 0, LP_OPTIONS16},
	{"NAND 128MiB 3,3V 16-bit",	0xC1, 0, 128, 0, LP_OPTIONS16},
	{"NAND 128MiB 1,8V 16-bit",     0xAD, 0, 128, 0, LP_OPTIONS16},

	/* 2 Gigabit */
	{"NAND 256MiB 1,8V 8-bit",	0xAA, 0, 256, 0, LP_OPTIONS},
	{"NAND 256MiB 3,3V 8-bit",	0xDA, 0, 256, 0, LP_OPTIONS},
	{"NAND 256MiB 1,8V 16-bit",	0xBA, 0, 256, 0, LP_OPTIONS16},
	{"NAND 256MiB 3,3V 16-bit",	0xCA, 0, 256, 0, LP_OPTIONS16},

	/* 4 Gigabit */
	{"NAND 512MiB 1,8V 8-bit",	0xAC, 0, 512, 0, LP_OPTIONS},
	{"NAND 512MiB 3,3V 8-bit",	0xDC, 0, 512, 0, LP_OPTIONS},
	{"NAND 512MiB 1,8V 16-bit",	0xBC, 0, 512, 0, LP_OPTIONS16},
	{"NAND 512MiB 3,3V 16-bit",	0xCC, 0, 512, 0, LP_OPTIONS16},

	/* 8 Gigabit */
	{"NAND 1GiB 1,8V 8-bit",	0xA3, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 3,3V 8-bit",	0xD3, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 1,8V 16-bit",	0xB3, 0, 1024, 0, LP_OPTIONS16},
	{"NAND 1GiB 3,3V 16-bit",	0xC3, 0, 1024, 0, LP_OPTIONS16},

	/* 16 Gigabit */
	{"NAND 2GiB 1,8V 8-bit",	0xA5, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 3,3V 8-bit",	0xD5, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 1,8V 16-bit",	0xB5, 0, 2048, 0, LP_OPTIONS16},
	{"NAND 2GiB 3,3V 16-bit",	0xC5, 0, 2048, 0, LP_OPTIONS16},

	/* 32 Gigabit */
	{"NAND 4GiB 3,3V 8-bit",	0xD7, 0, 4096, 0, LP_OPTIONS},

	/*
	 * Renesas AND 1 Gigabit. Those chips do not support extended id and
	 * have a strange page/block layout !  The chosen minimum erasesize is
	 * 4 * 2 * 2048 = 16384 Byte, as those chips have an array of 4 page
	 * planes 1 block = 2 pages, but due to plane arrangement the blocks
	 * 0-3 consists of page 0 + 4,1 + 5, 2 + 6, 3 + 7 Anyway JFFS2 would
	 * increase the eraseblock size so we chose a combined one which can be
	 * erased in one go There are more speed improvements for reads and
	 * writes possible, but not implemented now
	 */
	{"AND 128MiB 3,3V 8-bit",	0x01, 2048, 128, 0x4000,
	 NAND_IS_AND | NAND_NO_AUTOINCR |NAND_NO_READRDY | NAND_4PAGE_ARRAY |
	 BBT_AUTO_REFRESH
	},

	{NULL,}
};

/*
*	Manufacturer ID list
*/
struct nand_manufacturers nand_manuf_ids[] = {
	{NAND_MFR_TOSHIBA, "Toshiba"},
	{NAND_MFR_SAMSUNG, "Samsung"},
	{NAND_MFR_FUJITSU, "Fujitsu"},
	{NAND_MFR_NATIONAL, "National"},
	{NAND_MFR_RENESAS, "Renesas"},
	{NAND_MFR_STMICRO, "ST Micro"},
	{NAND_MFR_HYNIX, "Hynix"},
	{NAND_MFR_MICRON, "Micron"},
	{NAND_MFR_AMD, "AMD/Spansion"},
	{NAND_MFR_ZENTEL, "Zentel"},
	{0x0, "Unknown manufacturer"}
};

int nand_addrlen = 5;
int is_nand_page_2048 = 0;
const unsigned int nand_size_map[2][3] = {{25, 30, 30}, {20, 27, 30}};

#define	MTD_JFFS2_PART_SIZE	0x80000
static struct mtd_partition rt2880_partitions[] = {
#ifdef CONFIG_MTD_UBI
        {
                name:           "Bootloader",
                size:           MTD_BOOT_PART_SIZE,
                offset:         0,
        }, {
                name:           "UBI_DEV",
                size:           0,
                offset:         0,
        }
#else
        {
                name:           "Bootloader",
                size:           MTD_BOOT_PART_SIZE,
                offset:         0,
        }, {
                name:           "nvram",
                size:           MTD_CONFIG_PART_SIZE,
                offset:         MTDPART_OFS_APPEND
        }, {
                name:           "Factory",
                size:           MTD_FACTORY_PART_SIZE,
                offset:         MTDPART_OFS_APPEND
        }, {
                name:           "linux",
                size:           MTD_KERN_PART_SIZE,
                offset:         MTDPART_OFS_APPEND,
        }, {
                name:           "rootfs",
                size:           MTD_ROOTFS_PART_SIZE,
                offset:         MTDPART_OFS_APPEND,
        }, {
                name:           "jffs2",
                size:           MTD_JFFS2_PART_SIZE,
                offset:         MTDPART_OFS_APPEND,
        }, {
                name:           "ALL",
                size:           MTDPART_SIZ_FULL,
                offset:         0,
        }
#endif
};


/*************************************************************
 * nfc functions 
 *************************************************************/
static int nfc_wait_ready(int snooze_ms);
/**
 * reset nand chip
 */
static int nfc_chip_reset(void)
{
	int status;

	//ra_dbg("%s:\n", __func__);
	// reset nand flash
	ra_outl(NFC_CMD1, 0x0);
	ra_outl(NFC_CMD2, 0xff);
	ra_outl(NFC_ADDR, 0x0);
	ra_outl(NFC_CONF, 0x0411);

	status = nfc_wait_ready(5);  //erase wait 5us
	if (status & NAND_STATUS_FAIL) {
		printk("%s: fail \n", __func__);
	}
	
	return (int)(status & NAND_STATUS_FAIL);
}


/** 
 * clear NFC and flash chip.
 */
static int nfc_all_reset(void)
{
	int retry;

	ra_dbg("%s: \n", __func__);
	// reset controller
	ra_outl(NFC_CTRL, ra_inl(NFC_CTRL) | 0x02); //clear data buffer
	ra_outl(NFC_CTRL, ra_inl(NFC_CTRL) & ~0x02); //clear data buffer

	CLEAR_INT_STATUS();

	retry = READ_STATUS_RETRY;
	while ((ra_inl(NFC_INT_ST) & 0x02) != 0x02 && retry--);
	if (retry <= 0) {
		printk("nfc_all_reset: clean buffer fail \n");
		return -1;
	}

	retry = READ_STATUS_RETRY;
	while ((ra_inl(NFC_STATUS) & 0x1) != 0x0 && retry--) { //fixme, controller is busy ?
		udelay(1);
	}

	nfc_chip_reset();

	return 0;
}

/** NOTICE: only called by nfc_wait_ready().
 * @return -1, nfc can not get transction done 
 * @return 0, ok.
 */
static int _nfc_read_status(char *status)
{
	unsigned long cmd1, conf;
	int int_st, nfc_st;
	int retry;

	cmd1 = 0x70;
	conf = 0x000101 | (1 << 20);

	//fixme, should we check nfc status?
	CLEAR_INT_STATUS();

	ra_outl(NFC_CMD1, cmd1); 	
	ra_outl(NFC_CONF, conf); 

	/* FIXME, 
	 * 1. since we have no wired ready signal, directly 
	 * calling this function is not gurantee to read right status under ready state.
	 * 2. the other side, we can not determine how long to become ready, this timeout retry is nonsense.
	 * 3. SUGGESTION: call nfc_read_status() from nfc_wait_ready(),
	 * that is aware about caller (in sementics) and has snooze plused nfc ND_DONE.
	 */
	retry = READ_STATUS_RETRY; 
	do {
		nfc_st = ra_inl(NFC_STATUS);
		int_st = ra_inl(NFC_INT_ST);
		
		ndelay(10);
	} while (!(int_st & INT_ST_RX_BUF_RDY) && retry--);

	if (!(int_st & INT_ST_RX_BUF_RDY)) {
		printk("nfc_read_status: NFC fail, int_st(%x), retry:%x. nfc:%x, reset nfc and flash. \n", 
		       int_st, retry, nfc_st);
		nfc_all_reset();
		*status = NAND_STATUS_FAIL;
		return -1;
	}

	*status = (char)(le32_to_cpu(ra_inl(NFC_DATA)) & 0x0ff);
	return 0;
}

/**
 * @return !0, chip protect.
 * @return 0, chip not protected.
 */
static int nfc_check_wp(void)
{
	/* Check the WP bit */
#if !defined CONFIG_NOT_SUPPORT_WP
	return !!(ra_inl(NFC_CTRL) & 0x01);
#else
	char result = 0;
	int ret;

	ret = _nfc_read_status(&result);
	//FIXME, if ret < 0

	return !(result & NAND_STATUS_WP);
#endif
}

#if !defined CONFIG_NOT_SUPPORT_RB
/*
 * @return !0, chip ready.
 * @return 0, chip busy.
 */
static int nfc_device_ready(void)
{
	/* Check the ready  */
	return !!(ra_inl(NFC_STATUS) & 0x04);
}
#endif


/**
 * generic function to get data from flash.
 * @return data length reading from flash.
 */
static int _ra_nand_pull_data(char *buf, int len, int use_gdma)
{
#ifdef RW_DATA_BY_BYTE
	char *p = buf;
#else
	__u32 *p = (__u32 *)buf;
#endif
	int retry, int_st;
	unsigned int ret_data;
	int ret_size;

	// receive data by use_gdma 
	if (use_gdma) { 
		//if (_ra_nand_dma_pull((unsigned long)p, len)) {
		if (1) {
			printk("%s: fail \n", __func__);
			len = -1; //return error
		}

		return len;
	}

	//fixme: retry count size?
	retry = READ_STATUS_RETRY;
	// no gdma
	while (len > 0) {
		int_st = ra_inl(NFC_INT_ST);
		if (int_st & INT_ST_RX_BUF_RDY) {

			ret_data = ra_inl(NFC_DATA);
			ra_outl(NFC_INT_ST, INT_ST_RX_BUF_RDY); 
#ifdef RW_DATA_BY_BYTE
			ret_size = sizeof(unsigned int);
			ret_size = min(ret_size, len);
			len -= ret_size;
			while (ret_size-- > 0) {
				//nfc is little endian 
				*p++ = ret_data & 0x0ff;
				ret_data >>= 8; 
			}
#else
			ret_size = min(len, 4);
			len -= ret_size;
			if (ret_size == 4)
				*p++ = ret_data;
			else {
				__u8 *q = (__u8 *)p;
				while (ret_size-- > 0) {
					*q++ = ret_data & 0x0ff;
					ret_data >>= 8; 
				}
				p = (__u32 *)q;
			}
#endif
			retry = READ_STATUS_RETRY;
		}
		else if (int_st & INT_ST_ND_DONE) {
			break;
		}
		else {
			udelay(1);
			if (retry-- < 0) 
				break;
		}
	}

#ifdef RW_DATA_BY_BYTE
	return (int)(p - buf);
#else
	return ((int)p - (int)buf);
#endif
}

/**
 * generic function to put data into flash.
 * @return data length writing into flash.
 */
static int _ra_nand_push_data(char *buf, int len, int use_gdma)
{
#ifdef RW_DATA_BY_BYTE
	char *p = buf;
#else
	__u32 *p = (__u32 *)buf;
#endif
	int retry, int_st;
	unsigned int tx_data = 0;
	int tx_size, iter = 0;

	// receive data by use_gdma 
	if (use_gdma) { 
		//if (_ra_nand_dma_push((unsigned long)p, len))
		if (1)
			len = 0;		
		printk("%s: fail \n", __func__);
		return len;
	}

	// no gdma
	retry = READ_STATUS_RETRY;
	while (len > 0) {
		int_st = ra_inl(NFC_INT_ST);
		if (int_st & INT_ST_TX_BUF_RDY) {
#ifdef RW_DATA_BY_BYTE
			tx_size = min(len, (int)sizeof(unsigned long));
			for (iter = 0; iter < tx_size; iter++) {
				tx_data |= (*p++ << (8*iter));
			}
#else
			tx_size = min(len, 4);
			if (tx_size == 4)
				tx_data = (*p++);
			else {
				__u8 *q = (__u8 *)p;
				for (iter = 0; iter < tx_size; iter++)
					tx_data |= (*q++ << (8*iter));
				p = (__u32 *)q;
			}
#endif
			ra_outl(NFC_INT_ST, INT_ST_TX_BUF_RDY);
			ra_outl(NFC_DATA, tx_data);
			len -= tx_size;
			retry = READ_STATUS_RETRY;
		}
		else if (int_st & INT_ST_ND_DONE) {
			break;
		}
		else {
			udelay(1);
			if (retry-- < 0) {
				ra_dbg("%s p:%p buf:%p \n", __func__, p, buf);
				break;
			}
		}
	}

	
#ifdef RW_DATA_BY_BYTE
	return (int)(p - buf);
#else
	return ((int)p - (int)buf);
#endif

}

static int nfc_select_chip(struct ra_nand_chip *ra, int chipnr)
{
#if (CONFIG_NUMCHIPS == 1)
	if (!(chipnr < CONFIG_NUMCHIPS))
		return -1;
	return 0;
#else
	BUG();
#endif
}

/** @return -1: chip_select fail
 *	    0 : both CE and WP==0 are OK
 * 	    1 : CE OK and WP==1
 */
static int nfc_enable_chip(struct ra_nand_chip *ra, unsigned int offs, int read_only)
{
	int chipnr = offs >> ra->chip_shift;

	ra_dbg("%s: offs:%x read_only:%x \n", __func__, offs, read_only);
	chipnr = nfc_select_chip(ra, chipnr);
	if (chipnr < 0) {
		printk("%s: chip select error, offs(%x)\n", __func__, offs);
		return -1;
	}

	if (!read_only)
		return nfc_check_wp();
	
	return 0;
}

/** wait nand chip becomeing ready and return queried status.
 * @param snooze: sleep time in ms unit before polling device ready.
 * @return status of nand chip
 * @return NAN_STATUS_FAIL if something unexpected.
 */
static int nfc_wait_ready(int snooze_ms)
{
	int retry;
	char status;

	// wait nfc idle,
	if (snooze_ms == 0)
		snooze_ms = 1;
	else
		schedule_timeout(snooze_ms * HZ / 1000);
	
	snooze_ms = retry = snooze_ms *1000000 / 100 ;  // ndelay(100)

	while (!NFC_TRANS_DONE() && retry--) {
		if (!cond_resched())
			ndelay(100);
	}
	
	if (!NFC_TRANS_DONE()) {
		printk("nfc_wait_ready: no transaction done \n");
		return NAND_STATUS_FAIL;
	}

#if !defined (CONFIG_NOT_SUPPORT_RB)
	//fixme
	while(!(status = nfc_device_ready()) && retry--) {
		ndelay(100);
	}

	if (status == 0) {
		printk("nfc_wait_ready: no device ready. \n");	
		return NAND_STATUS_FAIL;
	}

	_nfc_read_status(&status);
	return status;
#else

	while(retry--) {
		_nfc_read_status(&status);
		if (status & NAND_STATUS_READY)
			break;
		ndelay(100);
	}
	if (retry<0)
		printk("nfc_wait_ready 2: no device ready, status(%x). \n", status);	

	return status;
#endif
}

/**
 * return 0: erase OK
 * return -EIO: fail 
 */
int nfc_erase_block(struct ra_nand_chip *ra, int row_addr)
{
	unsigned long cmd1, cmd2, bus_addr, conf;
	char status;

	cmd1 = 0x60;
	cmd2 = 0xd0;
	bus_addr = row_addr;
	conf = 0x00511 | ((CFG_ROW_ADDR_CYCLE)<<16);

	// set NFC
	ra_dbg("%s: cmd1: %lx, cmd2:%lx bus_addr: %lx, conf: %lx \n", 
	       __func__, cmd1, cmd2, bus_addr, conf);
	//fixme, should we check nfc status?
	CLEAR_INT_STATUS();

	ra_outl(NFC_CMD1, cmd1); 	
	ra_outl(NFC_CMD2, cmd2);
	ra_outl(NFC_ADDR, bus_addr);
	ra_outl(NFC_CONF, conf); 

	status = nfc_wait_ready(3);  //erase wait 3ms 
	if (status & NAND_STATUS_FAIL) {
		printk("%s: fail \n", __func__);
		return -EIO;
	}
	
	return 0;
}

static inline int _nfc_read_raw_data(int cmd1, int cmd2, int bus_addr, int bus_addr2, int conf, char *buf, int len, int flags)
{
	int ret;

	CLEAR_INT_STATUS();
	ra_outl(NFC_CMD1, cmd1); 	
	ra_outl(NFC_CMD2, cmd2);
	ra_outl(NFC_ADDR, bus_addr);
#if defined (CONFIG_RALINK_RT6855) || defined (CONFIG_RALINK_RT6855A) || \
    defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7621)	
	ra_outl(NFC_ADDR2, bus_addr2);
#endif	
	ra_outl(NFC_CONF, conf); 

	ret = _ra_nand_pull_data(buf, len, 0);
	if (ret != len) {
		ra_dbg("%s: ret:%x (%x) \n", __func__, ret, len);
		return NAND_STATUS_FAIL;
	}

	//FIXME, this section is not necessary
	ret = nfc_wait_ready(0); //wait ready 
	if (ret & NAND_STATUS_FAIL) {
		printk("%s: fail \n", __func__);
		return NAND_STATUS_FAIL;
	}

	return 0;
}

static inline int _nfc_write_raw_data(int cmd1, int cmd3, int bus_addr, int bus_addr2, int conf, char *buf, int len, int flags)
{
	int ret;

	CLEAR_INT_STATUS();
	ra_outl(NFC_CMD1, cmd1); 	
	ra_outl(NFC_CMD3, cmd3); 	
	ra_outl(NFC_ADDR, bus_addr);
#if defined (CONFIG_RALINK_RT6855) || defined (CONFIG_RALINK_RT6855A) || \
    defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7621)	
	ra_outl(NFC_ADDR2, bus_addr2);
#endif	
	ra_outl(NFC_CONF, conf); 

	ret = _ra_nand_push_data(buf, len, 0);
	if (ret != len) {
		ra_dbg("%s: ret:%x (%x) \n", __func__, ret, len);
		return NAND_STATUS_FAIL;
	}

	ret = nfc_wait_ready(1); //write wait 1ms
	if (ret & NAND_STATUS_FAIL) {
		printk("%s: fail \n", __func__);
		return NAND_STATUS_FAIL;
	}

	return 0;
}

/**
 * @return !0: fail
 * @return 0: OK
 */
int nfc_read_oob(struct ra_nand_chip *ra, int page, unsigned int offs, char *buf, int len, int flags)
{
	unsigned int cmd1 = 0, cmd2 = 0, conf = 0;
	unsigned int bus_addr = 0, bus_addr2 = 0;
	unsigned int ecc_en;
	int use_gdma;
	int status;
	int pages_perblock = 1<<(ra->erase_shift - ra->page_shift);
	// constrain of nfc read function 

	BUG_ON (offs >> ra->oob_shift); //page boundry
	BUG_ON ((unsigned int)(((offs + len) >> ra->oob_shift) + page) >
		((page + pages_perblock) & ~(pages_perblock-1))); //block boundry

	use_gdma = flags & FLAG_USE_GDMA;
	ecc_en = flags & FLAG_ECC_EN;
	bus_addr = (page << (CFG_COLUMN_ADDR_CYCLE*8)) | (offs & ((1<<CFG_COLUMN_ADDR_CYCLE*8) - 1));

	if (is_nand_page_2048) {
		bus_addr += CFG_PAGESIZE;
		bus_addr2 = page >> (CFG_COLUMN_ADDR_CYCLE*8);
		cmd1 = 0x0;
		cmd2 = 0x30;
		conf = 0x000511| ((CFG_ADDR_CYCLE)<<16) | (len << 20); 
	}
	else {
		cmd1 = 0x50;
		conf = 0x000141| ((CFG_ADDR_CYCLE)<<16) | (len << 20); 
	}
	if (ecc_en) 
		conf |= (1<<3); 
	if (use_gdma)
		conf |= (1<<2);

	ra_dbg("%s: cmd1:%x, bus_addr:%x, conf:%x, len:%x, flag:%x\n",
	       __func__, cmd1, bus_addr, conf, len, flags);
	status = _nfc_read_raw_data(cmd1, cmd2, bus_addr, bus_addr2, conf, buf, len, flags);
	if (status & NAND_STATUS_FAIL) {
		printk("%s: fail\n", __func__);
		return -EIO;
	}

	return 0; 
}

/**
 * @return !0: fail
 * @return 0: OK
 */
int nfc_write_oob(struct ra_nand_chip *ra, int page, unsigned int offs, char *buf, int len, int flags)
{
	unsigned int cmd1 = 0, cmd3=0, conf = 0;
	unsigned int bus_addr = 0, bus_addr2 = 0;
	int use_gdma;
	int status;
	int pages_perblock = 1<<(ra->erase_shift - ra->page_shift);
	// constrain of nfc read function 

	BUG_ON (offs >> ra->oob_shift); //page boundry
	BUG_ON ((unsigned int)(((offs + len) >> ra->oob_shift) + page) >
		((page + pages_perblock) & ~(pages_perblock-1))); //block boundry 

	use_gdma = flags & FLAG_USE_GDMA;
	bus_addr = (page << (CFG_COLUMN_ADDR_CYCLE*8)) | (offs & ((1<<CFG_COLUMN_ADDR_CYCLE*8) - 1));

	if (is_nand_page_2048) {
		cmd1 = 0x80;
		cmd3 = 0x10;
		bus_addr += CFG_PAGESIZE;
		bus_addr2 = page >> (CFG_COLUMN_ADDR_CYCLE*8);
		conf = 0x001123 | ((CFG_ADDR_CYCLE)<<16) | ((len) << 20);
	}
	else {
		cmd1 = 0x08050;
		cmd3 = 0x10;
		conf = 0x001223 | ((CFG_ADDR_CYCLE)<<16) | ((len) << 20); 
	}
	if (use_gdma)
		conf |= (1<<2);

	// set NFC
	ra_dbg("%s: cmd1: %x, cmd3: %x bus_addr: %x, conf: %x, len:%x\n", 
	       __func__, cmd1, cmd3, bus_addr, conf, len);

	status = _nfc_write_raw_data(cmd1, cmd3, bus_addr, bus_addr2, conf, buf, len, flags);
	if (status & NAND_STATUS_FAIL) {
		printk("%s: fail \n", __func__);
		return -EIO;
	}

	return 0; 
}

/* @return
 * 	number of bit is equal to 1
 */
static int bit_count(unsigned int val)
{
	int b = 0;

	while (val) {
		if ((val & 1) != 0)
			b++;

		val >>= 1;
	}

	return b;
}

int nfc_ecc_verify(struct ra_nand_chip *ra, unsigned char *buf, int page, int mode)
{
	int ret, i, j, bcnt = -1, byte_addr = -1, bit_addr = -1;
	int clean = 0, empty = 0, base;
	unsigned int corrected = 0, err = 0;
	unsigned int ecc, ecc_err;
	unsigned char *d, *p, *q, *data;
	unsigned char empty_ecc[CONFIG_ECC_BYTES], empty_data[4];
	unsigned char new_ecc[CONFIG_ECC_BYTES], ecc_xor[CONFIG_ECC_BYTES];
	struct mtd_info *mtd = ranfc_mtd;
	struct mtd_ecc_stats *ecc_stats = &mtd->ecc_stats;
	const int ecc_steps = mtd->writesize / 512;
	const char *dir = (mode == FL_READING)?"read":"write";

	//ra_dbg("%s, page:%x mode:%d\n", __func__, page, mode);
	if (mode == FL_WRITING) {
		int len = CFG_PAGESIZE + CFG_PAGE_OOBSIZE;
		int conf = 0x000141| ((CFG_ADDR_CYCLE)<<16) | (len << 20); 
		conf |= (1<<3); //(ecc_en) 
		//conf |= (1<<2); // (use_gdma)

		p = ra->readback_buffers;
		ret = nfc_read_page(ra, ra->readback_buffers, page, FLAG_ECC_EN); 
		if (ret >= 0 || ret == -EUCLEAN)
			goto ecc_check;
		
		//FIXME, double comfirm
		printk("%s: read back fail, try again \n",__func__);
		ret = nfc_read_page(ra, ra->readback_buffers, page, FLAG_ECC_EN); 
		if (ret >= 0 || ret == -EUCLEAN) {
			goto ecc_check;
		} else {
			printk("\t%s: read back fail agian \n",__func__);
			goto bad_block;
		}
	}
	else if (mode == FL_READING) {
		p = buf;
	}	
	else {
		printk("%s: mode %d, return -2\n", __func__, mode);
		return -2;
	}

ecc_check:
	data = p;
	p += (1 << ra->page_shift);
	memset(empty_ecc, 0xFF, sizeof(empty_ecc));
	for (i = 0, base = 0, q = p + CONFIG_ECC_OFFSET;
		i < ecc_steps;
		++i, base += 512, data += 512, p += 512, q += 16)
	{
		if (!i) {
			for (j = 0, clean = ecc_steps; clean == ecc_steps && j < ecc_steps; ++j) {
				if (ra_inl(NFC_ECC + 4*i) != 0)	/* not clean page */
					clean--;
			}
			if (clean == ecc_steps)
				break;

			for (j = 0, empty = ecc_steps; empty == ecc_steps && j < ecc_steps; ++j) {
				if (memcmp(q + 16 * j, empty_ecc, CONFIG_ECC_BYTES))
					empty--;
			}
			if (empty == ecc_steps) {
				/* If all ECC in a page is equal to 0xFFFFFF, check data again.
				 * ECC of some data is equal to 0xFFFFFF too.
				 */
				memset(empty_data, 0xFF, sizeof(empty_data));
				for (j = 0, empty = 1, d = data;
					empty && j < mtd->writesize;
					j+=sizeof(empty_data), d+=sizeof(empty_data))
				{
					if (memcmp(d, empty_data, sizeof(empty_data)))
						empty = 0;
				}
				if (empty) {
					printk("skip ecc 0xff at page %x\n", page);
					break;
				}
			}
		}

		ecc = ra_inl(NFC_ECC_P1 + 4*i);
		new_ecc[0] = ecc & 0xFF;
		new_ecc[1] = (ecc >> 8) & 0xFF;
		new_ecc[2] = (ecc >> 16) & 0xFF;
		if (!memcmp(q, new_ecc, CONFIG_ECC_BYTES))
			continue;

		ecc_xor[0] = *q ^ new_ecc[0];
		ecc_xor[1] = *(q + 1) ^ new_ecc[1];
		ecc_xor[2] = *(q + 2) ^ new_ecc[2];
		bcnt = bit_count(ecc_xor[2] << 16 | ecc_xor[1] << 8 | ecc_xor[0]);
		ecc_err = ra_inl(NFC_ECC_ERR1 + 4*i);

		printk("%s mode:%s, invalid ecc, page: %x, sub-page %d, "
			"read:%02x %02x %02x, ecc:%02x %02x %02x, xor: %02x %02x %02x, bcnt %d \n",
			__func__, dir, page, i, *(q), *(q + 1), *(q + 2),
			new_ecc[0], new_ecc[1], new_ecc[2], ecc_xor[0], ecc_xor[1], ecc_xor[2], bcnt);
		printk("HW:ECC_ERR%d: byte_addr 0x%x bit_addr %d hwccc %d\n", i+1, base + ((ecc_err >> 6) & 0x1FF), (ecc_err >> 2) & 0x7, ecc_err & 1);
		if (bcnt == 12) {
			/* correctable error */
			bit_addr = ((ecc_xor[2] >> 5) & 0x4) | ((ecc_xor[2] >> 4) & 0x2) | ((ecc_xor[2] >> 3) & 0x1);
			byte_addr = (((unsigned int)ecc_xor[2] << 7) & 0x100) |
				    (ecc_xor[1] & 0x80) |
				    ((ecc_xor[1] << 1) & 0x40) |
				    ((ecc_xor[1] << 2) & 0x20) |
				    ((ecc_xor[1] << 3) & 0x10) |
				    ((ecc_xor[0] >> 4) & 0x08) |
				    ((ecc_xor[0] >> 3) & 0x04) |
				    ((ecc_xor[0] >> 2) & 0x02) |
				    ((ecc_xor[0] >> 1) & 0x01);
			*(data + byte_addr) ^= 1 << bit_addr;
			corrected++;
			printk("SW:ECC     : byte_addr 0x%x bit_addr %d\n", base + byte_addr, bit_addr);
		} else if (bcnt == 1) {
			/* ECC code error */
			printk("ECC code error!\n");
			err++;
		} else {
			/* Uncorrectable error */
			printk("Uncorrectable error!\n");
			err++;
		}
	}

	ecc_stats->failed += err;
	ecc_stats->corrected += corrected;
	if (err)
		return -EBADMSG;
	else if (corrected)
		return -EUCLEAN;

	return 0;

bad_block:
	return -1;
}


/**
 * @return -EIO, writing size is less than a page 
 * @return
 * 	0:	OK
 *  -EUCLEAN:	corrected, ok
 *  -EBADMSG:	uncorrectable error or ECC code error
 *     <0:	error
 */
int nfc_read_page(struct ra_nand_chip *ra, unsigned char *buf, int page, int flags)
{
	unsigned int cmd1 = 0, cmd2 = 0, conf = 0;
	unsigned int bus_addr = 0, bus_addr2 = 0;
	unsigned int ecc_en;
	int use_gdma;
	int size, offs;
	int status = 0;

	use_gdma = flags & FLAG_USE_GDMA;
	ecc_en = flags & FLAG_ECC_EN;

	page = page & (CFG_CHIPSIZE - 1); // chip boundary
	size = CFG_PAGESIZE + CFG_PAGE_OOBSIZE; //add oobsize
	offs = 0;

	while (size > 0) {
		int len;
		len = size;
		bus_addr = (page << (CFG_COLUMN_ADDR_CYCLE*8)) | (offs & ((1<<CFG_COLUMN_ADDR_CYCLE*8)-1)); 
		if (is_nand_page_2048) {
			bus_addr2 = page >> (CFG_COLUMN_ADDR_CYCLE*8);
			cmd1 = 0x0;
			cmd2 = 0x30;
			conf = 0x000511| ((CFG_ADDR_CYCLE)<<16) | (len << 20); 
		}
		else {
			if (offs & ~(CFG_PAGESIZE-1))
				cmd1 = 0x50;
			else if (offs & ~((1<<CFG_COLUMN_ADDR_CYCLE*8)-1))
				cmd1 = 0x01;
			else
				cmd1 = 0;

			conf = 0x000141| ((CFG_ADDR_CYCLE)<<16) | (len << 20); 
		}
		if (ecc_en) 
			conf |= (1<<3); 
		if (use_gdma)
			conf |= (1<<2);

		status = _nfc_read_raw_data(cmd1, cmd2, bus_addr, bus_addr2, conf, buf+offs, len, flags);
		if (status & NAND_STATUS_FAIL) {
			printk("%s: fail \n", __func__);
			return -EIO;
		}

		offs += len;
		size -= len;
	}

	// verify and correct ecc
	if ((flags & (FLAG_VERIFY | FLAG_ECC_EN)) == (FLAG_VERIFY | FLAG_ECC_EN)) {
		status = nfc_ecc_verify(ra, buf, page, FL_READING);	
		if (status == -EUCLEAN) {
			printk("%s: corrected, buf:%p, page:%x\n", __func__, buf, page);
			return status;
		} else if (status != 0) {
			printk("%s: fail, buf:%p, page:%x, flag:%x, status %d\n",
			       __func__, buf, page, flags, status);
			return -EBADMSG;
		}
	}
	else {
		// fix,e not yet support
		ra->buffers_page = -1; //cached
	}

	return 0;
}

int nand_bbt_set(struct ra_nand_chip *ra, int block, int tag);
/** 
 * @return -EIO, fail to write
 * @return
 * 	0:	OK
 *  -EUCLEAN:	corrected, ok
 *  -EBADMSG:	uncorrectable error or ECC code error
 *     <0:	error
 */
int nfc_write_page(struct ra_nand_chip *ra, unsigned char *buf, int page, int flags)
{
	unsigned int cmd1 = 0, cmd3, conf = 0;
	unsigned int bus_addr = 0, bus_addr2 = 0;
	unsigned int ecc_en;
	int use_gdma;
	int size;
	int status;
	uint8_t *oob = buf + (1<<ra->page_shift);

	use_gdma = flags & FLAG_USE_GDMA;
	ecc_en = flags & FLAG_ECC_EN;

	oob[ra->badblockpos] = 0xff;	//tag as good block.
	ra->buffers_page = -1; //cached

	page = page & (CFG_CHIPSIZE-1); //chip boundary
	size = CFG_PAGESIZE + CFG_PAGE_OOBSIZE; //add oobsize
	bus_addr = (page << (CFG_COLUMN_ADDR_CYCLE*8)); //write_page always write from offset 0.

	if (is_nand_page_2048) {
		bus_addr2 = page >> (CFG_COLUMN_ADDR_CYCLE*8);
		cmd1 = 0x80;
		cmd3 = 0x10;
		conf = 0x001123| ((CFG_ADDR_CYCLE)<<16) | (size << 20); 
	}
	else {
		cmd1 = 0x8000;
		cmd3 = 0x10;
		conf = 0x001223| ((CFG_ADDR_CYCLE)<<16) | (size << 20);
	}

	if (ecc_en) 
		conf |= (1<<3); //enable ecc
	if (use_gdma)
		conf |= (1<<2);

	// set NFC
	ra_dbg("nfc_write_page: cmd1: %x, cmd3: %x bus_addr: %x, conf: %x, len:%x\n", 
	       cmd1, cmd3, bus_addr, conf, size);

	status = _nfc_write_raw_data(cmd1, cmd3, bus_addr, bus_addr2, conf, buf, size, flags);
	if (status & NAND_STATUS_FAIL)
		return -EIO;

	if (flags & FLAG_VERIFY) { // verify and correct ecc
		status = nfc_ecc_verify(ra, buf, page, FL_WRITING);
		if (status == -EUCLEAN) {
			printk("%s: corrected, buf:%p, page:%x, \n", __func__, buf, page);
			return -EIO;
		} else if (status != 0)
			return -EIO;
	}

	ra->buffers_page = page; //cached
	return 0;
}


/*************************************************************
 * nand internal process 
 *************************************************************/
/**
 * nand_release_device - [GENERIC] release chip
 * @mtd:	MTD device structure
 *
 * Deselect, release chip lock and wake up anyone waiting on the device
 */
static void nand_release_device(struct ra_nand_chip *ra)
{
	/* De-select the NAND device */
	nfc_select_chip(ra, -1);
	/* Release the controller and the chip */
	ra->state = FL_READY;
	mutex_unlock(ra->controller);
}

/**
 * nand_get_device - [GENERIC] Get chip for selected access
 * @chip:	the nand chip descriptor
 * @mtd:	MTD device structure
 * @new_state:	the state which is requested
 *
 * Get the device and lock it for exclusive access
 */
static int
nand_get_device(struct ra_nand_chip *ra, int new_state)
{
	int ret = 0;

	ret = mutex_lock_interruptible(ra->controller);
	if (!ret) 
		ra->state = new_state;

	return ret;

}

/*************************************************************
 * nand internal process 
 *************************************************************/

int nand_bbt_get(struct ra_nand_chip *ra, int block)
{
	int byte, bits;
	bits = block * BBTTAG_BITS;

	byte = bits / 8;
	bits = bits % 8;
	
	return (ra->bbt[byte] >> bits) & BBTTAG_BITS_MASK;
}

int nand_bbt_set(struct ra_nand_chip *ra, int block, int tag)
{
	int byte, bits;
	bits = block * BBTTAG_BITS;
	byte = bits / 8;
	bits = bits % 8;
	ra->bbt[byte] = (ra->bbt[byte] & ~(BBTTAG_BITS_MASK << bits)) | ((tag & BBTTAG_BITS_MASK) << bits);
		
	return tag;
}

/**
 * nand_block_checkbad - [GENERIC] Check if a block is marked bad
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 *
 * Check, if the block is bad. Either by reading the bad block table or
 * calling of the scan function.
 */
int nand_block_checkbad(struct ra_nand_chip *ra, loff_t offs)
{
	int page, block;
	int ret = 4;
	unsigned int tag;
	char *str[]= {"UNK", "RES", "BAD", "GOOD"};

	if (ranfc_bbt == 0)
		return 0;

	// align with chip
	offs = offs & ((1<<ra->chip_shift) -1);
	page = offs >> ra->page_shift;
	block = offs >> ra->erase_shift;

	tag = nand_bbt_get(ra, block);

	if (tag == BBT_TAG_UNKNOWN) {
		ret = nfc_read_oob(ra, page, ra->badblockpos, (char*)&tag, 1, FLAG_NONE);
		if (ret == 0)
			tag = ((le32_to_cpu(tag) & 0x0ff) == 0x0ff) ? BBT_TAG_GOOD : BBT_TAG_BAD;
		else
			tag = BBT_TAG_BAD;

		if (tag == BBT_TAG_GOOD) {
			ret = nfc_read_oob(ra, page + 1, ra->badblockpos, (char*)&tag, 1, FLAG_NONE);
			if (ret == 0)
				tag = ((le32_to_cpu(tag) & 0x0ff) == 0x0ff) ? BBT_TAG_GOOD : BBT_TAG_BAD;
			else
				tag = BBT_TAG_BAD;
		}

		nand_bbt_set(ra, block, tag);
	}

	if (tag != BBT_TAG_GOOD) {
		printk("%s: block: 0x%x tag: %s (offs: %x)\n", __func__, block, str[tag], (unsigned int)offs);
		return 1;
	}
	else 
		return 0;
	
}


/**
 * nand_block_markbad -
 */
int nand_block_markbad(struct ra_nand_chip *ra, loff_t offs)
{
	int block;
	int i = 0, ret = 4;
	unsigned int tag;
	uint8_t buf[2] = { 0x33, 0x33 };
	struct mtd_oob_ops ops;

	// align with chip
	ra_dbg("%s offs: %x \n", __func__, (int)offs);
	offs = offs & ((1<<ra->chip_shift) -1);
	block = offs >> ra->erase_shift;

	tag = nand_bbt_get(ra, block);
	if (tag == BBT_TAG_BAD)
		printk("%s: may mark bad-block repeatedly \n", __func__);

	nand_get_device(ra, FL_WRITING);
	/* Write to first two pages and to byte 1 and 6 if necessary.
	 * If we write to more than one location, the first error
	 * encountered quits the procedure. We write two bytes per
	 * location, so we dont have to mess with 16 bit access.
	 */
	do {
		ops.mode = MTD_OOB_PLACE;
		ops.len = ops.ooblen = 2;
		ops.datbuf = NULL;
		ops.oobbuf = buf;
		ops.ooboffs = ra->badblockpos & ~0x01;
		ret = nand_do_write_ops(ra, offs, &ops);
		if (ret) {
			printk("%s: nand_do_write_ops(offs 0x%lx) return %d.\n",
				__func__, (unsigned long)offs, ret);
		}

		i++;
		offs += ranfc_mtd->writesize;
	} while (!ret && i < 2);
	nand_release_device(ra);

	//update bbt
	nand_bbt_set(ra, block, BBT_TAG_BAD);

	return 0;
}


/**
 * nand_erase_nand - [Internal] erase block(s)
 * @mtd:	MTD device structure
 * @instr:	erase instruction
 * @allowbbt:	allow erasing the bbt area
 *
 * Erase one ore more blocks
 */
int nand_erase_nand(struct ra_nand_chip *ra, struct erase_info *instr)
{
	int page, len, status, ret;
	unsigned int addr, blocksize = 1<<ra->erase_shift;

	ra_dbg("%s: start:%x, len:%x \n", __func__, 
	       (unsigned int)instr->addr, (unsigned int)instr->len);
	if (BLOCK_ALIGNED(instr->addr) || BLOCK_ALIGNED(instr->len)) {
		ra_dbg("%s: erase block not aligned, addr:%x len:%x\n", __func__, instr->addr, instr->len);
		return -EINVAL;
	}
	instr->fail_addr = 0xffffffff;

	len = instr->len;
	addr = instr->addr;
	instr->state = MTD_ERASING;

	while (len) {

		page = (int)(addr >> ra->page_shift);

		/* select device and check wp */
		if (nfc_enable_chip(ra, addr, 0)) {
			printk("%s: nand is write protected \n", __func__);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}

		/* if we have a bad block, we do not erase bad blocks */
		if (nand_block_checkbad(ra, addr)) {
			printk(KERN_WARNING "nand_erase: attempt to erase a "
			       "bad block at 0x%08x\n", addr);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}

		/*
		 * Invalidate the page cache, if we erase the block which
		 * contains the current cached page
		 */
		if (BLOCK_ALIGNED(addr) == BLOCK_ALIGNED(ra->buffers_page << ra->page_shift))
			ra->buffers_page = -1;

		status = nfc_erase_block(ra, page);

		/* See if block erase succeeded */
		if (status) {
			printk("%s: failed erase, page 0x%08x\n", __func__, page);
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = (page << ra->page_shift);
			goto erase_exit;
		}


		/* Increment page address and decrement length */
		len -= blocksize;
		addr += blocksize;

	}
	instr->state = MTD_ERASE_DONE;

erase_exit:

	ret = ((instr->state == MTD_ERASE_DONE) ? 0 : -EIO);
	/* Do call back function */
	if (!ret)
		mtd_erase_callback(instr);

	/* Return more or less happy */
	return ret;
}


static int nand_write_oob_buf(struct ra_nand_chip *ra, uint8_t *buf, uint8_t *oob, size_t size, 
			      int mode, int ooboffs) 
{
	size_t oobsize = 1<<ra->oob_shift;
	int retsize = 0;

	ra_dbg("%s: size:%x, mode:%x, offs:%x  \n", __func__, size, mode, ooboffs);
	switch(mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_RAW:
		if (ooboffs > oobsize)
			return -1;

		size = min(size, oobsize - ooboffs);
		memcpy(buf + ooboffs, oob, size);
		retsize = size;
		break;

	case MTD_OOB_AUTO:  
	{
		struct nand_oobfree *free;
		uint32_t woffs = ooboffs;

		if (ooboffs > ra->oob->oobavail) 
			return -1;

		/* OOB AUTO does not clear buffer */
		while (size) {
			for(free = ra->oob->oobfree; free->length && size; free++) {
				int wlen = free->length - woffs;
				int bytes = 0;

				/* Write request not from offset 0 ? */
				if (wlen <= 0) {
					woffs = -wlen;
					continue;
				}
			
				bytes = min_t(size_t, size, wlen);
				memcpy (buf + free->offset + woffs, oob, bytes);
				woffs = 0;
				oob += bytes;
				size -= bytes;
				retsize += bytes;
			}
			
			buf += oobsize;
		}
		
		break;
	}
	default:
		BUG();
	}

	return retsize;
}

static int nand_read_oob_buf(struct ra_nand_chip *ra, uint8_t *oob, size_t size, 
			     int mode, int ooboffs) 
{
	size_t oobsize = 1<<ra->oob_shift;
	uint8_t *buf = ra->buffers + (1<<ra->page_shift);
	int retsize=0;

	ra_dbg("%s: size:%x, mode:%x, offs:%x  \n", __func__, size, mode, ooboffs);
	switch(mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_RAW:
		if (ooboffs > oobsize)
			return -1;

		size = min(size, oobsize - ooboffs);
		memcpy(oob, buf + ooboffs, size);
		return size;

	case MTD_OOB_AUTO: {
		struct nand_oobfree *free;
		uint32_t woffs = ooboffs;

		if (ooboffs > ra->oob->oobavail) 
			return -1;
		
		size = min(size, ra->oob->oobavail - ooboffs);
		for(free = ra->oob->oobfree; free->length && size; free++) {
			int wlen = free->length - woffs;
			int bytes = 0;

			/* Write request not from offset 0 ? */
			if (wlen <= 0) {
				woffs = -wlen;
				continue;
			}
			
			bytes = min_t(size_t, size, wlen);
			memcpy (oob, buf + free->offset + woffs, bytes);
			woffs = 0;
			oob += bytes;
			size -= bytes;
			retsize += bytes;
		}
		return retsize;
	}
	default:
		BUG();
	}
	
	return -1;
}


/**
 * nand_do_write_ops - [Internal] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operations description structure
 *
 * NAND write with ECC
 */
static int nand_do_write_ops(struct ra_nand_chip *ra, loff_t to,
			     struct mtd_oob_ops *ops)
{
	int page;
	uint32_t datalen = ops->len;
	uint32_t ooblen = ops->ooblen;
	uint8_t *oob = ops->oobbuf;
	uint8_t *data = ops->datbuf;
	int pagesize = (1<<ra->page_shift);
	int pagemask = (pagesize -1);
	int oobsize = 1<<ra->oob_shift;
	loff_t addr = to;
	//int i = 0; //for ra_dbg only

	ra_dbg("%s: to:%x, ops data:%p, oob:%p datalen:%x ooblen:%x, ooboffs:%x oobmode:%x \n", 
	       __func__, (unsigned int)to, data, oob, datalen, ooblen, ops->ooboffs, ops->mode);
	ops->retlen = 0;
	ops->oobretlen = 0;

	/* Invalidate the page cache, when we write to the cached page */
	ra->buffers_page = -1;
	
	if (data ==0)
		datalen = 0;
	
	// oob sequential (burst) write
	if (datalen == 0 && ooblen) {
		int len = oobsize;

		/* select chip, and check if it is write protected */
		if (nfc_enable_chip(ra, addr, 0))
			return -EIO;

		//FIXME, need sanity check of block boundary
		page = (int)((to & ((1<<ra->chip_shift)-1)) >> ra->page_shift); //chip boundary
		memset(ra->buffers, 0x0ff, pagesize);
		//fixme, should we reserve the original content?
		if (ops->mode == MTD_OOB_AUTO) {
			nfc_read_oob(ra, page, 0, ra->buffers, len, FLAG_NONE);
		}
		//prepare buffers
		nand_write_oob_buf(ra, ra->buffers, oob, ooblen, ops->mode, ops->ooboffs);
		// write out buffer to chip
		nfc_write_oob(ra, page, 0, ra->buffers, len, FLAG_USE_GDMA);

		ops->oobretlen = ooblen;
		ooblen = 0;
	}

	// data sequential (burst) write
	if (datalen && ooblen == 0) {
		// ranfc can not support write_data_burst, since hw-ecc and fifo constraints..
	}

	// page write
	while(datalen || ooblen) {
		int len;
		int ret;
		int offs;
		int ecc_en = 0;
		int i = 0;

		ra_dbg("%s (%d): addr:%x, ops data:%p, oob:%p datalen:%x ooblen:%x, ooboffs:%x \n", 
		       __func__, i, (unsigned int)addr, data, oob, datalen, ooblen, ops->ooboffs);
		++i;
		page = (int)((addr & ((1<<ra->chip_shift)-1)) >> ra->page_shift); //chip boundary
		
		/* select chip, and check if it is write protected */
		if (nfc_enable_chip(ra, addr, 0))
			return -EIO;

		// oob write
		if (ops->mode == MTD_OOB_AUTO) {
			//fixme, this path is not yet varified 
			nfc_read_oob(ra, page, 0, ra->buffers + pagesize, oobsize, FLAG_NONE);
		}
		if (oob && ooblen > 0) {
			len = nand_write_oob_buf(ra, ra->buffers + pagesize, oob, ooblen, ops->mode, ops->ooboffs);
			if (len < 0) 
				return -EINVAL;
			
			oob += len;
			ops->oobretlen += len;
			ooblen -= len;
		}

		// data write
		offs = addr & pagemask;
		len = min_t(size_t, datalen, pagesize - offs);
		if (data && len > 0) {
			memcpy(ra->buffers + offs, data, len);	// we can not sure ops->buf wether is DMA-able.

			data += len;
			datalen -= len;
			ops->retlen += len;

			ecc_en = FLAG_ECC_EN;
		}
		
		ret = nfc_write_page(ra, ra->buffers, page, FLAG_USE_GDMA | FLAG_VERIFY |
				     ((ops->mode == MTD_OOB_RAW || ops->mode == MTD_OOB_PLACE) ? 0 : ecc_en ));
		if (ret)
			return ret;

		nand_bbt_set(ra, addr >> ra->erase_shift, BBT_TAG_GOOD);
		addr = (page+1) << ra->page_shift;
	}
	return 0;
}

/**
 * nand_do_read_ops - [Internal] Read data with ECC
 *
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob ops structure
 *
 * Internal function. Called with chip held.
 */
static int nand_do_read_ops(struct ra_nand_chip *ra, loff_t from,
			    struct mtd_oob_ops *ops)
{
	int page;
	uint32_t datalen = ops->len;
	uint32_t ooblen = ops->ooblen;
	uint8_t *oob = ops->oobbuf;
	uint8_t *data = ops->datbuf;
	int pagesize = (1<<ra->page_shift);
	int pagemask = (pagesize -1);
	loff_t addr = from;
	const struct mtd_info *mtd = ranfc_mtd;
	struct mtd_ecc_stats stats = mtd->ecc_stats;
	//int i = 0; //for ra_dbg only

	ra_dbg("%s: addr:%x, ops data:%p, oob:%p datalen:%x ooblen:%x, ooboffs:%x \n", 
	       __func__, (unsigned int)addr, data, oob, datalen, ooblen, ops->ooboffs);
	ops->retlen = 0;
	ops->oobretlen = 0;
	if (data == 0)
		datalen = 0;

	while(datalen || ooblen) {
		int len;
		int ret;
		int offs;
		int i = 0;

		ra_dbg("%s (%d): addr:%x, ops data:%p, oob:%p datalen:%x ooblen:%x, ooboffs:%x \n", 
		       __func__, i, (unsigned int)addr, data, oob, datalen, ooblen, ops->ooboffs);
		++i;
		/* select chip */
		if (nfc_enable_chip(ra, addr, 1) < 0)
			return -EIO;

		page = (int)((addr & ((1<<ra->chip_shift)-1)) >> ra->page_shift); 
		ret = nfc_read_page(ra, ra->buffers, page, FLAG_VERIFY | 
				    ((ops->mode == MTD_OOB_RAW || ops->mode == MTD_OOB_PLACE) ? 0: FLAG_ECC_EN ));
		//FIXME, something strange here, some page needs 2 more tries to guarantee read success.
		if (ret < 0 && ret != -EUCLEAN) {
			printk("%s: read again. (ret %d)\n", __func__, ret);
			ret = nfc_read_page(ra, ra->buffers, page, FLAG_VERIFY | 
					    ((ops->mode == MTD_OOB_RAW || ops->mode == MTD_OOB_PLACE) ? 0: FLAG_ECC_EN ));
			if (ret >= 0 || ret == -EUCLEAN)
				printk("%s: read again susccess. (ret %d) \n", __func__, ret);
		}

		// oob read
		if (oob && ooblen > 0) {
			len = nand_read_oob_buf(ra, oob, ooblen, ops->mode, ops->ooboffs);
			if (len < 0) {
				printk("nand_read_oob_buf: fail return %x \n", len);
				return -EINVAL;
			}

			oob += len;
			ops->oobretlen += len;
			ooblen -= len;
		}

		// data read
		offs = addr & pagemask;
		len = min_t(size_t, datalen, pagesize - offs);
		if (data && len > 0) {
			memcpy(data, ra->buffers + offs, len);	// we can not sure ops->buf wether is DMA-able.

			data += len;
			datalen -= len;
			ops->retlen += len;
			if (ret && ret != -EUCLEAN)
				return ret;
		}

		nand_bbt_set(ra, addr >> ra->erase_shift, BBT_TAG_GOOD);
		// address go further to next page, instead of increasing of length of write. This avoids some special cases wrong.
		addr = (page+1) << ra->page_shift;
	}

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return  mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}

/************************************************************
 * the following are mtd necessary interface.
 ************************************************************/
/**
 * nand_erase - [MTD Interface] erase block(s)
 * @mtd:	MTD device structure
 * @instr:	erase instruction
 *
 * Erase one ore more blocks
 */
static int ramtd_nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int ret;
	struct ra_nand_chip *ra = (struct ra_nand_chip *)mtd->priv;

	ra_dbg("%s: \n", __func__);
	/* Grab the lock and see if the device is available */
	nand_get_device(ra, FL_ERASING);
	ret = nand_erase_nand((struct ra_nand_chip *)mtd->priv, instr);
	/* Deselect and wake up anyone waiting on the device */
	nand_release_device(ra);
	
	return ret;
}

/**
 * nand_write - [MTD Interface] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @len:	number of bytes to write
 * @retlen:	pointer to variable to store the number of written bytes
 * @buf:	the data to write
 *
 * NAND write with ECC
 */
static int ramtd_nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			    size_t *retlen, const uint8_t *buf)
{
	struct ra_nand_chip *ra = mtd->priv;
	int ret;
	struct mtd_oob_ops ops;

	ra_dbg("%s: \n", __func__);
	/* Do not allow reads past end of device */
	if ((to + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

	nand_get_device(ra, FL_WRITING);

	memset(&ops, 0, sizeof(ops));
	ops.len = len;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;
	ops.mode =  MTD_OOB_AUTO;
	ret = nand_do_write_ops(ra, to, &ops);
	*retlen = ops.retlen;

	nand_release_device(ra);

	return ret;
}

/**
 * nand_read - [MTD Interface] MTD compability function for nand_do_read_ecc
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @len:	number of bytes to read
 * @retlen:	pointer to variable to store the number of read bytes
 * @buf:	the databuffer to put data
 *
 * Get hold of the chip and call nand_do_read
 */
static int ramtd_nand_read(struct mtd_info *mtd, loff_t from, size_t len,
			   size_t *retlen, uint8_t *buf)
{
	struct ra_nand_chip *ra = mtd->priv;
	int ret;
	struct mtd_oob_ops ops;

	ra_dbg("%s: mtd:%p from:%x, len:%x, buf:%p \n", __func__, mtd, (unsigned int)from, len, buf);
	/* Do not allow reads past end of device */
	if ((from + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

	nand_get_device(ra, FL_READING);
	
	memset(&ops, 0, sizeof(ops));
	ops.len = len;
	ops.datbuf = buf;
	ops.oobbuf = NULL;
	ops.mode = MTD_OOB_AUTO;
	ret = nand_do_read_ops(ra, from, &ops);
	*retlen = ops.retlen;

	nand_release_device(ra);

	return ret;
}

/**
 * nand_read_oob - [MTD Interface] NAND read data and/or out-of-band
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob operation description structure
 *
 * NAND read data and/or out-of-band data
 */
static int ramtd_nand_readoob(struct mtd_info *mtd, loff_t from,
			      struct mtd_oob_ops *ops)
{
	struct ra_nand_chip *ra = mtd->priv;
	int ret;

	ra_dbg("%s: \n", __func__);
	nand_get_device(ra, FL_READING);
	ret = nand_do_read_ops(ra, from, ops);
	nand_release_device(ra);

	return ret;
}

/**
 * nand_write_oob - [MTD Interface] NAND write data and/or out-of-band
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operation description structure
 */
static int ramtd_nand_writeoob(struct mtd_info *mtd, loff_t to,
			       struct mtd_oob_ops *ops)
{
	struct ra_nand_chip *ra = mtd->priv;
	int ret;

	ra_dbg("%s: \n", __func__);
	nand_get_device(ra, FL_WRITING);
	ret = nand_do_write_ops(ra, to, ops);
	nand_release_device(ra);

	return ret;
}


/**
 * nand_block_isbad - [MTD Interface] Check if block at offset is bad
 * @mtd:	MTD device structure
 * @offs:	offset relative to mtd start
 */
static int ramtd_nand_block_isbad(struct mtd_info *mtd, loff_t offs)
{

	/* Check for invalid offset */
	//ra_dbg("%s: \n", __func__);

	if (offs > mtd->size)
		return -EINVAL;
	
	return nand_block_checkbad((struct ra_nand_chip *)mtd->priv, offs);
}

/**
 * nand_block_markbad - [MTD Interface] Mark block at the given offset as bad
 * @mtd:	MTD device structure
 * @ofs:	offset relative to mtd start
 */
static int ramtd_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct ra_nand_chip *ra = mtd->priv;

	ra_dbg("%s: \n", __func__);
	return nand_block_markbad(ra, ofs);
}


/************************************************************
 * the init/exit section.
 */

static struct nand_ecclayout ra_oob_layout = {
	.eccbytes = CONFIG_ECC_BYTES,
	.eccpos = {5, 6, 7},
	.oobfree = {
		 {.offset = 0, .length = 4},
		 {.offset = 8, .length = 8},
		 {.offset = 0, .length = 0}
	 },
#define RA_CHIP_OOB_AVAIL (4+8)
	.oobavail = RA_CHIP_OOB_AVAIL,
	// 5th byte is bad-block flag.
};

int ra_nand_id(char *buf, size_t buf_len)
{
	char *type_name = "Unknown type";
	int maf_id, maf_idx, dev_id;
	unsigned char id[4];
	struct nand_flash_dev *type = nand_flash_ids;

	if (!buf | !buf_len)
		return -1;

	*buf = '\0';
	_nfc_read_raw_data(0x90, 0, 0, 0, 0x410141, id, 4, FLAG_NONE);
	maf_id = id[0];
	dev_id = id[1];
	/* Try to identify manufacturer */
	for (maf_idx = 0; nand_manuf_ids[maf_idx].id != 0x0; maf_idx++) {
		if (nand_manuf_ids[maf_idx].id == maf_id)
			break;
	}

	for (; type->name != NULL; type++) {
		if (dev_id == type->id) {
			type_name = type->name;
                        break;
		}
	}

	snprintf(buf, buf_len, "0x%02X 0x%02X (%s, %s)",
		maf_id, dev_id, nand_manuf_ids[maf_idx].name, type_name);

	return 0;
}

static int __init ra_nand_init(void) 
{
	struct ra_nand_chip *ra;
	int alloc_size, bbt_size, buffers_size, reg;
	unsigned char chip_mode = 12;
	int raw_mode = 0;
	char buf[256];
#if !defined(CONFIG_MTD_UBI) && defined(CONFIG_ROOTFS_IN_FLASH_NO_PADDING)
	int i;
	loff_t offs;
	image_header_t hdr;
	version_t *hw = &hdr.u.tail.hw[0];
	uint32_t rfs_offset = 0;
	union {
		uint32_t rfs_offset_net_endian;
		uint8_t p[4];
	} u;
#endif
	
	if (ra_check_flash_type() != BOOT_FROM_NAND)
		raw_mode = 1;

	//FIXME: config 512 or 2048-byte page according to HWCONF
#if defined (CONFIG_RALINK_RT6855A)
	reg = ra_inl(RALINK_SYSCTL_BASE+0x8c);
	chip_mode = ((reg>>28) & 0x3)|(((reg>>22) & 0x3)<<2);
	if (chip_mode == 1) {
		printk("! nand 2048\n");
		ra_or(NFC_CONF1, 1);
		is_nand_page_2048 = 1;
		nand_addrlen = 5;
	}
	else {
		printk("! nand 512\n");
		ra_and(NFC_CONF1, ~1);
		is_nand_page_2048 = 0;
		nand_addrlen = 4;
	}	
#elif (defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_RT6855))
	ra_outl(RALINK_SYSCTL_BASE+0x60, ra_inl(RALINK_SYSCTL_BASE+0x60) & ~(0x3<<18));
	reg = ra_inl(RALINK_SYSCTL_BASE+0x10);
	chip_mode = (reg & 0x0F);

	if (raw_mode)
		chip_mode = 1;

	if((chip_mode==1)||(chip_mode==11)) {
		ra_or(NFC_CONF1, 1);
		is_nand_page_2048 = 1;
		nand_addrlen = ((chip_mode!=11) ? 4 : 5);
		printk("!!! nand page size = 2048, addr len=%d\n", nand_addrlen);
	}
	else {
		ra_and(NFC_CONF1, ~1);
		is_nand_page_2048 = 0;
		nand_addrlen = ((chip_mode!=10) ? 3 : 4);
		printk("!!! nand page size = 512, addr len=%d\n", nand_addrlen);
	}			
#else
	is_nand_page_2048 = 0;
	nand_addrlen = 3;
	printk("!!! nand page size = 512, addr len=%d\n", nand_addrlen);
#endif			

#if defined (CONFIG_RALINK_RT6855A) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_RT6855) 
	//config ECC location
	ra_and(NFC_CONF1, 0xfff000ff);
	ra_or(NFC_CONF1, ((CONFIG_ECC_OFFSET + 2) << 16) + ((CONFIG_ECC_OFFSET + 1) << 12) + (CONFIG_ECC_OFFSET << 8));
#endif

	if (!ra_nand_id(buf, sizeof(buf)))
		printk("Flash ID: %s\n", buf);

#define ALIGNE_16(a) (((unsigned long)(a)+15) & ~15)
	buffers_size = ALIGNE_16((1<<CONFIG_PAGE_SIZE_BIT) + (1<<CONFIG_OOBSIZE_PER_PAGE_BIT)); //ra->buffers
	bbt_size = BBTTAG_BITS * (1<<(CONFIG_CHIP_SIZE_BIT - (CONFIG_PAGE_SIZE_BIT + CONFIG_NUMPAGE_PER_BLOCK_BIT))) / 8; //ra->bbt
	bbt_size = ALIGNE_16(bbt_size);

	alloc_size = buffers_size + bbt_size;
	alloc_size += buffers_size; //for ra->readback_buffers
	alloc_size += sizeof(*ra); 
	alloc_size += sizeof(*ranfc_mtd);

	//make sure gpio-0 is input
	ra_outl(RALINK_PIO_BASE+0x24, ra_inl(RALINK_PIO_BASE+0x24) & ~0x01);

	ra = (struct ra_nand_chip *)kzalloc(alloc_size, GFP_KERNEL | GFP_DMA);
	if (!ra) {
		printk("%s: mem alloc fail \n", __func__);
		return -ENOMEM;
	}
	memset(ra, 0, alloc_size);

	//dynamic
	ra->buffers = (char *)((char *)ra + sizeof(*ra));
	ra->readback_buffers = ra->buffers + buffers_size; 
	ra->bbt = ra->readback_buffers + buffers_size; 
	ranfc_mtd = (struct mtd_info *)(ra->bbt + bbt_size);

	//static 
	ra->numchips		= CONFIG_NUMCHIPS;
	ra->chip_shift		= CONFIG_CHIP_SIZE_BIT;
	ra->page_shift		= CONFIG_PAGE_SIZE_BIT;
	ra->oob_shift		= CONFIG_OOBSIZE_PER_PAGE_BIT;
	ra->erase_shift		= (CONFIG_PAGE_SIZE_BIT + CONFIG_NUMPAGE_PER_BLOCK_BIT);
	ra->badblockpos		= CONFIG_BAD_BLOCK_POS;
	if (is_nand_page_2048) {
		int i, b;
		__u32 *p = &ra_oob_layout.eccpos[0];

		for (i = 0; i < 4; ++i, p+=3) {
			b = i * 16;
			*p = b + CONFIG_ECC_OFFSET;
			*(p+1) = b + CONFIG_ECC_OFFSET + 1;
			*(p+2) = b + CONFIG_ECC_OFFSET + 2;
		}
		ra_oob_layout.eccbytes = CONFIG_ECC_BYTES * 4;
		ra_oob_layout.oobavail = 64 - (ra_oob_layout.eccbytes + 1);
	} else {
		ra_oob_layout.eccpos[0] = CONFIG_ECC_OFFSET;
		ra_oob_layout.eccpos[1] = CONFIG_ECC_OFFSET + 1;
		ra_oob_layout.eccpos[2] = CONFIG_ECC_OFFSET + 2;

		ra_oob_layout.eccbytes = CONFIG_ECC_BYTES;
		ra_oob_layout.oobavail = 16 - (CONFIG_ECC_BYTES + 1);
	}
	ra->oob			= &ra_oob_layout;
	ra->buffers_page	= -1;

	ra_outl(NFC_CTRL, ra_inl(NFC_CTRL) | 0x01); //set wp to high
	nfc_all_reset();

	ranfc_mtd->type		= MTD_NANDFLASH;
	ranfc_mtd->flags	= MTD_CAP_NANDFLASH;
	ranfc_mtd->size		= CONFIG_NUMCHIPS * CFG_CHIPSIZE;
	ranfc_mtd->erasesize	= CFG_BLOCKSIZE;
	ranfc_mtd->writesize	= CFG_PAGESIZE;
	ranfc_mtd->oobsize 	= CFG_PAGE_OOBSIZE;
	ranfc_mtd->oobavail	= ra_oob_layout.oobavail;
	ranfc_mtd->name		= "ra_nfc";
	ranfc_mtd->ecclayout	= &ra_oob_layout;
	ranfc_mtd->erase 	= ramtd_nand_erase;
	ranfc_mtd->read		= ramtd_nand_read;
	ranfc_mtd->write	= ramtd_nand_write;
	ranfc_mtd->read_oob	= ramtd_nand_readoob;
	ranfc_mtd->write_oob	= ramtd_nand_writeoob;
	ranfc_mtd->block_isbad		= ramtd_nand_block_isbad;
	ranfc_mtd->block_markbad	= ramtd_nand_block_markbad;

	ranfc_mtd->priv = ra;

	ranfc_mtd->owner = THIS_MODULE;
	ra->controller = &ra->hwcontrol;
	mutex_init(ra->controller);

	printk("%s: alloc %x, at %p , btt(%p, %x), ranfc_mtd:%p\n", 
	       __func__ , alloc_size, ra, ra->bbt, bbt_size, ranfc_mtd);

	if (raw_mode) {
		rt2880_partitions[0].name = "NAND_ALL";
		rt2880_partitions[0].size = ranfc_mtd->size;
		rt2880_partitions[0].offset = 0;
		/* Register whole flash as a partition */
		add_mtd_partitions(ranfc_mtd, rt2880_partitions, 1);

		return 0;
	}

#if !defined(CONFIG_MTD_UBI) && defined(CONFIG_ROOTFS_IN_FLASH_NO_PADDING)
	offs = MTD_BOOT_PART_SIZE + MTD_CONFIG_PART_SIZE + MTD_FACTORY_PART_SIZE;
	ramtd_nand_read(ranfc_mtd, offs, sizeof(hdr), (size_t *)&alloc_size, (u_char *)(&hdr));
	/* Looking for rootfilesystem offset */
	u.rfs_offset_net_endian = 0;
	for (i = 0; i < (MAX_VER*2); ++i, ++hw) {
		if (hw->major != ROOTFS_OFFSET_MAGIC)
			continue;
		u.p[1] = hw->minor;
		hw++;
		u.p[2] = hw->major;
		u.p[3] = hw->minor;
		rfs_offset = ntohl(u.rfs_offset_net_endian);
	}

	if (rfs_offset != 0) {
		rt2880_partitions[4].offset = MTD_BOOT_PART_SIZE + MTD_CONFIG_PART_SIZE + MTD_FACTORY_PART_SIZE + rfs_offset;
		rt2880_partitions[4].mask_flags |= MTD_WRITEABLE;
		if (ranfc_mtd->size > 0x800000) {
			rt2880_partitions[3].size = ranfc_mtd->size - (MTD_BOOT_PART_SIZE + MTD_CONFIG_PART_SIZE + MTD_FACTORY_PART_SIZE) - MTD_JFFS2_PART_SIZE;
			rt2880_partitions[4].size = ranfc_mtd->size - rt2880_partitions[4].offset - MTD_JFFS2_PART_SIZE;
			rt2880_partitions[5].offset = ranfc_mtd->size - MTD_JFFS2_PART_SIZE;
		} else {
			rt2880_partitions[3].size = ranfc_mtd->size - (MTD_BOOT_PART_SIZE + MTD_CONFIG_PART_SIZE + MTD_FACTORY_PART_SIZE);
			rt2880_partitions[4].size = ranfc_mtd->size - rt2880_partitions[4].offset;
			rt2880_partitions[5].name = rt2880_partitions[6].name;
			rt2880_partitions[5].size = rt2880_partitions[6].size;
			rt2880_partitions[5].offset = rt2880_partitions[6].offset;
		}
		printk(KERN_NOTICE "partion 3: %x %x\n", (unsigned int)rt2880_partitions[3].offset, (unsigned int)rt2880_partitions[3].size);
		printk(KERN_NOTICE "partion 4: %x %x\n", (unsigned int)rt2880_partitions[4].offset, (unsigned int)rt2880_partitions[4].size);
	}
#endif

#if defined(CONFIG_MTD_UBI)
	printk("MTD_BOOT_PART_SIZE %x MTD_CONFIG_PART_SIZE %x rt2880_partitions[0].size %lx ranfc_mtd->erasesize %lx\n",
		MTD_BOOT_PART_SIZE, MTD_CONFIG_PART_SIZE, (unsigned long)rt2880_partitions[0].size, (unsigned long)ranfc_mtd->erasesize);
	rt2880_partitions[1].offset = ALIGN(rt2880_partitions[0].size + MTD_CONFIG_PART_SIZE, ranfc_mtd->erasesize);
	rt2880_partitions[1].size = ranfc_mtd->size - rt2880_partitions[1].offset;
#endif

	/* Register the partitions */
	add_mtd_partitions(ranfc_mtd, rt2880_partitions, ARRAY_SIZE(rt2880_partitions));

	return 0;
}

static void __devexit ra_nand_remove(void)
{
	struct ra_nand_chip *ra;
	
	if (ranfc_mtd) {
		ra = (struct ra_nand_chip  *)ranfc_mtd->priv;

		/* Deregister partitions */
		del_mtd_partitions(ranfc_mtd);
		kfree(ra);
	}
}

module_init(ra_nand_init);
module_exit(ra_nand_remove);

MODULE_LICENSE("GPL");
