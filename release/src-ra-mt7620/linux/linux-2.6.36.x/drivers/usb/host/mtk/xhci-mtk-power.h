#ifndef _XHCI_MTK_POWER_H
#define _XHCI_MTK_POWER_H

#include <linux/usb.h>
#include "xhci.h"
#include "xhci-mtk.h"

static int g_num_u3_port;
static int g_num_u2_port;

void enableXhciAllPortPower(struct xhci_hcd *xhci);
void enableAllClockPower();
void disablePortClockPower();
void enablePortClockPower(int port_index, int port_rev);

#endif
