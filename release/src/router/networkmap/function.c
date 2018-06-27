#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <stdio.h>
//#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <bcmnvram.h>
#include "networkmap.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <asm/byteorder.h>
#include "iboxcom.h"
#include "../shared/shutils.h"
#ifdef RTCONFIG_NOTIFICATION_CENTER
#include <libnt.h>
#endif

void toLowerCase(char *str) {
    char *p;

    for(p=str;*p!='\0';p++)
        if('A'<=*p&&*p<='Z')*p+=32;

}

int FindHostname(P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab)
{
	unsigned char *dest_ip = p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->detail_info_num];
	char ipaddr[16];
	sprintf(ipaddr, "%d.%d.%d.%d",(int)*(dest_ip),(int)*(dest_ip+1),(int)*(dest_ip+2),(int)*(dest_ip+3));

	char *nv, *nvp, *b;
	char *mac, *ip, *name, *expire;
	FILE *fp;
	char line[256];
	char *next;

// Get current hostname from DHCP leases
	if (!nvram_get_int("dhcp_enable_x") || !nvram_match("sw_mode", "1"))
		return 0;

	if ((fp = fopen("/var/lib/misc/dnsmasq.leases", "r"))) {
		fcntl(fileno(fp), F_SETFL, fcntl(fileno(fp), F_GETFL) | O_NONBLOCK);
		while ((next = fgets(line, sizeof(line), fp)) != NULL) {
			if (vstrsep(next, " ", &expire, &mac, &ip, &name) == 4) {
				if ((!strcmp(ipaddr, ip)) &&
				    (strlen(name) > 0) &&
				    (!strchr(name, '*')) &&	// Ensure it's not a clientid in
				    (!strchr(name, ':')))	// case device didn't have a hostname
						strlcpy(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num], name, 32);
			}
		}
		fclose(fp);
	}

// Get names from static lease list, overruling anything else
	nv = nvp = strdup(nvram_safe_get("dhcp_staticlist"));

	 if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &mac, &ip, &name) == 3) && (strlen(ip) > 0) && (strlen(name) > 0)) {
				if (!strcmp(ipaddr, ip))
					strlcpy(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num], name, 32);
			}
		}
		free(nv);
	}

	return 1;
}
