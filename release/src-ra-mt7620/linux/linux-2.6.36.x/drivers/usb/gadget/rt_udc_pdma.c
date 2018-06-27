/*
 *	driver/usb/gadget/rt_udc.c
 *
 *	Copyright (C) 2009 Ying Yuan Huang, Ralink Tech. <yyhuang@ralink_tech.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

/*
 *  1) [ USB composite device ]. The USB PDMA architecture is not suitable for USB composite
 *     device support. A passive gadget driver(device) may slow down or block other gadget
 *     (device) because they are in the same ring.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/usb/ch9.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/usb_gadget.h>
#else
#include <linux/usb/gadget.h>
#endif

static const char driver_name[] = "rt_udc";
static const char ep0name[] = "ep0";
static unsigned debuglevel = 0;
module_param (debuglevel, uint, S_IRUGO);

#define DEBUG
#include "rt_udc.h"

#define PROC_DIR				driver_name
#define DEBUGLEVEL_PROCFILE		"debuglevel"
static struct proc_dir_entry *pProcDir   = NULL;
static struct proc_dir_entry *pProcDebugLevel = NULL;

/*
 * USB PDMA related
 */
#define NUM_RX_DESC     256
#define NUM_TX_DESC     256
#define RX_BUFF_SZ      1600			/* 1536 */
#define RING_RESET_TIMEOUT  3000                    /* 3 secs */
#define RX_RESCHEDULE   64
#define TX_RESCHEDULE   4
static unsigned dma = 0;
module_param (dma, uint, S_IRUGO);
static unsigned sm = 0;
module_param (sm, uint, S_IRUGO);
static unsigned int TXMAXCAP = 512;
module_param (TXMAXCAP, uint, S_IRUGO);

static struct PDMA_txdesc *tx_ring0_cache = NULL;
static struct PDMA_rxdesc *rx_ring0_cache = NULL;
static volatile struct PDMA_rxdesc *rx_ring0_noncache = NULL;
static volatile struct PDMA_txdesc *tx_ring0_noncache = NULL;
static dma_addr_t tx_ring_bus_addr;
static dma_addr_t rx_ring_bus_addr;

static int rx_dma_owner_idx0;                             /* Point to the next RXD DMA wants to use in RXD Ring#0.  */
static int tx_cpu_owner_idx0;
static int tx_need_free_idx0;

static volatile unsigned char *USBRxPackets[NUM_RX_DESC];           /* Receive packets */
static unsigned char tx_zlp_dummy_buf[8];
struct tasklet_struct rx_dma_tasklet;
struct tasklet_struct tx_dma_tasklet;

static struct rt_udc_struct controller;
static struct rt_request *handle_outep(struct rt_ep_struct *rt_ep);

static int debuglevel_read(char *page, char **start, off_t off,int count, int *eof, void *data)
{
	int len;
	sprintf(page, "%d\n", debuglevel);
	len = strlen(page) + 1;
	*eof = 1;
	return len;
}

static int debuglevel_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char tmp[32];
	count = (count > 32) ? 32 : count;
	memset(tmp, 0, 32);
	if (copy_from_user(tmp, buffer, count))
		return -EFAULT;
	debuglevel = simple_strtol(tmp, 0, 10);
	return count;
}

static void ep0_chg_stat(const char *label, struct rt_udc_struct *rt_usb, enum ep0_state stat)
{
	xprintk("<0st>%s->%s\n", state_name[rt_usb->ep0state], state_name[stat]);

	if (rt_usb->ep0state == stat)
		return;
	rt_usb->ep0state = stat;
}

static u8 read_epcs(struct rt_ep_struct *rt_ep)
{
	int idx = EP_NO(rt_ep);
	int dir = EP_DIR(rt_ep);

	if(idx == 0)
		return usb_read(EP0CS);

	return (dir == EP_IN ? usb_read(0x7 + idx*8) : usb_read(0x3 + idx*8) );
}

static void write_epcs(struct rt_ep_struct *rt_ep, u8 val)
{
	int idx = EP_NO(rt_ep);
	int dir = EP_DIR(rt_ep);

	if(idx == 0)
		usb_write(EP0CS, val);
	else
		(dir == EP_IN ? /*IN */ usb_write(0x7 + idx*8, val) : usb_write(0x3 + idx*8, val) );
}

static u16 read_inbc(int epnum)
{
	u16 low, high = 0;
	if(epnum == 0){ /* EP0 */
		low = usb_read(IN0BC);
	}else{
		low = usb_read(epnum * 8 + 4);
		high = usb_read((epnum * 8 + 4)+1);
	}
	return (low | (high << 8));
}

static u16 read_outbc(int epnum)
{
	u16 low, high = 0;
	if(epnum == 0){ /* EP0 */
		low = usb_read(OUT0BC);
	}else{
		low = usb_read(epnum * 8);
		high = usb_read((epnum * 8)+1);
	}
	return (low | (high << 8));
}


static void rt_all_eps_reset(void)
{
	// reset(toggle & fifo) all 16 IN & 16 OUT endpoints
	usb_write(ENDPRST, 0x10);
	usb_write(ENDPRST, 0x70);
	usb_write(ENDPRST, 0x00);
	usb_write(ENDPRST, 0x60);
}

static void rt_ep_rst(struct rt_ep_struct *rt_ep)
{
	u8 reg = 0;
	u8 idx = EP_NO(rt_ep);
	u8 dir = EP_DIR(rt_ep);
	if(dir == EP_IN )
		reg |= ENDPRST_IO | idx;
	usb_write(ENDPRST, reg);

	reg |= ENDPRST_TOGRST | ENDPRST_FIFORST;
	usb_write(ENDPRST, reg);	        
}

static void rt_ep_irq_enable(struct rt_ep_struct *rt_ep)
{
	u8 reg;
	u8 idx = EP_NO(rt_ep);
	u8 dir = EP_DIR(rt_ep);

	if(idx == 0 /* ep0 */){
		usb_write(IN07IEN, (usb_read(IN07IEN) | 0x1) );
		usb_write(OUT07IEN, (usb_read(OUT07IEN) | 0x1) );
	}else{   /* epX */
		reg = usb_read(dir ? IN07IEN : OUT07IEN);
		reg = reg | (0x1 << idx);
		usb_write(dir == EP_IN ? IN07IEN : OUT07IEN, reg);
		reg = usb_read(dir ? IN07IEN : OUT07IEN);
	}
}

static void rt_udc_init_ep(struct rt_udc_struct *rt_usb)
{
	DBG;
	if(dma){
#if defined (CONFIG_RALINK_RT3883) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
		usb_write(IN1CON, 0x8D);  // InEP1 : Int, 2 subfifos
		usb_write(IN2CON, 0x89);  // InEP2 : Bulk, 2 subfifos
		usb_write(OUT1CON, 0x8D);  // OutEP1 : Int, 2 subfifos
		usb_write(OUT2CON, 0x89);  // OutEP2 : Bulk, 2 subfifos
		//usb_write(OUT3CON, 0x89);  // OutEP3 : Bulk, 2 subfifos
		//usb_write(OUT4CON, 0x89);  // OutEP4 : Bulk, 2 subfifos
#elif defined (CONFIG_RALINK_RT5350))
		usb_write(IN1CON, 0x89);	// InEP1  : BULK, 2 subfifos
		usb_write(OUT1CON, 0x89);	// OutEP1 : BULK, 2 subfifos
#else
#error	"define a platform"
#endif
	}else{
#if defined (CONFIG_RALINK_RT3883) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
		usb_write(IN1CON, 0x8C);	// InEP1 : Int  , 1 subfifos
		usb_write(IN2CON, 0x88);	// InEP2 : Bulk, 1 subfifo
		usb_write(OUT1CON, 0x8C);	// OutEP1 : Int, 1 subfifos
		usb_write(OUT2CON, 0x88);	// OutEP2 : Bulk, 1 subfifos
		//usb_write(OUT3CON, 0x88);	// OutEP3 : Bulk, 1 subfifo
		//usb_write(OUT4CON, 0x88);	// OutEP4 : Bulk. 1 subfifo

#elif defined (CONFIG_RALINK_RT5350))
		usb_write(IN1CON, 0x88);	// InEP1  : BULK  , 1 subfifos
		usb_write(OUT1CON, 0x88);	// OutEP1 : BULK, 1 subfifos
#else
#error	"define a platform"
#endif
	}
	// clear all pending HW interrupts
	usb_write(IN07IRQ, 0xFF);
	usb_write(OUT07IRQ, 0xFF);
	rt_all_eps_reset();
	rt_ep_irq_enable(&rt_usb->rt_ep[0]);
}

static void rt_udc_init_fifo(struct rt_udc_struct *rt_usb)
{
	// fifo control
	if(dma){
		usb_write(FIFOCTRL, 0x31);	// INEP1, Autoin = 1
		usb_write(FIFOCTRL, 0x32);	// INEP2, Autoin = 1
		usb_write(FIFOCTRL, 0x21);	// OUTEP1, Autoin = 1
		usb_write(FIFOCTRL, 0x22);	// OUTEP2, Autoin = 1
		//usb_write(FIFOCTRL, 0x23);// OUTEP3, Autoin = 1
		//usb_write(FIFOCTRL, 0x24);// OUTEP4, Autoin = 1

		usb_write(FIFOCTRL, 0x00);	// Access by DMA
	}else{
		usb_write(FIFOCTRL, 0x11);	// INEP1, Autoin = 0
		usb_write(FIFOCTRL, 0x12);	// INEP2, Autoin = 0
		usb_write(FIFOCTRL, 0x01);	// OUTEP1, Autoin = 0
		usb_write(FIFOCTRL, 0x02);	// OUTEP2, Autoin = 0
		//usb_write(FIFOCTRL, 0x03);// OUTEP3, Autoin = 0
		//usb_write(FIFOCTRL, 0x04);// OUTEP4, Autoin = 0

		usb_write(FIFOCTRL, 0x80);	// Access by CPU
	}
}

static void rt_udc_init(struct rt_udc_struct *rt_usb)
{
	/* Setup & Init endpoints */
	rt_udc_init_ep(rt_usb);

	// Enable HS, reset, suspend, SETUP valid data interrupt
	usb_write(USBIRQ, 0xff);			// clear first
	usb_write(USBIEN, 0x21);

	/* Setup ep fifos */
	rt_udc_init_fifo(rt_usb);
}

static void rt_ep_irq_disable(struct rt_ep_struct *rt_ep)
{
	u8 reg;
	u8 idx = EP_NO(rt_ep);
	u8 dir = EP_DIR(rt_ep);

	if(idx == 0 /* ep0 */){
		usb_write(IN07IEN, (usb_read(IN07IEN) & ~(0x1)) );
		usb_write(OUT07IEN, (usb_read(OUT07IEN) & ~(0x1)) );
	}else{
		reg = usb_read(dir ? IN07IEN : OUT07IEN);
		reg = reg & ~(0x1 << idx);
		usb_write(dir == EP_IN ? IN07IEN : OUT07IEN, reg);
		reg = usb_read(dir ? IN07IEN : OUT07IEN);
	}
}

static u32 rt_fifo_bcount(struct rt_ep_struct *rt_ep)
{
	u8 low, high;
	u32 rc;

	int idx = EP_NO(rt_ep);
	int dir = EP_DIR(rt_ep);

	if(idx == 0)
		return 0;

	if(dir /* IN */){
		low = usb_read(0x004 + idx*8);
		high = usb_read( (0x004 + idx*8)+1 );
	}else{  /* OUT */
		low = usb_read(0x000 + idx*8);
		high = usb_read( (0x000 + idx*8)+1 );
	}
	rc = high | low;
	return rc;
}

void rt_flush(struct rt_ep_struct *rt_ep)
{
	rt_ep_rst(rt_ep);
}

void rt_ep_stall(struct rt_ep_struct *rt_ep, int value)
{
	u8 tmp;
	u32 addr;
	int idx = EP_NO(rt_ep);
	int dir = EP_DIR(rt_ep);

	if(idx == 0){
		tmp = usb_read(EP0CS);
		if(value)
		tmp |= 0x1;
		else
			tmp &= ~(0x1);
		usb_write(EP0CS, tmp);
	}else{
		addr = (dir == EP_IN ? 0x006 : 0x002) + idx * 8;
		tmp = usb_read(addr);
		if(value)
		tmp |= 0x40;
		else
			tmp &= ~(0x40);
		usb_write(addr, tmp);
	}

	return;
}

static int rt_udc_get_frame(struct usb_gadget *_gadget)
{
	return 0;
}

static int rt_udc_wakeup(struct usb_gadget *_gadget)
{
	DBG;
	return 0;
}


/*******************************************************************************
 * USB request control functions
 *******************************************************************************
 */
static inline void ep_add_request(struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	if (unlikely(!req))
		return;
	req->in_use = 1;
	req->zlp_dma_done = 0;
	req->rt_ep = rt_ep;
	list_add_tail(&req->queue, &rt_ep->queue);
}

static inline void ep_del_request(struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	if (unlikely(!req))
		return;
	list_del_init(&req->queue);
	req->zlp_dma_done = 0;
	req->in_use = 0;
}

static void done(struct rt_ep_struct *rt_ep, struct rt_request *req, int status)
{
	ep_del_request(rt_ep, req);

	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (status && status != -ESHUTDOWN)
		D_ERR(rt_ep->rt_usb->dev, "<%s> complete %s req %p stat %d len %u/%u\n", __func__, rt_ep->ep.name, &req->req, status,req->req.actual, req->req.length);

	req->req.complete(&rt_ep->ep, &req->req);
}

#if 0
/* for reference */
struct tasklet_struct rx_tasklet_tmp;
static void rx_done_do_tasklet(unsigned long arg)
{
	struct rt_ep_struct 	*rt_ep;
	struct rt_request 		*rt_req;
	struct usb_request 		*usb_req;
	struct usb_ep 			*ep;
	int						i, rx_count, status = 0;
	struct rt_udc_struct 	*rt_usb = &controller;

	for (i = (IN_EP_NUM+1); i < RT_USB_NB_EP; i++) {
		rt_ep = &rt_usb->rt_ep[i];
		ep = &rt_ep->ep;

		// shared by irq handler, protect it
		spin_lock_irqsave(&rx_done_lock, rx_done_lock_flags);
		rx_count = rt_ep->rx_done_count;

		//spin_unlock_irqrestore(&rx_done_lock, rx_done_lock_flags);

		for (;rx_count > 0; rx_count--) {
			if(unlikely(list_empty(&rt_ep->queue)))
				FATAL_ERROR("empty queue");

			rt_req = list_entry(rt_ep->queue.next, struct rt_request, queue);
			usb_req = &rt_req->req;

			ep_del_request(rt_ep, rt_req);
			rt_ep->rx_done_count--;

			spin_unlock_irqrestore(&rx_done_lock, rx_done_lock_flags);

			if (unlikely(rt_req->req.status == -EINPROGRESS))
				rt_req->req.status = status;
			else
				status = rt_req->req.status;

			if (unlikely(status && status != -ESHUTDOWN))
				D_ERR(rt_ep->rt_usb->dev, "<%s> complete %s req %p stat %d len %u/%u\n", __func__, rt_ep->ep.name, &rt_req->req, status,rt_req->req.actual, rt_req->req.length);

			// indicate gadget driver.
			usb_req->complete(ep, usb_req);

			spin_lock_irqsave(&rx_done_lock, rx_done_lock_flags);
		}
		spin_unlock_irqrestore(&rx_done_lock, rx_done_lock_flags);
    }
}
#endif

struct tasklet_struct tx_tasklet;
static void tx_do_tasklet(unsigned long arg)
{
	return;
}

struct tasklet_struct rx_tasklet;
static void rx_do_tasklet(unsigned long arg)
{
	struct rt_ep_struct 	*rt_ep;
	struct rt_request 		*req;
	struct usb_ep 			*ep;
	int						i;
	struct rt_udc_struct 	*rt_usb = &controller;

	for (i = (IN_EP_NUM+1/* EP0 */); i < RT_USB_NB_EP; i++){
		u8 epcs;
		rt_ep = &rt_usb->rt_ep[i];
		ep = &rt_ep->ep;

		epcs = read_epcs(rt_ep);        
		while(!(epcs & EP_CS_BSY)){
			req = handle_outep(rt_ep);
			if(!req){
				// No usb request found.
				// Just set up the flag (pending) and clear int.
				rt_ep->pending = 1;
				break;
	        }else{
				if(req && ( (req->req.actual % rt_ep->ep.maxpacket) || (req->req.actual >= req->req.length))){
					xprintk("q.l=%d,q.a=%d\n", req->req.length, req->req.actual);
					done(rt_ep, req, 0);
				}
			}

	        epcs = read_epcs(rt_ep);
	        write_epcs(rt_ep, 0x0);
			epcs = read_epcs(rt_ep);
		}
    }
}

static void nuke(struct rt_ep_struct *rt_ep, int status)
{
	struct rt_request *req;

	DBG;
	while (!list_empty(&rt_ep->queue)) {
		req = list_entry(rt_ep->queue.next, struct rt_request, queue);
		done(rt_ep, req, status);
	}
}

/*
 *******************************************************************************
 * Data tansfer over USB functions
 *******************************************************************************
 */
static int read_ep0_fifo(struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	u8	*buf;
	int	byte_count, req_bufferspace, count, i;

DBG;
	if(!in_irq())
		FATAL_ERROR("not irq context.");

	byte_count = read_outbc(EP_NO(rt_ep));
	req_bufferspace = req->req.length - req->req.actual;

	buf = req->req.buf + req->req.actual;

	if(!req_bufferspace)
		FATAL_ERROR("zlp");

	if(byte_count > req_bufferspace)
		FATAL_ERROR("buffer overflow, byte_count=%d, req->req.length=%d, req->req.actual=%d\n", byte_count, req->req.length ,req->req.actual);

	count = min(byte_count, req_bufferspace);

	//test, Access by CPU
	if(dma)
		usb_write(FIFOCTRL, 0x80);

	for (i = 0; i < count; i++){
		*buf = usb_read(EP0OUTDAT+i);
		buf++;
	}
	req->req.actual += count;

	//test, Access by DMA
	if(dma)
		usb_write(FIFOCTRL, 0x00);

	return count;
}


static int read_ep_fifo(struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	u8	*buf, ep_no, ep_no_shift;
	int	byte_count, req_bufferspace, count, i;

DBG;
	ep_no = EP_NO(rt_ep);

	byte_count = read_outbc(ep_no);
	if(unlikely(!byte_count))
		FATAL_ERROR("ep_no:%d bc = 0", ep_no);

	req_bufferspace = req->req.length - req->req.actual;

	buf = req->req.buf + req->req.actual;

	if(unlikely(!req_bufferspace))
		FATAL_ERROR("zlp");

	xprintk("bc=%d,r.l=%d,r.a=%d\n", byte_count, req->req.length ,req->req.actual);
	if(unlikely(byte_count > req_bufferspace))
		FATAL_ERROR("buffer overflow, byte_count=%d, req->req.length=%d, req->req.actual=%d\n", byte_count, req->req.length ,req->req.actual);

	count = min(byte_count, req_bufferspace);

	ep_no_shift = 0x80+ep_no * 4;
	for (i = 0; i < count; i++){
		*buf = usb_read(ep_no_shift);
		buf++;
	}

	req->req.actual += count;

	// EP Out irq handler would arm another transaction.
	return count;
}

static int write_ep_fifo_zlp(struct rt_ep_struct *rt_ep)
{
	u8	epcs;
	int	ep_no = EP_NO(rt_ep);

DBG;
	xprintk("w%d ZLP\n", EP_NO(rt_ep));
	epcs = read_epcs(rt_ep);
	if(epcs & EP_CS_BSY)
		FATAL_ERROR("EP%d busy. cs=%x\n", ep_no, epcs);

	/* check INEP byte count is zero? */
	if(read_inbc(ep_no))
		FATAL_ERROR("EP%d bc zero. bc=%d\n", ep_no, read_inbc(ep_no));

	epcs = read_epcs(rt_ep);
	write_epcs(rt_ep, epcs);
	return 0;
}


/*
 * handle_epinirq()
*/
static int write_ep_fifo(struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	u8	*buf, epcs;
	int	length, i, ep_no = EP_NO(rt_ep);

DBG;
	xprintk("w ep%d req=%p,r.l=%d,r.a=%d\n",EP_NO(rt_ep),&req->req,req->req.length,req->req.actual);
	epcs = read_epcs(rt_ep);
	if(epcs & EP_CS_BSY)
		FATAL_ERROR("EP%d busy. epcs=%x\n", ep_no, epcs);

	/* check INEP byte count is zero? */
	if(read_inbc(ep_no))
		FATAL_ERROR("EP%d bc=%d\n", ep_no, read_inbc(ep_no));

	buf = req->req.buf + req->req.actual;
	length = (req->req.length - req->req.actual) < rt_ep->ep.maxpacket ? (req->req.length - req->req.actual) : rt_ep->ep.maxpacket;
	req->req.actual += length;
	if (!length) {	/* zlp */
		// for debug
		xprintk("<%s> zero packet\n", __func__);
		write_ep_fifo_zlp(rt_ep);
		return 0;
	}

	// write to ep in fifo
	for (i=0; i< length; i++)
		usb_write(0x80+ep_no*4, *buf++);

	epcs = read_epcs(rt_ep);
	write_epcs(rt_ep, epcs);

	return length;
}

/*
 *
 */
static int write_ep0_fifo(struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	u8	*buf;
	int	length, i;
	u32 maxpacket;

DBG;
	xprintk("q.l=%d, q.a=%d, maxp=%d\n", req->req.length, req->req.actual, rt_ep->ep.maxpacket);

	buf = req->req.buf + req->req.actual;
	maxpacket = (u32)(rt_ep->ep.maxpacket);
	length = min(req->req.length - req->req.actual, maxpacket);

	req->req.actual += length;

	if (!length && req->req.zero)
		FATAL_ERROR("zlp");

	if(!in_irq())
		FATAL_ERROR("Not in irq context");

	//test, Access by CPU
	if(dma)
		usb_write(FIFOCTRL, 0x80);

	//write to ep0in fifo
	for (i=0; i< length; i++)
		usb_write(EP0INDAT+i, *buf++);

	// arm ep0in
	usb_write(IN0BC, length);
	if(length != rt_ep->ep.maxpacket)
		usb_write(EP0CS, 0x2);		// clear NAK bit to ACK host.

	//test, Access by CPU
	if(dma)
		usb_write(FIFOCTRL, 0x00);

	return length;
}

static struct rt_request *get_unhandled_req(struct rt_ep_struct *rt_ep)
{
	struct list_head *p;
	struct rt_request *req = NULL;

	if(EP_DIR(rt_ep) == EP_OUT)
		FATAL_ERROR("Out EP");

	if(dma){
		list_for_each(p, &rt_ep->queue){
			req = list_entry(p, struct rt_request, queue);
			if(req->req.length > req->req.actual )
				return req;
			else if(unlikely(req->req.length == 0 && req->zlp_dma_done == 0))
				return req;
			else
				continue;
		}
		return NULL;
	}else{
		if (!list_empty(&rt_ep->queue)){
			req = list_entry(rt_ep->queue.next, struct rt_request, queue);
		}else {
			FATAL_ERROR("%s No request", rt_ep->ep.name);
		}
		return req;
	}
}

#define PADDING_LENGTH 64
static int write_dma_txring(struct rt_ep_struct *rt_ep,struct rt_request *req)
{
	u8 *buf;
	int length;
	int retry_times = 0;
	u32 hw_current_idx;
DBG;
	xprintk("w%dr=%p,r.l=%d,r.a=%d\n", EP_NO(rt_ep), &req->req,req->req.length,req->req.actual);

	length = req->req.length;

	while(length > 0 || (req->req.length == 0 && req->zlp_dma_done == 0)){
retry:
	/* wait for a free TXD */
	hw_current_idx = reg_read(RTUSB_TX_DTX_IDX0);
	if ( tx_ring0_cache[tx_cpu_owner_idx0].txd_info2.DDONE_bit == 0 ||
		((tx_cpu_owner_idx0+1) % NUM_TX_DESC == hw_current_idx) ) {
		if(retry_times > 1000)
			return -1;
		mdelay(1);
		retry_times++;
		goto retry;
	}

		if(length > TXMAXCAP)
			length = TXMAXCAP;

		buf = req->req.buf + req->req.actual;
		req->req.actual += length;

	/* deal with ZLP.*/
		if(req->req.length == 0 && req->zlp_dma_done == 0)
		req->zlp_dma_done = 1;

		req->txd_count++;

#define phys_to_bus(a) ((u32)a & 0x1FFFFFFF)
		if(length){
			tx_ring0_cache[tx_cpu_owner_idx0].txd_info1.SDP0 = cpu_to_le32(phys_to_bus(buf));
			tx_ring0_cache[tx_cpu_owner_idx0].txd_info2.SDL0 = cpu_to_le32(length);
			tx_ring0_cache[tx_cpu_owner_idx0].txd_info4.zlp_flag = 0;
			dma_cache_sync(NULL, (void *)buf, length, DMA_TO_DEVICE);
		}else{
			tx_ring0_cache[tx_cpu_owner_idx0].txd_info1.SDP0 = cpu_to_le32(phys_to_bus(tx_zlp_dummy_buf));
			tx_ring0_cache[tx_cpu_owner_idx0].txd_info2.SDL0 = cpu_to_le32(sizeof(tx_zlp_dummy_buf));
			tx_ring0_cache[tx_cpu_owner_idx0].txd_info4.zlp_flag = 1;
		}

		tx_ring0_cache[tx_cpu_owner_idx0].txd_info4.In_ep_addr = cpu_to_le32(EP_NO(rt_ep));
		tx_ring0_cache[tx_cpu_owner_idx0].txd_info2.DDONE_bit = 0;
		tx_cpu_owner_idx0 = (tx_cpu_owner_idx0 + 1) % NUM_TX_DESC;

		length = req->req.length - req->req.actual;
	}

	reg_write(RTUSB_TX_CTX_IDX0, tx_cpu_owner_idx0);

	return 0;
}


/*******************************************************************************
 * Endpoint handlers
 *******************************************************************************
 */

/*
 *  Handle In Endpoint
 *  CPU(FIFO):
 *			Enqueue -> Write fifo -> TX_DONE -> Write fifo -> TX_DONE -> ..
 *
 *  DMA
 *			Enqueue -> Kick off TxD.   Enqueue -> Kick off TxD.   Enqueue -> Kick off TxD. 
 */
static int handle_inep(struct rt_ep_struct *rt_ep)
{
	struct rt_request *req;

DBG;
	if(!(req = get_unhandled_req(rt_ep)))
		return -1;

	if(dma){
		write_dma_txring(rt_ep, req);
	}else{
		write_ep_fifo(rt_ep, req);
		rt_ep->tx_done_count = 1;
	}
	return 0;
}

/*
 * IRQ context.
 */
static struct rt_request *handle_outep(struct rt_ep_struct *rt_ep)
{
	struct rt_request *req;
	struct list_head *p;
	int count = 0;

DBG;
	if (list_empty(&rt_ep->queue)){
		return NULL;
	}

	list_for_each(p, &rt_ep->queue){
		if(count != rt_ep->rx_done_count){
			count++;       
			continue;
		}
		req = list_entry(p, struct rt_request, queue);
		read_ep_fifo(rt_ep, req);
		return req;
	}

	return NULL;
}

static struct rt_request *handle_inep0(struct rt_ep_struct *rt_ep)
{
	struct rt_request *req = NULL;

DBG;
	if (list_empty(&rt_ep->queue)) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> no request on %s\n", __func__, rt_ep->ep.name);
		return NULL;
	}

	req = list_entry(rt_ep->queue.next, struct rt_request, queue);
	switch (rt_ep->rt_usb->ep0state) {
		case EP0_IN_DATA_PHASE:			/* GET_DESCRIPTOR */
			write_ep0_fifo(rt_ep, req);
			break;

		// Impossible:
		//case EP0_OUT_DATA_PHASE:		/* SET_DESCRIPTOR */
		//case EP0_NO_DATA_PHASE:		/* for no data stage control transfer */

		default:
			D_ERR(rt_ep->rt_usb->dev, "<%s> ep0 i/o, odd state %d\n", __func__, rt_ep->rt_usb->ep0state);
			ep_del_request(rt_ep, req);
			req = NULL;
			break;
	}

	return req;
}

static struct rt_request *handle_outep0(struct rt_ep_struct *rt_ep)
{
	struct rt_request *req = NULL;

DBG;
	if (list_empty(&rt_ep->queue)) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> no request on %s\n", __func__, rt_ep->ep.name);
		return NULL;
	}

	if(rt_ep->rt_usb->ep0state != EP0_OUT_DATA_PHASE){
		D_EP0(rt_ep->rt_usb->dev, "<%s> ep0 i/o, odd state %d\n", __func__, rt_ep->rt_usb->ep0state);
		ep_del_request(rt_ep, req);
		req = NULL;
	}

	req = list_entry(rt_ep->queue.next, struct rt_request, queue);

	read_ep0_fifo(rt_ep, req);

	return req;
}

/*******************************************************************************
 * USB gadget callback functions
 *******************************************************************************
 */
static void handle_dma_rxdone(struct rt_udc_struct *rt_usb)
{
	DBG;
	tasklet_schedule(&rx_dma_tasklet);
}

static void handle_dma_txdone(struct rt_udc_struct *rt_usb)
{
	DBG;
	tasklet_schedule(&tx_dma_tasklet);
}

static void handle_dmairq(struct rt_udc_struct *rt_usb, u32 irq)
{
	if(irq & RTUSB_RX_DONE_INT0){
		handle_dma_rxdone(rt_usb);
	}

	if(irq & RTUSB_TX_DONE_INT0){
		handle_dma_txdone(rt_usb);
	}

	reg_write(RTUSB_INT_STATUS, irq);
}

static inline int udc_dma_reset_txring(void)
{
    int count = 0;
    u32 reg;

    while(count++< RING_RESET_TIMEOUT){
        reg = reg_read(RTUSB_PDMA_GLO_CFG);
        if(reg & RTUSB_TX_DMA_BUSY){
            mdelay(1);
        }else
            break;

    }
    if(count== RING_RESET_TIMEOUT)
        return -1;

    reg = reg_read(RTUSB_PDMA_RST_IDX);
    udelay(100);
    reg |= (RTUSB_RST_DTX_IDX1 | RTUSB_RST_DTX_IDX0);
    reg_write(RTUSB_PDMA_RST_IDX, reg);
    udelay(100);
    return 0;
}

static inline int udc_dma_reset_rxring(void)
{
    int count = 0;
    u32 reg;

    while(count++< RING_RESET_TIMEOUT){
        reg = reg_read(RTUSB_PDMA_GLO_CFG);
        if(reg & RTUSB_RX_DMA_BUSY){
            mdelay(1);
        }else
            break;
    }
    if(count== RING_RESET_TIMEOUT)
        return -1;

    reg = reg_read(RTUSB_PDMA_RST_IDX);
    udelay(100);
    reg |= (RTUSB_RST_DRX_IDX1 | RTUSB_RST_DRX_IDX0);
    reg_write(RTUSB_PDMA_RST_IDX, reg);
    udelay(100);\
    return 0;
}

static int udc_dma_hw_reset(void)
{
    if(udc_dma_reset_rxring() == -1)
        return -1;
    if(udc_dma_reset_txring() == -1)
        return -1;
    return 0;
}

static void udc_dma_enable(int enable)
{
    u32 reg;
    reg = reg_read(RTUSB_PDMA_GLO_CFG);
    udelay(100);
    if(enable)
        reg |= RTUSB_TX_WB_DDONE | RTUSB_RX_DMA_EN | RTUSB_TX_DMA_EN ;
    else
        reg &= ~(RTUSB_TX_WB_DDONE | RTUSB_RX_DMA_EN | RTUSB_TX_DMA_EN) ;
    reg_write(RTUSB_PDMA_GLO_CFG, reg);
    udelay(500);
}

static void udc_dma_int_enable(int enable)
{
    u32 reg;
    reg = reg_read(RTUSB_INT_MASK);
    udelay(100);
    if(enable)
        reg |=  RTUSB_RX_DONE_INT_MSK0 | RTUSB_TX_DONE_INT_MSK0 ;
    else
        reg &= ~(RTUSB_RX_DONE_INT_MSK0 | RTUSB_TX_DONE_INT_MSK0) ;
    reg_write(RTUSB_INT_MASK, reg);
    udelay(100);
}

static inline void udc_dma_tx_int_clear(void)
{
    reg_write(RTUSB_INT_STATUS, 0x0000000F);
}

static inline void udc_dma_rx_int_clear(void)
{
    reg_write(RTUSB_INT_STATUS, 0x00030000);
}

static inline void udc_dma_all_int_clear(void)
{
    reg_write(RTUSB_INT_STATUS, 0xFFFFFFFF);
}

static int copy_data_to_ep(void *src, int length, int ep_num)
{
	struct rt_ep_struct *rt_ep;
	struct rt_udc_struct *rt_usb = &controller;
	struct rt_request *req;
	int req_bufferspace, count;
	u8 *buf;

	DBG;
	rt_ep = &rt_usb->rt_ep[ep_num+IN_EP_NUM];

	if (list_empty(&rt_ep->queue)){
		/* It is safe to return 0 if no req queued. */
		return 0;
	}

	req = list_entry(rt_ep->queue.next, struct rt_request, queue);
	req_bufferspace = req->req.length - req->req.actual;

    if(unlikely(!req_bufferspace)){
        // for debug
        FATAL_ERROR("zlp");
        return -1;
    }

    if(length > req_bufferspace){
        FATAL_ERROR("buffer overflow");
        return -1;
    }

    // sync with cache.
    if(likely(length))
        dma_cache_sync(NULL, src, length, DMA_FROM_DEVICE);

    buf = req->req.buf + req->req.actual;
    count = min(length, req_bufferspace);
    memcpy(buf, src, count);

    req->req.actual += count;

	if((req->req.actual % rt_ep->ep.maxpacket) || (req->req.actual >= req->req.length)){
		done(rt_ep, req, 0);    // short packet indicates transaction is done.
	}
    return count;
}

static void rx_dma_done_do_tasklet(unsigned long arg)
{
	u32 *rxd_info;
	u32 length;
	int ep, rc;
	int processed_count=0;

	DBG;
	for (;;){
		if (rx_ring0_cache[rx_dma_owner_idx0].rxd_info2.DDONE_bit == 0)
			break;

		if(processed_count++ > RX_RESCHEDULE){
			tasklet_schedule(&rx_dma_tasklet);
			break;
		}

		length = rx_ring0_cache[rx_dma_owner_idx0].rxd_info4.Rx_bcnt;
		ep = rx_ring0_cache[rx_dma_owner_idx0].rxd_info4.Out_ep_addr;

		// copy data from RXD->buffer to ep queue.
		rc = copy_data_to_ep((void *)USBRxPackets[rx_dma_owner_idx0], length, ep);
		if(rc <= 0)
			return;

		rxd_info = (u32 *)&rx_ring0_cache[rx_dma_owner_idx0].rxd_info4;
		*rxd_info = 0;

		/* clear DDONE bit*/
		rxd_info = (u32 *)&rx_ring0_cache[rx_dma_owner_idx0].rxd_info2;
		*rxd_info = 0;
		//rx_ring0_cache[rx_dma_owner_idx0].rxd_info2.DDONE_bit = 0;
		//rx_ring0_cache[i].rxd_info2.LS0= 0;
		rx_ring0_cache[rx_dma_owner_idx0].rxd_info2.PLEN0= sizeof(u8) * RX_BUFF_SZ;

		/*  Move point to next RXD which wants to alloc */
		//OUTL(cpu_to_le32((u32) rx_dma_owner_idx0), RTUSB_RX_CALC_IDX0);
		reg_write(RTUSB_RX_CALC_IDX0, rx_dma_owner_idx0);

		/* Update to Next packet point that was received.
		*/
		rx_dma_owner_idx0 = (rx_dma_owner_idx0 + 1) % NUM_RX_DESC;
    }
}

/*
 *  Recycle reqs and call gadget complete callback function.
 */
static void tx_dma_done_do_tasklet(unsigned long arg)
{
	int ep_num;
	u32 hw_current;
	struct rt_ep_struct *rt_ep;
	struct rt_request   *rt_req;
	struct rt_udc_struct *rt_usb = &controller;

	DBG;
	while(tx_need_free_idx0 != (hw_current = reg_read(RTUSB_TX_DTX_IDX0))){
		int retry = 0;
		while(tx_ring0_cache[tx_need_free_idx0].txd_info2.DDONE_bit != 1){
			mdelay(1);
			retry++;
			if(retry > 1000)
				FATAL_ERROR("tx timeout");
		}

		// rt_ep = tx_ring0_req_mapping[tx_need_free_idx0];
		ep_num = tx_ring0_cache[tx_need_free_idx0].txd_info4.In_ep_addr;
		if(!ep_num || ep_num > IN_EP_NUM)
			FATAL_ERROR("Out of range");

		rt_ep = &rt_usb->rt_ep[ep_num];
		if(list_empty(&rt_ep->queue))
			FATAL_ERROR("ep[%d] No request", ep_num);

		rt_req = list_entry(rt_ep->queue.next, struct rt_request, queue);
		rt_req->txd_count--;


		if(rt_req->txd_count == 0)
			done(rt_ep, rt_req, 0);

		tx_need_free_idx0 = (tx_need_free_idx0 + 1) % NUM_TX_DESC;

	}
}

static int udc_dma_rst(void)
{
	if( udc_dma_reset_txring() == -1)
		return -1;
	if( udc_dma_reset_rxring() == -1)
		return -1;

	tx_cpu_owner_idx0 = 0;
	tx_need_free_idx0 = 0;
	rx_dma_owner_idx0 = 0;
	reg_write(RTUSB_RX_CALC_IDX0, cpu_to_le32(NUM_RX_DESC - 1));
	return 0;
}

static int rt_udc_dma_init(void)
{
	int i;
	
	if( udc_dma_hw_reset() == -1)
		return -1;

	for(i=0; i<NUM_RX_DESC; i++){
		USBRxPackets[i]  = kmalloc(sizeof(u8) * RX_BUFF_SZ, GFP_ATOMIC | GFP_DMA); // todo: use  GFP_KERNEL instead.
		if(!USBRxPackets[i]){
			for(i=i-1; i>=0; i--)
				kfree((void *)USBRxPackets[i]);
			printk("No mem.");
			return -1;
		}
	}

	tx_ring0_cache = dma_alloc_coherent(NULL, sizeof(struct PDMA_txdesc) * NUM_TX_DESC, &tx_ring_bus_addr, GFP_KERNEL);
	rx_ring0_cache = dma_alloc_coherent(NULL, sizeof(struct PDMA_rxdesc) * NUM_RX_DESC, &rx_ring_bus_addr, GFP_KERNEL);

	printk("USB PDMA mode enabled.\n");
	printk("tx_ring=%p\n", tx_ring0_cache);
	printk("rx_ring=%p\n", rx_ring0_cache);

	tx_ring0_noncache = tx_ring0_cache;
	rx_ring0_noncache = rx_ring0_cache;

	for(i=0; i < NUM_RX_DESC; i++){
		memset((void *)&rx_ring0_noncache[i], 0, 16 /* sizeof()*/);
		rx_ring0_noncache[i].rxd_info2.DDONE_bit = 0;
		rx_ring0_noncache[i].rxd_info2.LS0= 0;
		rx_ring0_noncache[i].rxd_info2.PLEN0= sizeof(u8) * RX_BUFF_SZ;
		rx_ring0_noncache[i].rxd_info1.PDP0 = dma_map_single(NULL, (void *)USBRxPackets[i], sizeof(u8) * RX_BUFF_SZ, DMA_FROM_DEVICE);
	}

	for (i=0; i < NUM_TX_DESC; i++) {
		memset((void *)&tx_ring0_noncache[i],0, 16 /* sizeof()*/);
		tx_ring0_noncache[i].txd_info2.LS0_bit = 1;
		tx_ring0_noncache[i].txd_info2.DDONE_bit = 1;
		// we would map dma buffer dynamically in IRQ handler & ep_queue();
	}

	rx_dma_owner_idx0 = 0;
	tx_cpu_owner_idx0 = 0;
	tx_need_free_idx0 = 0;

	/* initial UDMA register */
	//OUTL(cpu_to_le32((u32) UDMA_Init_Setting), RTUSB_UDMA_CTRL);

	if(sm){
		printk("Storage mode enabled.\n");
		reg_write(RTUSB_UDMA_CTRL, 0x3F000063);	/* enable storage mode */
	}else
	reg_write(RTUSB_UDMA_CTRL, 0x3F000003);


	/* Tell the adapter where the TX/RX rings are located. */
	//OUTL(phys_to_bus((u32) &rx_ring[0]), RTUSB_RX_BASE_PTR0);
	reg_write(RTUSB_RX_BASE_PTR0, rx_ring_bus_addr);

	//OUTL(phys_to_bus((u32) &tx_ring0[0]), RTUSB_TX_BASE_PTR0);
	reg_write(RTUSB_TX_BASE_PTR0, tx_ring_bus_addr);

	//OUTL(cpu_to_le32((u32) NUM_RX_DESC), RTUSB_RX_MAX_CNT0);
	//OUTL(cpu_to_le32((u32) NUM_TX_DESC), RTUSB_TX_MAX_CNT0);
	reg_write(RTUSB_RX_MAX_CNT0, cpu_to_le32(NUM_RX_DESC));
	reg_write(RTUSB_TX_MAX_CNT0, cpu_to_le32(NUM_TX_DESC));

	//OUTL(cpu_to_le32((u32) tx_cpu_owner_idx0), RTUSB_TX_CTX_IDX0);
	//OUTL(cpu_to_le32((u32) (NUM_RX_DESC - 1)), RTUSB_RX_CALC_IDX0);
	reg_write(RTUSB_TX_CTX_IDX0, cpu_to_le32(tx_cpu_owner_idx0));
	reg_write(RTUSB_RX_CALC_IDX0, cpu_to_le32(NUM_RX_DESC - 1));

	udelay(500);
	return 0;
}

static int udc_dma_fini(void)
{
	int i;
	u32 len;
	dma_addr_t addr;

	udc_dma_enable(false);
	udc_dma_int_enable(false);

	/* restore UDMA register */
	reg_write(RTUSB_UDMA_CTRL, 0x0);

	// unmap & free RX buffer
	for(i=0; i<NUM_RX_DESC; i++){
		addr = rx_ring0_noncache[i].rxd_info1.PDP0;
		if(addr)
			dma_unmap_single(NULL, addr, sizeof(u8) * RX_BUFF_SZ, DMA_FROM_DEVICE);
		kfree((void *)USBRxPackets[i]);
	}

	// unmap Tx buffer only(but not free it)
	for(i=0; i<NUM_TX_DESC; i++){
		addr = tx_ring0_noncache[i].txd_info1.SDP0;
		if(addr){
			len  = tx_ring0_noncache[i].txd_info2.SDL0;
			dma_unmap_single(NULL, addr, sizeof(u8) * len, DMA_TO_DEVICE);
		}
	}

	dma_free_coherent(NULL, sizeof(struct PDMA_txdesc) * NUM_TX_DESC, tx_ring0_cache, tx_ring_bus_addr);
	dma_free_coherent(NULL, sizeof(struct PDMA_rxdesc) * NUM_RX_DESC, rx_ring0_cache, rx_ring_bus_addr);

	return 0;
}

static int rt_ep_enable(struct usb_ep *usb_ep, const struct usb_endpoint_descriptor *desc)
{
	struct rt_ep_struct *rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);
	struct rt_udc_struct *rt_usb = rt_ep->rt_usb;
	unsigned long flags;

	DBG;

	if (!usb_ep || !desc || !EP_NO(rt_ep) || desc->bDescriptorType != USB_DT_ENDPOINT || rt_ep->bEndpointAddress != desc->bEndpointAddress) {
		D_ERR(rt_usb->dev, "<%s> bad ep or descriptor\n", __func__);
		return -EINVAL;
	}
	if (rt_ep->bmAttributes != desc->bmAttributes) {
		D_ERR(rt_usb->dev, "<%s> %s type mismatch, 0x%x, 0x%x\n", __func__, usb_ep->name, rt_ep->bmAttributes, desc->bmAttributes);
		return -EINVAL;
	}
	if (!rt_usb->driver || rt_usb->gadget.speed == USB_SPEED_UNKNOWN) {
		D_ERR(rt_usb->dev, "<%s> bogus device state\n", __func__);
		return -ESHUTDOWN;
	}
	local_irq_save(flags);
	rt_ep->stopped = 0;
	if(dma){
		//rt_ep_irq_enable(rt_ep);
	}else
		rt_ep_irq_enable(rt_ep);
	local_irq_restore(flags);

	xprintk("<%s> ENABLED %s\n", __func__, usb_ep->name);
	return 0;
}

static int rt_ep_disable(struct usb_ep *usb_ep)
{
	struct rt_ep_struct *rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);
	unsigned long flags;

DBG;
	if (!usb_ep || !EP_NO(rt_ep) /* || !list_empty(&rt_ep->queue) */) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> %s can not be disabled\n", __func__, usb_ep ? rt_ep->ep.name : NULL);
		return -EINVAL;
	}

	local_irq_save(flags);
	rt_ep->stopped = 1;
	nuke(rt_ep, -ESHUTDOWN);
	rt_flush(rt_ep);
	rt_ep_irq_disable(rt_ep);

	local_irq_restore(flags);

	xprintk("<%s> DISABLED %s\n", __func__, usb_ep->name);
	return 0;
}

static struct usb_request *rt_ep_alloc_request (struct usb_ep *usb_ep, gfp_t gfp_flags)
{
	struct rt_request *req;

	DBG;
	req = kzalloc(sizeof *req, gfp_flags);
	if (!req || !usb_ep)
		return 0;

	INIT_LIST_HEAD(&req->queue);
	req->in_use = 0;
	return &req->req;
}

static void rt_ep_free_request(struct usb_ep *usb_ep, struct usb_request *usb_req)
{
	struct rt_request *req;

	DBG;
	req = container_of(usb_req, struct rt_request, req);
	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

/*
 * Two cases :
 * 1) UDC TX (IN EPs)
 * enqueue req -> handle_ep() -> write fifo -> TX_DONE -> handle_ep() -> write next fifo -> TX_DONE...
 *
 * 2) UDC RX (OUT EPs)
 * enqueue req -> RX_DONE -> handle_ep() -> read_fifo -> RX_DONE -> handle_ep() -> read fifo...
 */
static int rt_ep_queue(struct usb_ep *usb_ep, struct usb_request *req, gfp_t gfp_flags)
{
	struct rt_ep_struct		*rt_ep;
	struct rt_udc_struct	*rt_usb;
	struct rt_request		*rt_req;
	unsigned long		flags;
	int			ret = 0;
	int			handle_right_now = 0;

	rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);
	rt_usb = rt_ep->rt_usb;
	rt_req = container_of(req, struct rt_request, req);
	rt_req->rt_ep = rt_ep;

	if (rt_usb->set_config && !EP_NO(rt_ep)) {
		rt_usb->set_config = 0;
		D_ERR(rt_usb->dev, "<%s> gadget reply set config\n", __func__);
		return 0;
	}

	if (unlikely(!req || !rt_req || !req->complete || !req->buf)) {
		D_ERR(rt_usb->dev, "<%s> bad params\n", __func__);
		return -EINVAL;
	}

	if (unlikely(!usb_ep || !rt_ep)) {
		D_ERR(rt_usb->dev, "<%s> bad ep\n", __func__);
		return -EINVAL;
	}

	if (!rt_usb->driver || rt_usb->gadget.speed == USB_SPEED_UNKNOWN) {
		D_ERR(rt_usb->dev, "<%s> bogus device state\n", __func__);
		return -ESHUTDOWN;
	}

	/* debug */
	xprintk("<eq> ep%d%s %p %dB\n",  EP_NO(rt_ep), ((!EP_NO(rt_ep) && rt_ep->rt_usb->ep0state == EP0_IN_DATA_PHASE) || (EP_NO(rt_ep) && EP_DIR(rt_ep) == EP_IN  )) ? "IN" : "OUT", &rt_req->req,  req->length);

	if (rt_ep->stopped) {
		printk("EP%d -> stopped.\n",  EP_NO(rt_ep));
		req->status = -ESHUTDOWN;
		return -ESHUTDOWN;
	}

	if (rt_req->in_use) {
		D_ERR(rt_usb->dev, "<%s> refusing to queue req %p (already queued)\n", __func__, req);
		return -1;
	}

	local_irq_save(flags);
	/*
	 *	handle No-data Ctrl transfer.
	 */
	if(!EP_NO(rt_ep)/* EP0 */ && EP_DIR(rt_ep) == EP_OUT && !req->length){
		done(rt_ep, rt_req, 0);
		local_irq_restore(flags);
		return ret;
	}

	req->status = -EINPROGRESS;
	req->actual = 0;

	if(dma || list_empty(&rt_ep->queue))
		handle_right_now = 1;

	ep_add_request(rt_ep, rt_req);

	if(handle_right_now){
		if(!EP_NO(rt_ep) && rt_ep->rt_usb->ep0state != EP0_OUT_DATA_PHASE){	/* ep0 && TX*/
			handle_inep0(rt_ep);
		}else if( EP_DIR(rt_ep) == EP_IN){			/* epin[1-x] */
			handle_inep(rt_ep);
		}else{
			// other reqs are waiting for TX_DONE int.
		}
	}

	if(dma){
		if(EP_NO(rt_ep) && (EP_DIR(rt_ep) == EP_OUT))
			tasklet_schedule(&rx_dma_tasklet);
	}else{
		if( (EP_DIR(rt_ep) == EP_OUT)/* OUT EP */ && rt_ep->pending){
			rt_ep->pending = 0;
			handle_pending_epoutirq(rt_usb, rt_ep, rt_req);
		}
	}

	local_irq_restore(flags);
	return ret;
}

static int rt_ep_dequeue(struct usb_ep *usb_ep, struct usb_request *usb_req)
{
	struct rt_ep_struct *rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);
	struct rt_request *req;
	unsigned long flags;

	DBG;
	if (unlikely(!usb_ep || !EP_NO(rt_ep))) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> bad ep\n", __func__);
		return -EINVAL;
	}

	local_irq_save(flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &rt_ep->queue, queue) {
		if (&req->req == usb_req)
			break;
	}
	if (&req->req != usb_req) {
		local_irq_restore(flags);
		return -EINVAL;
	}

	done(rt_ep, req, -ECONNRESET);

	local_irq_restore(flags);
	return 0;
}

static int rt_ep_set_halt(struct usb_ep *usb_ep, int value)
{
	struct rt_ep_struct *rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);
	unsigned long flags;

	DBG;
	if (unlikely(!usb_ep || !EP_NO(rt_ep))) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> bad ep\n", __func__);
		return -EINVAL;
	}

	local_irq_save(flags);

	if ((rt_ep->bEndpointAddress & USB_DIR_IN) && !list_empty(&rt_ep->queue)) {
			local_irq_restore(flags);
			return -EAGAIN;
	}

	rt_ep_stall(rt_ep, 1);

	local_irq_restore(flags);

	D_EPX(rt_ep->rt_usb->dev, "<%s> %s halt\n", __func__, usb_ep->name);
	return 0;
}

static int rt_ep_fifo_status(struct usb_ep *usb_ep)
{
	struct rt_ep_struct *rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);

	DBG;
	if (!usb_ep) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> bad ep\n", __func__);
		return -ENODEV;
	}

	if (rt_ep->rt_usb->gadget.speed == USB_SPEED_UNKNOWN)
		return 0;
	else
		return rt_fifo_bcount(rt_ep);
}

static void rt_ep_fifo_flush(struct usb_ep *usb_ep)
{
	struct rt_ep_struct *rt_ep = container_of(usb_ep, struct rt_ep_struct, ep);
	unsigned long flags;

	DBG;
	local_irq_save(flags);

	if (!usb_ep || !EP_NO(rt_ep) || !list_empty(&rt_ep->queue)) {
		D_ERR(rt_ep->rt_usb->dev, "<%s> bad ep\n", __func__);
		local_irq_restore(flags);
		return;
	}

	/* toggle and halt bits stay unchanged */
	rt_flush(rt_ep);

	local_irq_restore(flags);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static void *rt_ep_alloc_buffer(struct usb_ep *_ep, unsigned bytes, dma_addr_t *dma, gfp_t gfp_flags)
{
	char	*retval;

	retval = kmalloc (bytes, gfp_flags & ~(__GFP_DMA|__GFP_HIGHMEM));
	if (retval)
//		*dma = virt_to_bus (retval);
		*dma = (dma_addr_t)~0;
	return retval;
}

static void rt_ep_free_buffer(struct usb_ep *_ep, void *buf, dma_addr_t dma, unsigned bytes)
{
	kfree (buf);
}
#endif

static struct usb_ep_ops rt_ep_ops = {
	.enable		= rt_ep_enable,
	.disable	= rt_ep_disable,

	.alloc_request	= rt_ep_alloc_request,
	.free_request	= rt_ep_free_request,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	.alloc_buffer	= rt_ep_alloc_buffer,
	.free_buffer	= rt_ep_free_buffer,
#endif
	.queue		= rt_ep_queue,
	.dequeue	= rt_ep_dequeue,

	.set_halt	= rt_ep_set_halt,
	.fifo_status= rt_ep_fifo_status,
	.fifo_flush	= rt_ep_fifo_flush,
};

/*******************************************************************************
 * USB endpoint control functions
 *******************************************************************************
 */
static void usb_init_data(struct rt_udc_struct *rt_usb)
{
	struct rt_ep_struct *rt_ep;
	u8 i;

	DBG;
	/* device/ep0 records init */
	INIT_LIST_HEAD(&rt_usb->gadget.ep_list);
	INIT_LIST_HEAD(&rt_usb->gadget.ep0->ep_list);
	ep0_chg_stat(__func__, rt_usb, EP0_IDLE);

	/* basic endpoint records init */
	for (i = 0; i < RT_USB_NB_EP; i++) {
		rt_ep = &rt_usb->rt_ep[i];

		if (i) {
			list_add_tail(&rt_ep->ep.ep_list, &rt_usb->gadget.ep_list);
			rt_ep->stopped = 1;
		} else
			rt_ep->stopped = 0;

		INIT_LIST_HEAD(&rt_ep->queue);
	}
}

static void udc_stop_activity(struct rt_udc_struct *rt_usb, struct usb_gadget_driver *driver)
{
	struct rt_ep_struct *rt_ep;
	int i;

	if (rt_usb->gadget.speed == USB_SPEED_UNKNOWN)
		driver = NULL;

	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < RT_USB_NB_EP; i++) {
		rt_ep = &rt_usb->rt_ep[i];
		if(i != 0){					/* don't have to flush EP[0]. */
			rt_flush(rt_ep);
			rt_ep->stopped = 1;
			rt_ep_irq_disable(rt_ep);
		}
		nuke(rt_ep, -ESHUTDOWN);
	}

	rt_usb->cfg = 0;
	rt_usb->intf = 0;
	rt_usb->alt = 0;

	if (driver)
		driver->disconnect(&rt_usb->gadget);
}

/*
 * keep for reference.
 */
static void handle_config(unsigned long data)
{
	DBG;
#if 0
	struct imx_udc_struct *imx_usb = (void *)data;
	struct usb_ctrlrequest u;
	int temp, cfg, intf, alt;

	local_irq_disable();

	temp = __raw_readl(imx_usb->base + USB_STAT);
	cfg  = (temp & STAT_CFG) >> 5;
	intf = (temp & STAT_INTF) >> 3;
	alt  =  temp & STAT_ALTSET;

	xprintk("<%s> orig config C=%d, I=%d, A=%d / req config C=%d, I=%d, A=%d\n", __func__, imx_usb->cfg, imx_usb->intf, imx_usb->alt, cfg, intf, alt);

	if (cfg == 1 || cfg == 2) {

		if (imx_usb->cfg != cfg) {
			u.bRequest = USB_REQ_SET_CONFIGURATION;
			u.bRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
			u.wValue = cfg;
			u.wIndex = 0;
			u.wLength = 0;
			imx_usb->cfg = cfg;
			imx_usb->driver->setup(&imx_usb->gadget, &u);

		}
		if (imx_usb->intf != intf || imx_usb->alt != alt) {
			u.bRequest = USB_REQ_SET_INTERFACE;
			u.bRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE;
			u.wValue = alt;
			u.wIndex = intf;
			u.wLength = 0;
			imx_usb->intf = intf;
			imx_usb->alt = alt;
			imx_usb->driver->setup(&imx_usb->gadget, &u);
		}
	}

	imx_usb->set_config = 0;

	local_irq_enable();
#endif
}

static void handle_setup(struct rt_udc_struct *rt_usb)
{
	u8 epcs;
	int i;
	union {
		struct usb_ctrlrequest  r;
		u8          raw[8];
		u32         word[2];
	} u;
	struct rt_ep_struct *rt_ep = &rt_usb->rt_ep[0];

	nuke(rt_ep, -EPROTO);

	// read setup packet
	for (i = 0; i < 8; i++)
		u.raw[i] = usb_read(SETUPDATA + i);

	le16_to_cpus(&u.r.wValue);
	le16_to_cpus(&u.r.wIndex);
	le16_to_cpus(&u.r.wLength);

	xprintk("<SETUP> %02x.%02x v%04x\n", u.r.bRequestType, u.r.bRequest, u.r.wValue);

	switch(u.r.bRequest){
		/* HW(CUSB2) has handled it. */
		case USB_REQ_SET_ADDRESS:
			if (u.r.bRequestType != (USB_TYPE_STANDARD | USB_RECIP_DEVICE))
				break;
			return;
	}

	if(!u.r.wLength){
		ep0_chg_stat(__func__, rt_usb, EP0_NO_DATA_PHASE);
	}else if (u.r.bRequestType & USB_DIR_IN){
		ep0_chg_stat(__func__, rt_usb, EP0_IN_DATA_PHASE);
	}else{
		// reload and clear out0bc
		usb_write(OUT0BC, 0);
		ep0_chg_stat(__func__, rt_usb, EP0_OUT_DATA_PHASE);
	}

	if(!rt_usb->driver){
		printk("<%s> please insert gadget driver/module.\n", __func__);
		goto stall;
	}

	if(!rt_usb->driver->setup)
		goto stall;

	i = rt_usb->driver->setup(&rt_usb->gadget, &u.r); // gadget would queue more usb req here

	if (i < 0) {
		printk("<%s> device setup error %d\n", __func__, i);
		goto stall;
	}

	if(rt_usb->ep0state == EP0_NO_DATA_PHASE){
		epcs = read_epcs(rt_ep);
		epcs |= EP_CS_EP0_HSNAK;	// clear hsnak to let HW ack the status stage.
		write_epcs(rt_ep, epcs);
	}

	return;
stall:
	printk("<%s> protocol STALL\n", __func__);
	rt_ep_stall(rt_ep, 1);
	ep0_chg_stat(__func__, rt_usb, EP0_STALL);
	return;
}

/*
 *  handle TX done interrupt
 */
static void handle_epinirq(struct rt_udc_struct *rt_usb, u8 epinirq)
{
	u8 irq = 0x0;
    struct rt_request *req;
	struct rt_ep_struct *rt_ep;

	rt_ep = &rt_usb->rt_ep[epinirq];

	if (list_empty(&rt_ep->queue))
		FATAL_ERROR("empty queue");

	// clear ep interrupt
	if(epinirq < 8){
		irq |= 1 << epinirq; 
		usb_write(IN07IRQ, irq);
	}else{
		irq |= 1 << (epinirq-8);
		usb_write(IN815IRQ, irq);
	}

	req = list_entry(rt_ep->queue.next, struct rt_request, queue);
	xprintk("r.l=%d, r.a=%d\n", req->req.length, req->req.actual);
	if(req->req.actual >= req->req.length ){
		if( req->req.actual && (!(req->req.actual % rt_ep->ep.maxpacket))  && req->req.zero){
			// deal with one more "zlp"
			req->req.zero = 0;
			write_ep_fifo_zlp(rt_ep);
                        return;
		}

		// the first tx req in ep->queue is done.
		rt_ep->tx_done_count = 0;
		if(!epinirq /* EP0 */)
			ep0_chg_stat(__func__, rt_usb, EP0_IDLE);
		done(rt_ep, req, 0);
#if 1
		// more reqs there.
		if (!list_empty(&rt_ep->queue) && !rt_ep->tx_done_count){
			if(!epinirq /* EP0 */){
				handle_inep0(rt_ep);
			}else{
				handle_inep(rt_ep);
			}
		}
#endif
	}else{
		if(!epinirq /* EP0 */){
			handle_inep0(rt_ep);
		}else
			handle_inep(rt_ep);
	}
}

static void handle_ep0outirq(struct rt_udc_struct *rt_usb, u8 epoutirq)
{
	u8 epcs, irq = 0x0;
    struct rt_request *req = NULL;
	struct rt_ep_struct *rt_ep = NULL;

DBG;
	rt_ep = &rt_usb->rt_ep[0];

	if(rt_usb->ep0state == EP0_STALL){
		printk("<%s> protocol STALL\n", __func__);
		rt_ep_stall(rt_ep, 1);
		return;
	}

	if(rt_usb->ep0state != EP0_OUT_DATA_PHASE)
		FATAL_ERROR("Odd stage");

	do{
		if(unlikely(!read_outbc(0x0)))
			FATAL_ERROR("EP0 BC");

		if (unlikely(list_empty(&rt_ep->queue)))
			FATAL_ERROR("EP0 no req");

		req = handle_outep0(rt_ep);

		//req = list_entry(rt_ep->queue.next, struct rt_request, queue);
		xprintk("q.l=%d,q.a=%d\n", req->req.length, req->req.actual);

		// clear ep interrupt
		irq |= 1; 
		usb_write(OUT07IRQ, irq);

		if(req && ((req->req.actual % rt_ep->ep.maxpacket) || (req->req.actual >= req->req.length))){
			ep0_chg_stat(__func__, rt_usb, EP0_IDLE);
			done(rt_ep, req, 0);	// short packet indicates transaction is done.

			epcs = read_epcs(rt_ep);
			epcs |= EP_CS_EP0_HSNAK;	// clear hsnak bit to let HW ack the status stage.
			write_epcs(rt_ep, epcs);
			break;
		}

		// reload EP[0]
		usb_write(OUT0BC /*out0bc*/, 0x0);
		epcs = read_epcs(rt_ep);
	}while(!(epcs & EP0_OUT_BSY));
}

static void handle_pending_epoutirq(struct rt_udc_struct *rt_usb, struct rt_ep_struct *rt_ep, struct rt_request *req)
{
	u8 epcs;

DBG;
	do{
		if(unlikely(!read_outbc(EP_NO(rt_ep))))
			FATAL_ERROR("No BC");

		handle_outep(rt_ep);
		if(req && ( (req->req.actual % rt_ep->ep.maxpacket) || (req->req.actual >= req->req.length))){
			xprintk("q.l=%d,q.a=%d\n", req->req.length, req->req.actual);

			//rx_done(rt_ep, req, 0);
			done(rt_ep, req, 0);
		}

		epcs = read_epcs(rt_ep);
		write_epcs(rt_ep, 0x0);
		epcs = read_epcs(rt_ep);

	}while(!(epcs & EP_CS_BSY));
}

static void handle_epoutirq(struct rt_udc_struct *rt_usb, u8 epoutirq)
{
	u8 irq = 0x0;

DBG;
	if(unlikely(epoutirq == 0x0)){
		handle_ep0outirq(rt_usb, 0x0);
		return;
	}

	tasklet_schedule(&rx_tasklet);

	// clear ep interrupt
	irq |= 1 << epoutirq; 
	usb_write(OUT07IRQ, irq);
	return;
}

static void eps_change_to_hs(struct rt_udc_struct *rt_usb)
{
	int i;
	struct rt_ep_struct *rt_ep;
	for(i = 0; i < RT_USB_NB_EP; i++){
		rt_ep = &rt_usb->rt_ep[i];
		if(rt_ep->bmAttributes == USB_ENDPOINT_XFER_BULK){
			rt_ep->ep.maxpacket = 512;
		}
	}
}

static void eps_change_to_fs(struct rt_udc_struct *rt_usb)
{
	int i;
	struct rt_ep_struct *rt_ep;
	for(i = 0; i < RT_USB_NB_EP; i++){
		rt_ep = &rt_usb->rt_ep[i];
		if(rt_ep->bmAttributes == USB_ENDPOINT_XFER_BULK){
			rt_ep->ep.maxpacket = 64;
		}
	}
}

void handle_highspeed(struct rt_udc_struct *rt_usb)
{
	DBG;

	eps_change_to_hs(rt_usb);

	if(dma){
#if defined (CONFIG_RALINK_RT3883) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
		usb_write(IN1CON, 0x8D);	// InEP1 : Int, 2 subfifos
		usb_write(IN2CON, 0x89);	// InEP2 : Bulk, 2 subfifo
		usb_write(OUT1CON, 0x8D);	// OutEP1 : Int, 2 subfifos
		usb_write(OUT2CON, 0x89);	// OutEP2 : Bulk, 2 subfifos
		//usb_write(OUT3CON, 0x89);	// OutEP3 : Bulk, 2 subfifo
		//usb_write(OUT4CON, 0x89);	// OutEP4 : Bulk. 2 subfifo
#elif defined (CONFIG_RALINK_RT5350)
		// Access by CPU
		usb_write(IN1CON, 0x89);	// InEP1 : Bulk, 2 subfifos
		usb_write(OUT1CON, 0x89);	// OutEP1 : Bulk, 2 subfifos
#else
#error	"define a platform"
#endif
	}else{
		// Access by CPU
#if defined (CONFIG_RALINK_RT3883) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
		usb_write(IN1CON, 0x8C);	// InEP1 : Int  , 1 subfifos
		usb_write(IN2CON, 0x88);	// InEP2 : Bulk, 1 subfifo

		usb_write(OUT1CON, 0x8C);	// OutEP1 : Int, 1 subfifos
		usb_write(OUT2CON, 0x88);	// OutEP2 : Bulk, 1 subfifos
		//usb_write(OUT3CON, 0x88);	// OutEP3 : Bulk, 1 subfifo
		//usb_write(OUT4CON, 0x88);	// OutEP4 : Bulk. 1 subfifo
#elif defined (CONFIG_RALINK_RT5350)
		// Access by CPU
		usb_write(IN1CON, 0x88);	// InEP1 : Bulk  , 1 subfifos
		usb_write(OUT1CON, 0x88);	// OutEP1 : Bulk, 1 subfifos
#else
#error	"define a platform"
#endif

	}
	// clear all pending interrupts
	usb_write(IN07IRQ, 0xFF);
	usb_write(OUT07IRQ, 0xFF);

	rt_usb->gadget.speed = USB_SPEED_HIGH;	

	// reset ALL endpoints
	rt_all_eps_reset();

	// Enable ep0 interrupt.
	// (EPx interrupt is enabled in EPx_enable(). )
	rt_ep_irq_enable(&rt_usb->rt_ep[0]);
}

static void handle_reset(struct rt_udc_struct *rt_usb)
{
	struct rt_ep_struct *rt_ep;
        int i;

	eps_change_to_fs(rt_usb);

	// remove all EPs' usb request
	for (i = 0; i < RT_USB_NB_EP; i++) {
		rt_ep = &rt_usb->rt_ep[i];
		if(i != 0){                                     /* don't have to flush EP[0]. */
			rt_flush(rt_ep);
			rt_ep->stopped = 1;
			rt_ep_irq_disable(rt_ep);
		}
		nuke(rt_ep, -ESHUTDOWN);
	}

        rt_usb->cfg = 0;
        rt_usb->intf = 0;
        rt_usb->alt = 0;

        if(dma){
		// clear all PDMA interrupts
		udc_dma_all_int_clear();
		// reset PDMA
		udc_dma_rst();
	}

	// clear all pending interrupts
	usb_write(IN07IRQ, 0xFF);
	usb_write(OUT07IRQ, 0xFF);

	// flush all EP's fifo
	rt_all_eps_reset();                        
}

static void handle_usbirq(struct rt_udc_struct *rt_usb, u8 usbirq)
{
	if(usbirq & USB_INTR_SETUP_TOKEN_VALID){
		// Setup token is arrival.
		// get setup data and pass it to gadget driver.
		handle_setup(rt_usb);
	}

	if(usbirq & USB_INTR_RESET)
		handle_reset(rt_usb);

	if(usbirq & USB_INTR_HSPEED)
		handle_highspeed(rt_usb);

	/*
	 * DO NOT try to clear SoF and token Interrupt!
	 */
	if( (usbirq & USB_INTR_SETUP_TOKEN_VALID) ||
		(usbirq & USB_INTR_HSPEED) ||
		(usbirq & USB_INTR_RESET))
		usb_write(USBIRQ, usbirq);
}

static int irq_count = 100;	/* for debug */
/*
 * Interrupt handler
 */
irqreturn_t rt_irq_handler(int irq, void *_dev)
{
	u32 usbirq,epin07irq,epin07ien,epout07irq,epout07ien;
	struct rt_udc_struct *rt_usb = _dev;
#ifdef DEBUG
	u32 count_tmp = irq_count;	
#endif

        
	DBG;
	irq_count++;

	usbirq = usb_read(USBIRQ);
	epin07irq = usb_read(IN07IRQ);
	epin07ien = usb_read(IN07IEN);
	epout07irq = usb_read(OUT07IRQ);
	epout07ien = usb_read(OUT07IEN);

	//epin07irq = epin07irq & epin07ien;
	//epout07irq = epout07irq & epout07ien;
	
	xprintk(">%x\n", count_tmp);
	dump_usbirq(usbirq);
	dump_epirq(epin07irq, epin07ien, 1);
	dump_epirq(epout07irq, epout07ien, 0);

	if(dma){
		u32 dma_irq = reg_read(RTUSB_INT_STATUS);
		if(epin07irq & 0x1)				// INEP0
			handle_epinirq(rt_usb, 0);

		if(usbirq)						// HS, Reset, SetupValid
			handle_usbirq(rt_usb, usbirq);

		if(epout07irq & 0x1)			// OUTEP0
			handle_epoutirq(rt_usb, 0);

		if(dma_irq)
			handle_dmairq(rt_usb, dma_irq);

	}else{
		if(epin07irq & 0x1)				// INEP0
			handle_epinirq(rt_usb, 0);

		if(usbirq)						// HS, Reset, SetupValid
			handle_usbirq(rt_usb, usbirq);

		if(epout07irq & 0x1)			// OUTEP0
			handle_epoutirq(rt_usb, 0);

		if(epout07irq & 0x2)			// OUTEP1
			handle_epoutirq(rt_usb, 1);

		if(epin07irq & 0x2)				// INEP1
			handle_epinirq(rt_usb, 1);

		if(epout07irq & 0x4)			// OUTEP2
			handle_epoutirq(rt_usb, 2);

		if(epin07irq & 0x4)				// INEP2
			handle_epinirq(rt_usb, 2);

		//if(epout07irq & 0x8)			// OUTEP3
		//	handle_epoutirq(rt_usb, 3);

		//if(epout07irq & 0x10)			// OUTEP4
		//	handle_epoutirq(rt_usb, 4);
	}
	xprintk("<%x\n", count_tmp);
	return IRQ_HANDLED;
}

/*
 ******************************************************************************
 * Static defined Ralink UDC structure
 *******************************************************************************
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static void nop_release(struct device *dev)
{
    return;
}
#endif

static const struct usb_gadget_ops rt_udc_ops = {
	.get_frame	 = rt_udc_get_frame,
	.wakeup		 = rt_udc_wakeup,
};

static struct rt_udc_struct controller = {
	.gadget = {
		.ops		= &rt_udc_ops,
		.ep0		= &controller.rt_ep[0].ep,
		.name		= driver_name,
		.dev = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
                        .init_name = "gadget",
                        .release = nop_release,
#else
			.bus_id = "gadget",
#endif
		},
	},
	.rt_ep[0] = {
		.ep = {
			.name		= ep0name,
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb		= &controller,
		.bEndpointAddress	= 0,
		.bmAttributes		= USB_ENDPOINT_XFER_CONTROL,
		.pending		= 0,
	 },
#if defined (CONFIG_RALINK_RT3883) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
	.rt_ep[1] = {
		.ep = {
			.name		= "ep1in-int",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress	= USB_DIR_IN | 1,
		.bmAttributes		= USB_ENDPOINT_XFER_INT,
		.pending        = 0,
	 },
	.rt_ep[2] = {
		.ep = {
			.name		= "ep2in-bulk",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress	= USB_DIR_IN | 2,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.pending        = 0,
	},
	.rt_ep[3] = {
		.ep = {
			.name		= "ep1out-int",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress 	= USB_DIR_OUT | 1,
		.bmAttributes		= USB_ENDPOINT_XFER_INT,
		.pending        = 0,
	 },
	.rt_ep[4] = {
		.ep = {
			.name		= "ep2out-bulk",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		 },
		.rt_usb			= &controller,
		.bEndpointAddress 	= USB_DIR_OUT | 2,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.pending        = 0,
	 },
	 /*
	.rt_ep[5] = {
		.ep = {
			.name		= "ep3out-bulk",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress 	= USB_DIR_OUT | 3,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.pending        = 0,
	 },
	.rt_ep[6] = {
		.ep = {
			.name		= "ep4out-bulk",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress 	= USB_DIR_OUT | 4,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.pending        = 0,
	 },
	 */

#elif defined (CONFIG_RALINK_RT5350)
	.rt_ep[1] = {
		.ep = {
			.name		= "ep1in-bulk",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress	= USB_DIR_IN | 1,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.pending        = 0,
	 },
	.rt_ep[2] = {
		.ep = {
			.name		= "ep1out-bulk",
			.ops		= &rt_ep_ops,
			.maxpacket	= 64,
		},
		.rt_usb			= &controller,
		.bEndpointAddress 	= USB_DIR_OUT | 1,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.pending        = 0,
	 },
#else
#error	"define a platform"
#endif
};

/*
 *******************************************************************************
 * USB gadged driver functions
 *******************************************************************************
 */

static void rt_udc_enable(struct rt_udc_struct *rt_usb)
{
	DBG;
	rt_usb->gadget.speed = USB_SPEED_FULL;
	if(dma){
		// enable dma interrupts
		udc_dma_all_int_clear();
		udc_dma_int_enable(true);

		udc_dma_rst();

		// enable dma
		udc_dma_enable(true);
	}
}

static void rt_udc_disable(struct rt_udc_struct *rt_usb)
{
	DBG;
	ep0_chg_stat(__func__, rt_usb, EP0_IDLE);
	rt_usb->gadget.speed = USB_SPEED_UNKNOWN;
	if(dma){
		// disable dma interrupts
		udc_dma_all_int_clear();
		udc_dma_int_enable(false);

		udc_dma_rst();

		// disable dma
		udc_dma_enable(false);
	}
}

int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct rt_udc_struct *rt_usb = &controller;
	int retval;

	DBG;
	if (!driver || driver->speed < USB_SPEED_FULL || !driver->bind || !driver->disconnect || !driver->setup)
			return -EINVAL;
	if (!rt_usb)
		return -ENODEV;
	if (rt_usb->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	rt_usb->driver = driver;
	rt_usb->gadget.dev.driver = &driver->driver;
	retval = device_add(&rt_usb->gadget.dev);
	if (retval)
		goto fail;

	retval = driver->bind(&rt_usb->gadget);
	if (retval) {
		D_ERR(rt_usb->dev, "<%s> bind to driver --> error %d\n", __func__, retval);
		device_del(&rt_usb->gadget.dev);
		goto fail;
	}

	D_INI(rt_usb->dev, "<%s> registered gadget driver '%s'\n", __func__, driver->driver.name);
	rt_udc_enable(rt_usb);
	return 0;

fail:
	rt_usb->driver = NULL;
	rt_usb->gadget.dev.driver = NULL;
	return retval;
}
EXPORT_SYMBOL(usb_gadget_register_driver);

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct rt_udc_struct *rt_usb = &controller;

DBG;
	if (!rt_usb)
		return -ENODEV;
	if (!driver || driver != rt_usb->driver || !driver->unbind)
		return -EINVAL;

	udc_stop_activity(rt_usb, driver);
	rt_udc_disable(rt_usb);
	del_timer(&rt_usb->timer);

	driver->unbind(&rt_usb->gadget);
	rt_usb->gadget.dev.driver = NULL;
	rt_usb->driver = NULL;

	device_del(&rt_usb->gadget.dev);

	D_INI(rt_usb->dev, "<%s> unregistered gadget driver '%s'\n", __func__, driver->driver.name);

	return 0;
}
EXPORT_SYMBOL(usb_gadget_unregister_driver);

/*******************************************************************************
 * Module functions
 *******************************************************************************
 */
static int __init rt_udc_probe(struct platform_device *pdev)
{
	struct rt_udc_struct *rt_usb = &controller;
	struct resource *res_mem, *res_irq;
	void __iomem *base;
	int ret = 0, res_mem_size;

DBG;
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&pdev->dev, "can't get device resources\n");
		return -ENODEV;
	}

	res_mem_size = res_mem->end - res_mem->start + 1;
	if (!request_mem_region(res_mem->start, res_mem_size, res_mem->name)) {
		dev_err(&pdev->dev, "can't allocate %d bytes at %d address\n", res_mem_size, res_mem->start);
		return -ENOMEM;
	}

	base = ioremap(res_mem->start, res_mem_size);
	if (!base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -EIO;
		goto fail1;
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq) {
			dev_err(&pdev->dev, "can't get irq number\n");
			ret = -ENODEV;
			goto fail3;
	}
	rt_usb->interrupt = res_irq->start;

	ret = request_irq(rt_usb->interrupt, rt_irq_handler, IRQF_DISABLED, driver_name, rt_usb);
	if (ret) {
		dev_err(&pdev->dev, "can't get irq %i, err %d\n", rt_usb->interrupt, ret);
		goto fail3;
	}

	rt_usb->res = res_mem;
	rt_usb->base = base;
	rt_usb->dev = &pdev->dev;

	device_initialize(&rt_usb->gadget.dev);

	rt_usb->gadget.dev.parent = &pdev->dev;
	rt_usb->gadget.dev.dma_mask = pdev->dev.dma_mask;

	platform_set_drvdata(pdev, rt_usb);

	usb_init_data(rt_usb);

	if(dma){
		if(rt_udc_dma_init())
			goto fail4;
	}

	rt_udc_init(rt_usb);

	init_timer(&rt_usb->timer);
	rt_usb->timer.function = handle_config;
	rt_usb->timer.data = (unsigned long)rt_usb;

	return 0;
fail4:
	free_irq(rt_usb->interrupt, rt_usb);
fail3:
	iounmap(base);
fail1:
	release_mem_region(res_mem->start, res_mem_size);
	return ret;
}

static int __exit rt_udc_remove(struct platform_device *pdev)
{
	struct rt_udc_struct *rt_usb = platform_get_drvdata(pdev);

	DBG;
	rt_udc_disable(rt_usb);
	del_timer(&rt_usb->timer);

	free_irq(rt_usb->interrupt, rt_usb);

	iounmap(rt_usb->base);
	release_mem_region(rt_usb->res->start,	rt_usb->res->end - rt_usb->res->start + 1);

	//if (pdata->exit)
	//	pdata->exit(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void set_device_mode(void)
{
	u32 val;
	val = le32_to_cpu(*(volatile u_long *)(SYSCFG1));
	val = val & ~(USB0_HOST_MODE);
	*(volatile u_long *)(SYSCFG1) = cpu_to_le32(val);
	udelay(10000);
}

/*----------------------------------------------------------------------------*/
static struct platform_driver udc_driver = {

	.driver		= {
		.name	= driver_name,
		.owner	= THIS_MODULE,
	},
	.probe		= rt_udc_probe,
	.remove		= __exit_p(rt_udc_remove),
	.suspend	= NULL,
	.resume		= NULL,
};

static int udc_create_proc(void)
{
    pProcDir = proc_mkdir(PROC_DIR, NULL);
    if ((pProcDebugLevel = create_proc_entry(DEBUGLEVEL_PROCFILE, 0, pProcDir))){
        pProcDebugLevel->read_proc = (read_proc_t*)&debuglevel_read;
        pProcDebugLevel->write_proc = (write_proc_t*)&debuglevel_write;
    }
    return 0;
}

static int udc_remove_proc(void)
{
    if (pProcDebugLevel)
        remove_proc_entry(DEBUGLEVEL_PROCFILE, pProcDir);
    if (pProcDir)
        remove_proc_entry(PROC_DIR, 0);

    return 0;
}

static int __init udc_init(void)
{
	int ret;
	udc_create_proc();

	try_wake_up();
	set_device_mode();

	ret = platform_driver_register(&udc_driver);

	tasklet_init(&rx_tasklet, rx_do_tasklet, 0);
	tasklet_init(&tx_tasklet, tx_do_tasklet, 0);

	if(dma){
		printk("DMA TXMAXCAP=%d\n", TXMAXCAP);
		tasklet_init(&rx_dma_tasklet, rx_dma_done_do_tasklet, 0);
		tasklet_init(&tx_dma_tasklet, tx_dma_done_do_tasklet, 0);
	}

	return ret; //platform_driver_probe(&udc_driver, rt_udc_probe);
}
module_init(udc_init);

static void __exit udc_exit(void)
{
	DBG;
	udc_remove_proc();
	if(dma)
		udc_dma_fini();
	platform_driver_unregister(&udc_driver);
}
module_exit(udc_exit);

MODULE_DESCRIPTION("Ralink USB Device Controller driver");
MODULE_AUTHOR("Ying Yuan Huang <yy_huang@ralinktech.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rt_udc");

