#if defined (CONFIG_RALINK_RT3052)
#include "ralink_nand_rt3052.h"
#define RT2880_NAND_H
#endif

#ifndef RT2880_NAND_H
#define RT2880_NAND_H

#include <linux/mtd/mtd.h>

#if !defined (__UBOOT__)
#include <asm/rt2880/rt_mmap.h>
#else
#include <rt_mmap.h>
#define	EIO		 5	/* I/O error */
#define	EINVAL		22	/* Invalid argument */
#define	ENOMEM		12	/* Out of memory */
#define	EBADMSG		74	/* Not a data message */
#define	EUCLEAN		117	/* Structure needs cleaning */
#endif

//#include "gdma.h"

#define ra_inl(addr)  (*(volatile unsigned int *)(addr))
#define ra_outl(addr, value)  (*(volatile unsigned int *)(addr) = (value))
#define ra_aor(addr, a_mask, o_value)  ra_outl(addr, (ra_inl(addr) & (a_mask)) | (o_value))
#define ra_and(addr, a_mask)  ra_aor(addr, a_mask, 0)
#define ra_or(addr, o_value)  ra_aor(addr, -1, o_value)


#define CONFIG_NUMCHIPS 1
#define CONFIG_NOT_SUPPORT_WP //rt3052 has no WP signal for chip.
//#define CONFIG_NOT_SUPPORT_RB

extern int is_nand_page_2048;
extern int nand_addrlen;
extern const unsigned int nand_size_map[2][3];

//chip
// chip geometry: SAMSUNG small size 32MB.
#define CONFIG_CHIP_SIZE_BIT (nand_size_map[is_nand_page_2048][nand_addrlen-3]) //! (1<<NAND_SIZE_BYTE) MB
//#define CONFIG_CHIP_SIZE_BIT (is_nand_page_2048? 29 : 25)	//! (1<<NAND_SIZE_BYTE) MB
#define CONFIG_PAGE_SIZE_BIT (is_nand_page_2048? 11 : 9)	//! (1<<PAGE_SIZE) MB
//#define CONFIG_SUBPAGE_BIT 1		//! these bits will be compensate by command cycle
#define CONFIG_NUMPAGE_PER_BLOCK_BIT (is_nand_page_2048? 6 : 5)	//! order of number of pages a block. 
#define CONFIG_OOBSIZE_PER_PAGE_BIT (is_nand_page_2048? 6 : 4)	//! byte number of oob a page.
#define CONFIG_BAD_BLOCK_POS (is_nand_page_2048? 0 : 5)     //! offset of byte to denote bad block.
#define CONFIG_ECC_BYTES 3      //! ecc has 3 bytes
#define CONFIG_ECC_OFFSET (is_nand_page_2048? 6 : 5)        //! ecc starts from offset 5.

//this section should not be modified.
//#define CFG_COLUMN_ADDR_MASK ((1 << (CONFIG_PAGE_SIZE_BIT - CONFIG_SUBPAGE_BIT)) - 1)
//#define CFG_COLUMN_ADDR_CYCLE (((CONFIG_PAGE_SIZE_BIT - CONFIG_SUBPAGE_BIT) + 7)/8) 
//#define CFG_ROW_ADDR_CYCLE ((CONFIG_CHIP_SIZE_BIT - CONFIG_PAGE_SIZE_BIT + 7)/8) 
//#define CFG_ADDR_CYCLE (CFG_COLUMN_ADDR_CYCLE + CFG_ROW_ADDR_CYCLE)

#define CFG_COLUMN_ADDR_CYCLE   (is_nand_page_2048? 2 : 1)
#define CFG_ROW_ADDR_CYCLE      (nand_addrlen - CFG_COLUMN_ADDR_CYCLE)
#define CFG_ADDR_CYCLE (CFG_COLUMN_ADDR_CYCLE + CFG_ROW_ADDR_CYCLE)

#define CFG_CHIPSIZE    (1 << ((CONFIG_CHIP_SIZE_BIT>=32)? 31 : CONFIG_CHIP_SIZE_BIT))
//#define CFG_CHIPSIZE  	(1 << CONFIG_CHIP_SIZE_BIT)
#define CFG_PAGESIZE	(1 << CONFIG_PAGE_SIZE_BIT)
#define CFG_BLOCKSIZE 	(CFG_PAGESIZE << CONFIG_NUMPAGE_PER_BLOCK_BIT)
#define CFG_NUMPAGE	(1 << (CONFIG_CHIP_SIZE_BIT - CONFIG_PAGE_SIZE_BIT))
#define CFG_NUMBLOCK	(CFG_NUMPAGE >> CONFIG_NUMPAGE_PER_BLOCK_BIT)
#define CFG_BLOCK_OOBSIZE	(1 << (CONFIG_OOBSIZE_PER_PAGE_BIT + CONFIG_NUMPAGE_PER_BLOCK_BIT))	
#define CFG_PAGE_OOBSIZE	(1 << CONFIG_OOBSIZE_PER_PAGE_BIT)	

#define NAND_BLOCK_ALIGN(addr) ((addr) & (CFG_BLOCKSIZE-1))
#define NAND_PAGE_ALIGN(addr) ((addr) & (CFG_PAGESIZE-1))


#define NFC_BASE 	RALINK_NAND_CTRL_BASE
#define NFC_CTRL	(NFC_BASE + 0x0)
#define NFC_CONF	(NFC_BASE + 0x4)
#define NFC_CMD1	(NFC_BASE + 0x8)
#define NFC_CMD2	(NFC_BASE + 0xc)
#define NFC_CMD3	(NFC_BASE + 0x10)
#define NFC_ADDR	(NFC_BASE + 0x14)
#define NFC_DATA	(NFC_BASE + 0x18)
#if defined (CONFIG_RALINK_RT6855) || defined (CONFIG_RALINK_RT6855A) || \
	defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7621)
#define NFC_ECC		(NFC_BASE + 0x30)
#else
#define NFC_ECC		(NFC_BASE + 0x1c)
#endif
#define NFC_STATUS	(NFC_BASE + 0x20)
#define NFC_INT_EN	(NFC_BASE + 0x24)
#define NFC_INT_ST	(NFC_BASE + 0x28)
#if defined (CONFIG_RALINK_RT6855) || defined (CONFIG_RALINK_RT6855A) || \
	defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7621)
#define NFC_CONF1	(NFC_BASE + 0x2c)
#define NFC_ECC_P1	(NFC_BASE + 0x30)
#define NFC_ECC_P2	(NFC_BASE + 0x34)
#define NFC_ECC_P3	(NFC_BASE + 0x38)
#define NFC_ECC_P4	(NFC_BASE + 0x3c)
#define NFC_ECC_ERR1	(NFC_BASE + 0x40)
#define NFC_ECC_ERR2	(NFC_BASE + 0x44)
#define NFC_ECC_ERR3	(NFC_BASE + 0x48)
#define NFC_ECC_ERR4	(NFC_BASE + 0x4c)
#define NFC_ADDR2	(NFC_BASE + 0x50)
#endif

enum _int_stat {
	INT_ST_ND_DONE 	= 1<<0,
	INT_ST_TX_BUF_RDY       = 1<<1,
	INT_ST_RX_BUF_RDY	= 1<<2,
	INT_ST_ECC_ERR		= 1<<3,
	INT_ST_TX_TRAS_ERR	= 1<<4,
	INT_ST_RX_TRAS_ERR	= 1<<5,
	INT_ST_TX_KICK_ERR	= 1<<6,
	INT_ST_RX_KICK_ERR      = 1<<7
};


//#define WORKAROUND_RX_BUF_OV 1


/*************************************************************
 * stolen from nand.h
 *************************************************************/

/*
 * Standard NAND flash commands
 */
#define NAND_CMD_READ0		0
#define NAND_CMD_READ1		1
#define NAND_CMD_RNDOUT		5
#define NAND_CMD_PAGEPROG	0x10
#define NAND_CMD_READOOB	0x50
#define NAND_CMD_ERASE1		0x60
#define NAND_CMD_STATUS		0x70
#define NAND_CMD_STATUS_MULTI	0x71
#define NAND_CMD_SEQIN		0x80
#define NAND_CMD_RNDIN		0x85
#define NAND_CMD_READID		0x90
#define NAND_CMD_ERASE2		0xd0
#define NAND_CMD_RESET		0xff

/* Extended commands for large page devices */
#define NAND_CMD_READSTART	0x30
#define NAND_CMD_RNDOUTSTART	0xE0
#define NAND_CMD_CACHEDPROG	0x15

/* Extended commands for AG-AND device */
/*
 * Note: the command for NAND_CMD_DEPLETE1 is really 0x00 but
 *       there is no way to distinguish that from NAND_CMD_READ0
 *       until the remaining sequence of commands has been completed
 *       so add a high order bit and mask it off in the command.
 */
#define NAND_CMD_DEPLETE1	0x100
#define NAND_CMD_DEPLETE2	0x38
#define NAND_CMD_STATUS_MULTI	0x71
#define NAND_CMD_STATUS_ERROR	0x72
/* multi-bank error status (banks 0-3) */
#define NAND_CMD_STATUS_ERROR0	0x73
#define NAND_CMD_STATUS_ERROR1	0x74
#define NAND_CMD_STATUS_ERROR2	0x75
#define NAND_CMD_STATUS_ERROR3	0x76
#define NAND_CMD_STATUS_RESET	0x7f
#define NAND_CMD_STATUS_CLEAR	0xff

#define NAND_CMD_NONE		-1

/* Status bits */
#define NAND_STATUS_FAIL	0x01
#define NAND_STATUS_FAIL_N1	0x02
#define NAND_STATUS_TRUE_READY	0x20
#define NAND_STATUS_READY	0x40
#define NAND_STATUS_WP		0x80

typedef enum {
	FL_READY,
	FL_READING,
	FL_WRITING,
	FL_ERASING,
	FL_SYNCING,
	FL_CACHEDPRG,
	FL_PM_SUSPENDED,
} nand_state_t;

/*************************************************************/



typedef enum _ra_flags {
	FLAG_NONE	= 0,
	FLAG_ECC_EN 	= (1<<0),
	FLAG_USE_GDMA 	= (1<<1),
	FLAG_VERIFY 	= (1<<2),
} RA_FLAGS;


#define BBTTAG_BITS		2
#define BBTTAG_BITS_MASK	((1<<BBTTAG_BITS) -1)
enum BBT_TAG {
	BBT_TAG_UNKNOWN = 0, //2'b01
	BBT_TAG_GOOD	= 3, //2'b11
	BBT_TAG_BAD	= 2, //2'b10
	BBT_TAG_RES	= 1, //2'b01
};

struct ra_nand_chip {
	int	numchips;
	int 	chip_shift;
	int	page_shift;
	int 	erase_shift;
	int 	oob_shift;
	int	badblockpos;
#if !defined (__UBOOT__)
	struct mutex hwcontrol;
	struct mutex *controller;
#endif
	struct nand_ecclayout	*oob;
	int 	state;
	unsigned int 	buffers_page;
	unsigned char	*buffers; //[CFG_PAGESIZE + CFG_PAGE_OOBSIZE];
	unsigned char 	*readback_buffers;
	unsigned char 	*bbt;
#if defined (WORKAROUND_RX_BUF_OV)
	unsigned int	 sandbox_page;	// steal a page (block) for read ECC verification
#endif

};

/*
 * NAND Flash Manufacturer ID Codes
 */
#define NAND_MFR_TOSHIBA	0x98
#define NAND_MFR_SAMSUNG	0xec
#define NAND_MFR_FUJITSU	0x04
#define NAND_MFR_NATIONAL	0x8f
#define NAND_MFR_RENESAS	0x07
#define NAND_MFR_STMICRO	0x20
#define NAND_MFR_HYNIX		0xad
#define NAND_MFR_MICRON		0x2c
#define NAND_MFR_AMD		0x01
#define NAND_MFR_ZENTEL		0x92

/**
 * struct nand_flash_dev - NAND Flash Device ID Structure
 * @name:	Identify the device type
 * @id:		device ID code
 * @pagesize:	Pagesize in bytes. Either 256 or 512 or 0
 *		If the pagesize is 0, then the real pagesize
 *		and the eraseize are determined from the
 *		extended id bytes in the chip
 * @erasesize:	Size of an erase block in the flash device.
 * @chipsize:	Total chipsize in Mega Bytes
 * @options:	Bitfield to store chip relevant options
 */
struct nand_flash_dev {
	char *name;
	int id;
	unsigned long pagesize;
	unsigned long chipsize;
	unsigned long erasesize;
	unsigned long options;
};

/**
 * struct nand_manufacturers - NAND Flash Manufacturer ID Structure
 * @name:	Manufacturer name
 * @id:		manufacturer ID code of device.
*/
struct nand_manufacturers {
	int id;
	char * name;
};

/* Option constants for bizarre disfunctionality and real
*  features
*/
/* Chip can not auto increment pages */
#define NAND_NO_AUTOINCR	0x00000001
/* Buswitdh is 16 bit */
#define NAND_BUSWIDTH_16	0x00000002
/* Device supports partial programming without padding */
#define NAND_NO_PADDING		0x00000004
/* Chip has cache program function */
#define NAND_CACHEPRG		0x00000008
/* Chip has copy back function */
#define NAND_COPYBACK		0x00000010
/* AND Chip which has 4 banks and a confusing page / block
 * assignment. See Renesas datasheet for further information */
#define NAND_IS_AND		0x00000020
/* Chip has a array of 4 pages which can be read without
 * additional ready /busy waits */
#define NAND_4PAGE_ARRAY	0x00000040
/* Chip requires that BBT is periodically rewritten to prevent
 * bits from adjacent blocks from 'leaking' in altering data.
 * This happens with the Renesas AG-AND chips, possibly others.  */
#define BBT_AUTO_REFRESH	0x00000080
/* Chip does not require ready check on read. True
 * for all large page devices, as they do not support
 * autoincrement.*/
#define NAND_NO_READRDY		0x00000100
/* Chip does not allow subpage writes */
#define NAND_NO_SUBPAGE_WRITE	0x00000200

/* Device is one of 'new' xD cards that expose fake nand command set */
#define NAND_BROKEN_XD		0x00000400

/* Device behaves just like nand, but is readonly */
#define NAND_ROM		0x00000800

/* Options valid for Samsung large page devices */
#define NAND_SAMSUNG_LP_OPTIONS \
	(NAND_NO_PADDING | NAND_CACHEPRG | NAND_COPYBACK)

/* Macros to identify the above */
#define NAND_CANAUTOINCR(chip) (!(chip->options & NAND_NO_AUTOINCR))
#define NAND_MUST_PAD(chip) (!(chip->options & NAND_NO_PADDING))
#define NAND_HAS_CACHEPROG(chip) ((chip->options & NAND_CACHEPRG))
#define NAND_HAS_COPYBACK(chip) ((chip->options & NAND_COPYBACK))
/* Large page NAND with SOFT_ECC should support subpage reads */
#define NAND_SUBPAGE_READ(chip) ((chip->ecc.mode == NAND_ECC_SOFT) \
					&& (chip->page_shift > 9))

/* Mask to zero out the chip options, which come from the id table */
#define NAND_CHIPOPTIONS_MSK	(0x0000ffff & ~NAND_NO_AUTOINCR)

/* Non chip related options */
/* Use a flash based bad block table. This option is passed to the
 * default bad block table function. */
#define NAND_USE_FLASH_BBT	0x00010000
/* This option skips the bbt scan during initialization. */
#define NAND_SKIP_BBTSCAN	0x00020000
/* This option is defined if the board driver allocates its own buffers
   (e.g. because it needs them DMA-coherent */
#define NAND_OWN_BUFFERS	0x00040000
/* Chip may not exist, so silence any errors in scan */
#define NAND_SCAN_SILENT_NODEV	0x00080000

/* Options set by nand scan */
/* Nand scan has allocated controller struct */
#define NAND_CONTROLLER_ALLOC	0x80000000

/* Cell info constants */
#define NAND_CI_CHIPNR_MSK	0x03
#define NAND_CI_CELLTYPE_MSK	0x0C

//fixme, gdma api 
int nand_dma_sync(void);
void release_dma_buf(void);
int set_gdma_ch(unsigned long dst, 
		unsigned long src, unsigned int len, int burst_size,
		int soft_mode, int src_req_type, int dst_req_type,
		int src_burst_mode, int dst_burst_mode);




#endif
