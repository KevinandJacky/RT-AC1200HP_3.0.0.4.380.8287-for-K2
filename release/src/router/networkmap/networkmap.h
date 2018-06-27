/*
#include <sys/socket.h>
#include <stdio.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
*/
#include <syslog.h>
#include "../shared/version.h"
#include "../shared/shared.h"

#define FALSE   0
#define TRUE    1
#define INTERFACE 	"br0"
#define MODEL_NAME 	RT_BUILD_NAME
#define ARP_BUFFER_SIZE	512

// Hardware type field in ARP message
#define DIX_ETHERNET            1
// Type number field in Ethernet frame
#define IP_PACKET               0x0800
#define ARP_PACKET              0x0806
#define RARP_PACKET             0x8035
// Message type field in ARP messages
#define ARP_REQUEST             1
#define ARP_RESPONSE            2
#define RARP_REQUEST            3
#define RARP_RESPONSE           4
#define RCV_TIMEOUT             2 //sec
#define MAXDATASIZE             512
#define LPR                     0x02
#define LPR_RESPONSE            0x00
#define LINE_SIZE               200
                                                                                                                                            
//for Notification Center trigger flag
#ifdef RTCONFIG_NOTIFICATION_CENTER
enum
{
	FLAG_SAMBA_INLAN = 0,
	FLAG_OSX_INLAN,
	FLAG_XBOX_PS,
	FLAG_UPNP_RENDERER
};
#endif

#define USERAGENT "Asuswrt/networkmap"

#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_JFFSV1) || defined(RTCONFIG_BRCM_NAND_JFFS2))
#define NMP_CLIENT_LIST_FILENAME        "/jffs/nmp_client_list"
#else
#define NMP_CLIENT_LIST_FILENAME        "/tmp/nmp_client_list"
#endif
#define NCL_LIMIT 14336   //nmp_client_list limit to 14KB to avoid UI glitch

#ifdef DEBUG
	#define NMP_DEBUG(fmt, args...) _dprintf(fmt, ## args)
	//#define NMP_DEBUG(fmt, args...) syslog(LOG_NOTICE, fmt, ## args)
#else
	#define NMP_DEBUG(fmt, args...)
#endif

#ifdef DEBUG_MORE
        #define NMP_DEBUG_M(fmt, args...) _dprintf(fmt, ## args)
	//#define NMP_DEBUG_M(fmt, args...) syslog(LOG_NOTICE, fmt, ## args)
#else
        #define NMP_DEBUG_M(fmt, args...)
#endif

#ifdef DEBUG_FUNCTION
        #define NMP_DEBUG_F(fmt, args...) _dprintf(fmt, ## args)
        //#define NMP_DEBUG_FUN(fmt, args...) syslog(LOG_NOTICE, fmt, ## args)
#else
        #define NMP_DEBUG_F(fmt, args...)
#endif

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;

//Device service info data structure
typedef struct {
        unsigned char   ip_addr[255][4];
        unsigned char   mac_addr[255][6];
	unsigned char   user_define[255][16];
	unsigned char   device_name[255][32];
	unsigned char	apple_model[255][16];
	char		pad[2];
        int             type[255];
        int             http[255];
        int             printer[255];
        int             itune[255];
	int		exist[255];
#ifdef RTCONFIG_BWDPI
	char		bwdpi_host[255][32];
	char		bwdpi_vendor[255][100];
	char		bwdpi_type[255][100];
	char		bwdpi_device[255][100];
#endif
        int             ip_mac_num;
	int 		detail_info_num;
	char		delete_mac[13];
	char		pad1[3];
} CLIENT_DETAIL_INFO_TABLE, *P_CLIENT_DETAIL_INFO_TABLE;

// walf test
typedef struct
{
	unsigned short  hardware_type; 
   	unsigned short  protocol_type;           
   	unsigned char hwaddr_len;
   	unsigned char ipaddr_len;               
   	unsigned short  message_type;
   	unsigned char source_hwaddr[6];              
   	unsigned char source_ipaddr[4];
   	unsigned char dest_hwaddr[6];    
   	unsigned char dest_ipaddr[4];
} ARP_HEADER;

int FindAllApp( unsigned char *src_ip, P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab, int i);
