/*
 *		Swansea University Computer Society NET3
 *
 *	This work is derived from NET2Debugged, which is in turn derived
 *	from NET2D which was written by:
 * 		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This work was derived from Ross Biro's inspirational work
 *		for the LINUX operating system.  His version numbers were:
 *
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 * 		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 * 		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 * 		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 * 		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *		$Id: //WIFI_SOC/MP/SDK_4_2_0_0/RT288x_SDK/source/linux-2.6.36.x/include/linux/inet.h#1 $
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_INET_H
#define _LINUX_INET_H

#include <linux/types.h>

/*
 * These mimic similar macros defined in user-space for inet_ntop(3).
 * See /usr/include/netinet/in.h .
 */
#define INET_ADDRSTRLEN		(16)
#define INET6_ADDRSTRLEN	(48)

extern __be32 in_aton(const char *str);
extern int in4_pton(const char *src, int srclen, u8 *dst, int delim, const char **end);
extern int in6_pton(const char *src, int srclen, u8 *dst, int delim, const char **end);
#endif	/* _LINUX_INET_H */
