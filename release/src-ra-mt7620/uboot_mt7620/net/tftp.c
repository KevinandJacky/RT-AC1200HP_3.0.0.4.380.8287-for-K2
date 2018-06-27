/*
 *	Copyright 1994, 1995, 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000, 2001 DENX Software Engineering, Wolfgang Denk, wd@denx.de
 */

#include <common.h>
#include <command.h>
#include <net.h>
#include "tftp.h"
#include "bootp.h"

#undef	ET_DEBUG

#if (CONFIG_COMMANDS & CFG_CMD_NET)

#define WELL_KNOWN_PORT	69		/* Well known TFTP port #		*/
#define TIMEOUT		5		/* Seconds to timeout for a lost pkt	*/
#ifndef	CONFIG_NET_RETRY_COUNT
# define TIMEOUT_COUNT	10		/* # of timeouts before giving up  */
#else
# define TIMEOUT_COUNT  (CONFIG_NET_RETRY_COUNT * 2)
#endif
					/* (for checking the image size)	*/
#define HASHES_PER_LINE	65		/* Number of "loading" hashes per line	*/

/*
 *	TFTP operations.
 */
#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5
#define TFTP_OACK	6


static int	TftpServerPort;		/* The UDP port at their end		*/
static int	TftpOurPort;		/* The UDP port at our end		*/
static int	TftpTimeoutCount;
static ulong	TftpBlock;		/* packet sequence number		*/
static ulong	TftpLastBlock;		/* last packet sequence number received */
static ulong	TftpBlockWrap;		/* count of sequence number wraparounds */
static ulong	TftpBlockWrapOffset;	/* memory offset due to wrapping	*/
static int	TftpState;
int		TftpStarted;		/* To know ARP reply requested by TFTP  */

#define STATE_RRQ	1
#define STATE_DATA	2
#define STATE_TOO_LARGE	3
#define STATE_BAD_MAGIC	4
#define STATE_OACK	5
#define STATE_RECV_WRQ	6
#define STATE_SEND_WRQ	7

#define TFTP_BLOCK_SIZE		512		    /* default TFTP block size	*/
#define TFTP_SEQUENCE_SIZE	((ulong)(1<<16))    /* sequence number is 16 bit */

static unsigned short TftpBlkSize = TFTP_BLOCK_SIZE;

#define DEFAULT_NAME_LEN	(8 + 4 + 1)
static char default_filename[DEFAULT_NAME_LEN];
static char *tftp_filename;

#ifdef CFG_DIRECT_FLASH_TFTP
extern flash_info_t flash_info[CFG_MAX_FLASH_BANKS];
#endif
static int	TftpWriting;	/* 1 if writing, else 0 */
#ifdef CONFIG_CMD_TFTPPUT
static int	TftpFinalBlock;	/* 1 if we have sent the last block */
#else
#define TftpWriting	0
#endif

static __inline__ void
store_block (unsigned block, uchar * src, unsigned len)
{
	ulong offset = block * TFTP_BLOCK_SIZE + TftpBlockWrapOffset;
	ulong newsize = offset + len;
#ifdef CFG_DIRECT_FLASH_TFTP
	int i, rc = 0;

	for (i=0; i<CFG_MAX_FLASH_BANKS; i++) {
		/* start address in flash? */
		if (load_addr + offset >= flash_info[i].start[0]) {
			rc = 1;
			break;
		}
	}

	if (rc) { /* Flash is destination for this packet */
		rc = flash_write ((uchar *)src, (ulong)(load_addr+offset), len);
		if (rc) {
			flash_perror (rc);
			NetState = NETLOOP_FAIL;
			return;
		}
	}
	else
#endif /* CFG_DIRECT_FLASH_TFTP */
	{
		(void)memcpy((void *)(load_addr + offset), src, len);
	}

	if (NetBootFileXferSize < newsize)
		NetBootFileXferSize = newsize;
}

#ifdef CONFIG_CMD_TFTPPUT
/**
 * Load the next block from memory to be sent over tftp.
 *
 * @param block	Block number to send
 * @param dst	Destination buffer for data
 * @param len	Number of bytes in block (this one and every other)
 * @return number of bytes loaded
 */
static int load_block(unsigned block, uchar *dst, unsigned len)
{
	ulong offset = (block - 1) * len + TftpBlockWrapOffset;
	ulong tosend = len;

	tosend = min(NetBootFileXferSize - offset, tosend);
	(void)memcpy(dst, (void *)(save_addr + offset), tosend);
	debug("%s: block=%d, offset=%ld, len=%d, tosend=%ld\n", __func__,
		block, offset, len, tosend);
	return tosend;
}
#endif

void TftpSend (void);
static void TftpTimeout (void);

/**********************************************************************/

static void show_block_marker(void)
{
	ulong pos = TftpBlock * TftpBlkSize + TftpBlockWrapOffset;

#ifdef CONFIG_TFTP_TSIZE
	if (TftpTsize) {
		while (TftpNumchars <
			/* TODO: needs to be different for put */
			pos * 50 / TftpTsize) {
			putc('#');
			TftpNumchars++;
		}
	} else
#endif
	{
		if (((TftpBlock - 1) % 10) == 0)
			putc('#');
		else if ((TftpBlock % (10 * HASHES_PER_LINE)) == 0)
			puts("\n\t ");
	}
}

/* The TFTP get or put is complete */
static void tftp_complete(void)
{
#ifdef CONFIG_TFTP_TSIZE
	/* Print hash marks for the last packet received */
	while (TftpTsize && TftpNumchars < 49) {
		putc('#');
		TftpNumchars++;
	}
#endif
	puts("\ndone\n");
	NetState = NETLOOP_SUCCESS;
}

void
TftpSend (void)
{
	volatile uchar *	pkt;
	volatile uchar *	xp;
	int			len = 0;
	volatile ushort *s;
	/*
	 *	We will always be sending some sort of packet, so
	 *	cobble together the packet headers now.
	 */
	pkt = NetTxPacket + NetEthHdrSize() + IP_HDR_SIZE;

	switch (TftpState) {

	case STATE_RRQ:
	case STATE_SEND_WRQ:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TftpState == STATE_RRQ ? TFTP_RRQ : TFTP_WRQ);
		pkt = (uchar *)s;
		strcpy ((char *)pkt, tftp_filename);
		pkt += strlen(tftp_filename) + 1;
		strcpy ((char *)pkt, "octet");
		pkt += 5 /*strlen("octet")*/ + 1;
		strcpy ((char *)pkt, "timeout");
		pkt += 7 /*strlen("timeout")*/ + 1;
		sprintf((char *)pkt, "%d", TIMEOUT);

#ifdef ET_DEBUG
		printf("send option \"timeout %s\"\n", (char *)pkt);
#endif
		pkt += strlen((char *)pkt) + 1;
#ifdef CONFIG_TFTP_TSIZE
		pkt += sprintf((char *)pkt, "tsize%c%lu%c",
				0, NetBootFileXferSize, 0);
#endif
		len = pkt - xp;
		break;

	case STATE_DATA:
	case STATE_OACK:
		xp = pkt;
		s = (ushort *)pkt;
		s[0] = htons(TFTP_ACK);
		s[1] = htons(TftpBlock);
		pkt = (uchar *)(s + 2);
#ifdef CONFIG_CMD_TFTPPUT
		if (TftpWriting) {
			int toload = TftpBlkSize;
			int loaded = load_block(TftpBlock, pkt, toload);

			s[0] = htons(TFTP_DATA);
			pkt += loaded;
			TftpFinalBlock = (loaded < toload);
		}
#endif
		//printf("\n [%d]",ttc++);
		//printf("\n w:htons(TftpBlock)=0x%04X,r:%04X\n",htons(TftpBlock),*(((ushort *)xp)+1));
		
		len = pkt - xp;
		break;

	case STATE_TOO_LARGE:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TFTP_ERROR);
			*s++ = htons(3);

		pkt = (uchar *)s;
		strcpy ((char *)pkt, "File too large");
		pkt += 14 /*strlen("File too large")*/ + 1;
		len = pkt - xp;
		break;

	case STATE_BAD_MAGIC:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TFTP_ERROR);
		*s++ = htons(2);
		pkt = (uchar *)s;
		strcpy ((char *)pkt, "File has bad magic");
		pkt += 18 /*strlen("File has bad magic")*/ + 1;
		len = pkt - xp;
		break;
	}

	NetSendUDPPacket(NetServerEther, NetServerIP, TftpServerPort, TftpOurPort, len);
}


static void
TftpHandler (uchar * pkt, unsigned dest, unsigned src, unsigned len)
{
	ushort proto;
	ushort *s;
	int i, block;

	if (dest != TftpOurPort) {
		return;
	}
	if (TftpState != STATE_RRQ && src != TftpServerPort &&
	    TftpState != STATE_RECV_WRQ && TftpState != STATE_SEND_WRQ) {
		return;
	}

	if (len < 2) {
		return;
	}
	len -= 2;
	/* warning: don't use increment (++) in ntohs() macros!! */
	s = (ushort *)pkt;
	proto = *s++;
	pkt = (uchar *)s;
	block = ntohs(*s);

	switch (ntohs(proto)) {

	case TFTP_RRQ:
	case TFTP_WRQ:
		break;
	case TFTP_ACK:
#ifdef CONFIG_CMD_TFTPPUT
		debug("Ack block %d\n", block);
		show_block_marker();
		if (TftpWriting) {
			if (TftpFinalBlock) {
				tftp_complete();
			} else {
				TftpBlock = block + 1;
				//show_block_marker();
				TftpSend(); /* Send next data block */
			}
		}
#endif
		break;
	case TFTP_OACK:
#ifdef ET_DEBUG
		printf("Got OACK: %s %s\n", pkt, pkt+strlen(pkt)+1);
#endif
		TftpState = STATE_OACK;
		TftpServerPort = src;
		/*
		 * Check for 'blksize' option.
		 * Careful: "i" is signed, "len" is unsigned, thus
		 * something like "len-8" may give a *huge* number
		 */
		for (i = 0; i+8 < len; i++) {
			if (strcmp((char *)pkt+i, "blksize") == 0) {
				TftpBlkSize = (unsigned short)
					simple_strtoul((char *)pkt+i+8, NULL,
						       10);
				debug("Blocksize ack: %s, %d\n",
					(char *)pkt+i+8, TftpBlkSize);
			}
#ifdef CONFIG_TFTP_TSIZE
			if (strcmp((char *)pkt+i, "tsize") == 0) {
				TftpTsize = simple_strtoul((char *)pkt+i+6,
							   NULL, 10);
				debug("size = %s, %d\n",
					 (char *)pkt+i+6, TftpTsize);
			}
#endif
		}
#ifdef CONFIG_CMD_TFTPPUT
		if (TftpWriting) {
			/* Get ready to send the first block */
			TftpState = STATE_DATA;
			TftpBlock++;
		}
#endif
		TftpSend(); /* Send ACK or first data block */
		break;
	case TFTP_DATA:
		if (len < 2)
			return;
		len -= 2;
		TftpBlock = ntohs(*(ushort *)pkt);

		//printf("\n TftpBlock=[%08X],(TftpBlock - 1) % 10) = %d",TftpBlock,((TftpBlock - 1) % 10));

		/*
		 * RFC1350 specifies that the first data packet will
		 * have sequence number 1. If we receive a sequence
		 * number of 0 this means that there was a wrap
		 * around of the (16 bit) counter.
		 */
		if (TftpBlock == 0) {
			TftpBlockWrap++;
			TftpBlockWrapOffset += TFTP_BLOCK_SIZE * TFTP_SEQUENCE_SIZE;
			printf ("\n\t %lu MB reveived\n\t ", TftpBlockWrapOffset>>20);
		} else {
			show_block_marker();
		}

#ifdef ET_DEBUG
		if (TftpState == STATE_RRQ) {
			puts ("Server did not acknowledge timeout option!\n");
		}
#endif

		if (TftpState == STATE_RRQ || TftpState == STATE_OACK ||
		    TftpState == STATE_RECV_WRQ) {
			/* first block received */
			TftpState = STATE_DATA;
			TftpServerPort = src;
			TftpLastBlock = 0;
			TftpBlockWrap = 0;
			TftpBlockWrapOffset = 0;
			//printf("\n first block received  \n");
			if (TftpBlock != 1) {	/* Assertion */
				printf ("\nTFTP error: "
					"First block is not block 1 (%ld)\n"
					"Starting again\n\n",
					TftpBlock);
				NetStartAgain ();
				break;
			}
		}

		if (TftpBlock == TftpLastBlock) {
			/*
			 *	Same block again; ignore it.
			 */
			printf("\n Same block again; ignore it \n"); 
			break;
		}

		TftpLastBlock = TftpBlock;
		NetSetTimeout (TIMEOUT * CFG_HZ, TftpTimeout);

		store_block (TftpBlock - 1, pkt + 2, len);

		/*
		 *	Acknoledge the block just received, which will prompt
		 *	the server for the next one.
		 */
		TftpSend ();

		if (len < TFTP_BLOCK_SIZE)
			tftp_complete();
		break;

	case TFTP_ERROR:
		printf ("\nTFTP error: '%s' (%d)\n",
					pkt + 2, ntohs(*(ushort *)pkt));
		puts ("Starting again\n\n");
		NetStartAgain ();
		break;
	default:
		return;

	}
}


static void
TftpTimeout (void)
{
	if (++TftpTimeoutCount > TIMEOUT_COUNT) {
		puts ("\nRetry count exceeded; starting again\n");
		NetStartAgain ();
	} else {
		puts ("T ");
		NetSetTimeout (TIMEOUT * CFG_HZ, TftpTimeout);
		TftpSend ();
	}
}


void
TftpStart (int protocol)
{
	TftpStarted=1;

	if (BootFile[0] == '\0') {
	    sprintf(default_filename, "%s","test.bin");
	    tftp_filename = default_filename;

		printf ("*** Warning: no boot file name; using '%s'\n",
			tftp_filename);
	} else {
		tftp_filename = BootFile;
	}

#if defined(CONFIG_NET_MULTI)
	printf ("Using %s device\n", eth_get_name());
#endif
	if (protocol == TFTPPUT)
		puts ("TFTP to server ");
	else
		puts ("TFTP from server ");
	print_IPaddr (NetServerIP);
	puts ("; our IP address is ");	print_IPaddr (NetOurIP);

	/* Check if we need to send across this subnet */
	if (NetOurGatewayIP && NetOurSubnetMask) {
	    IPaddr_t OurNet 	= NetOurIP    & NetOurSubnetMask;
	    IPaddr_t ServerNet 	= NetServerIP & NetOurSubnetMask;

	    if (OurNet != ServerNet) {
		puts ("; sending through gateway ");
		print_IPaddr (NetOurGatewayIP) ;
	    }
	}
	putc ('\n');

	printf ("Filename '%s'.", tftp_filename);

	if (NetBootFileSize) {
		printf (" Size is 0x%x Bytes = ", NetBootFileSize<<9);
		print_size (NetBootFileSize<<9, "");
	}

	putc ('\n');

#ifdef CONFIG_CMD_TFTPPUT
	TftpWriting = (protocol == TFTPPUT);
	if (TftpWriting) {
		printf("Save address: 0x%lx\n", save_addr);
		printf("Save size:    0x%lx\n", save_size);
		NetBootFileXferSize = save_size;
		puts("Saving: *\b");
		TftpState = STATE_SEND_WRQ;
		TftpFinalBlock = 0;
	} else
#endif
	{
		printf ("\n TIMEOUT_COUNT=%d,Load address: 0x%lx\n",TIMEOUT_COUNT,load_addr);
		puts ("Loading: *\b");
		TftpState = STATE_RRQ;
	}

	NetSetTimeout (TIMEOUT * CFG_HZ * 2, TftpTimeout);
	NetSetHandler (TftpHandler);

	TftpServerPort = WELL_KNOWN_PORT;
	TftpTimeoutCount = 0;
	TftpOurPort = 1024 + (get_timer(0) % 3072);
	TftpBlock = 0;

	/* zero out server ether in case the server ip has changed */
	memset(NetServerEther, 0, 6);
	
   
	TftpSend ();
}

#endif /* CFG_CMD_NET */
