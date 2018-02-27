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

#if 0
/* Address to send to all hosts. */
#define	INADDR_BROADCAST	((unsigned long int) 0xffffffff)

struct ifreq {
#define IFHWADDRLEN	6
	union
	{
		char	ifrn_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	} ifr_ifrn;
	
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		struct	sockaddr ifru_netmask;
		struct  sockaddr ifru_hwaddr;
		short	ifru_flags;
		int	ifru_ivalue;
		int	ifru_mtu;
		struct  ifmap ifru_map;
		char	ifru_slave[IFNAMSIZ];	/* Just fits the size */
		char	ifru_newname[IFNAMSIZ];
		void __user *	ifru_data;
		struct	if_settings ifru_settings;
	} ifr_ifru;
};
#endif

static SOCKET ifreq_socket = -1;

static void ifreq_panic(int flag)
{
	if (flag == -1) {
		sys_debug(0, "[ERROR] %s", strerror(errno));
		assert_param(0);
	}
}

static void ifreq_init()
{
	ifreq_socket = socket(AF_INET, SOCK_DGRAM, 0);
	assert_return(ifreq_socket > 0);
}

static void ifreq_broadcast_send()
{
	int ret = -1;
	struct sockaddr_in addrto;
	int addr_len = sizeof(struct sockaddr_in);
	const char *msg = "broadcast";

    bzero(&addrto, sizeof(struct sockaddr_in));
    addrto.sin_family = AF_INET;
    addrto.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addrto.sin_port = htons(6000);

	//for (; ;) {
	//	sleep(1);
		ret = sendto(ifreq_socket, msg, strlen(msg), 0, (struct sockaddr *) &addrto, addr_len);
		if (ret <= 0) {
            sys_debug(0, "[ERROR] %s", strerror(errno));
        } else {
            printf("send ok\n");     
        }
	//}
}

static void ifreq_broadcast_recv()
{
	int ret = -1;

	// 1. bind address
    struct sockaddr_in addrto;
    bzero(&addrto, sizeof(struct sockaddr_in));
    addrto.sin_family = AF_INET;
    addrto.sin_addr.s_addr = htonl(INADDR_ANY);
    addrto.sin_port = htons(6000);

	// 2. broadcast sending
    struct sockaddr_in from;
    bzero(&from, sizeof(struct sockaddr_in));
    from.sin_family = AF_INET;
    from.sin_addr.s_addr = htonl(INADDR_ANY);
    from.sin_port = htons(6000);

	// 3. enable broadcast
	const int broadcast_state = 1;
	ret = setsockopt(ifreq_socket, SOL_SOCKET, SO_BROADCAST,
		(char *)&broadcast_state, sizeof(broadcast_state));
	ifreq_panic(ret);

	ret = bind(ifreq_socket, (struct sockaddr *) &(addrto),
		sizeof(struct sockaddr_in));
	ifreq_panic(ret);

	char smsg[BUFSIZE] = { 0 };
	int addr_len = sizeof(struct sockaddr_in);  
	for (; ;) {
		ifreq_broadcast_send();
		usleep(10000);
        ret = recvfrom(ifreq_socket, smsg, BUFSIZE, 0, (struct sockaddr*) &from, (socklen_t*) &addr_len);
        if (ret <= 0) {
            sys_debug(0, "[ERROR] %s", strerror(errno));
        } else {
            printf("recv %s\n", smsg);     
        }
  		memset(smsg, 0, BUFSIZE);
        sleep(1);
		
	}
}

static void ifreq_info()
{
	struct ifreq ifr;
	int ret;
	bzero(&ifr, sizeof(struct ifreq));
	// 1. get the second card ifname
	ifr.ifr_ifindex = 2;
	ret = ioctl(ifreq_socket, SIOCGIFNAME, &ifr);
	ifreq_panic(ret);
	sys_debug(0, "[DEBUG] interface name: %s", ifr.ifr_name);

	// 2. get ip address
	ret = ioctl(ifreq_socket, SIOCGIFADDR, &ifr);
	ifreq_panic(ret);
	char ipstr[16] = { 0 };
	/**
	 * const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
	 * inet_ntop()  converts  the  network  address structure @src in the @af
       address family into a character string.  The resulting string is copied
       to the buffer pointed to by @dst, which must be a non-null pointer.  The
       caller specifies the number of bytes available in this  buffer  in  the
       argument @size.
     * On success, inet_ntop() returns a non-null pointer  to  dst.   NULL  is
       returned if there was an error, with errno set to indicate the error.
     */
	inet_ntop(AF_INET, &((struct sockaddr_in*) &(ifr.ifr_ifru.ifru_addr))->sin_addr,
		ipstr, sizeof(ipstr));
	sys_debug(0, "[DEBUG] IP addr: %s\n", ipstr);

	// 3. get flags
	ret = ioctl(ifreq_socket, SIOCGIFFLAGS, &ifr);
	ifreq_panic(ret);
	if (ifr.ifr_flags & IFF_UP) {
		sys_debug(0, "[DEBUG] flag IFF_UP set, means card has up.");
	}
#if 0

	if (ifr.ifr_flags & IFF_PROMISC) {
		sys_debug(0, "[DEBUG] flag IFF_PROMISC set\n");
	} else {
		ifr.ifr_flags |= IFF_PROMISC;
		ret = ioctl(ifreq_socket, SIOCSIFFLAGS, &ifr);
		ifreq_panic(ret);
	}

	else if (ifr.ifr_flags & IFF_BROADCAST) {
		sys_debug(0, "[DEBUG] flag IFF_BROADCAST set\n");
	} else if (ifr.ifr_flags & IFF_MULTICAST) {
		sys_debug(0, "[DEBUG] flag IFF_MULTICAST set\n");
	}
#endif
}

void ifreq_test_entry()
{
	ifreq_init();
	//ifreq_info();
	ifreq_broadcast_recv();
}

