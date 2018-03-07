#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>

#include <sockets.h>
#include <utils.h>
#include <log_util.h>

// <net/if.h>
//struct ifreq {
//	char ifr_name[IFNAMSIZ]; /* interface name, e.g., "le0" */
//	union {
//		struct sockaddr ifru_addr;
//		struct sockaddr ifru_dstaddr;
//		struct sockaddr ifru_broadaddr;
//		short ifru_flags;
//		int ifru_metric;
//		caddr_t ifru_data;
//	} ifr_ifru;
//};

/**
 * Check a given net device whether has got IP
 * If a name is pecified for an IPv4 socket, we call ioctl
 * with a request of SIOCGIFADDR to obtain the unicast IP address for the interface
 *
 * For IPv4 only
 */
char *get_netdev_ip(const char *ifname)
{
	char * strIP = NULL;
	SOCKET sockfd;
	struct ifreq ifr;

	if (!ifname) return NULL;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) return NULL;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	/* return the unicast address in the ifr_addr member */
	if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
		strIP = inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr);
	}

	close(sockfd);
	return strIP ? strdup(strIP) : NULL;
}

/**
 * @ipstr_out Max 4 IPs support
 * Return how many ips are in @ipstr_out array
 *
 * nslookup domain-name
 */
int getip_byhostname(const char *hostname, char ipstr_out[4][16])
{
	struct hostent *ht = NULL;
	int i;

	ht = gethostbyname(hostname);
	assert_return(ht);
	if (ht->h_addr_list && *ht->h_addr_list) {
		for (i = 0; i < 4; ht->h_addr_list ++, i ++) {
			char *ipstr = *ht->h_addr_list;
			if (!ipstr) break;
			snprintf(ipstr_out[i], 16, "%d.%d.%d.%d",
				(uint8_t)ipstr[0], (uint8_t)ipstr[1], (uint8_t)ipstr[2], (uint8_t)ipstr[3]);
		}
	}

	return i;
}

