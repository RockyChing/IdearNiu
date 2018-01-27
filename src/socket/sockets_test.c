#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include <sockets.h>
#include <log_util.h>

#define TEST_DNS_NAME "live.smart.jd.com"

#if 0
struct hostent {
    char  *h_name;            /* official name of host */
    char **h_aliases;         /* alias list */
    int    h_addrtype;        /* host address type */
    int    h_length;          /* length of address */
    char **h_addr_list;       /* list of addresses */
		An array of pointers to network addresses for the host (in network byte order), terminated by a NULL pointer
}
#endif

static void print_hostent(struct hostent *hostent)
{
	struct hostent *ht = hostent;
	if (ht->h_addrtype != AF_INET) {
		sys_debug(0, "Only support IPV4");
		exit(0);
	}

	printf("official name\t:");
	if (ht->h_name) {
		printf("%s\n", ht->h_name);
	}

	printf("alias name\t:");
	if (ht->h_aliases) {
		for (; *ht->h_aliases; ht->h_aliases ++)
			printf("%s\n", *ht->h_aliases);
	}

	printf("IP\t\t:");
	if (ht->h_addr_list) {
		for (; *ht->h_addr_list; ht->h_addr_list ++) {
			char *ipstr = *ht->h_addr_list;
			printf("%d.%d.%d.%d\n",
				(uint8_t)ipstr[0], (uint8_t)ipstr[1], (uint8_t)ipstr[2], (uint8_t)ipstr[3]);
		}
	}
}

static void test_getip_byhostname()
{
	struct hostent *hostent;
	sys_debug(0, "***test_getip_byhostname enter***");
	hostent = gethostbyname(TEST_DNS_NAME);
	if (!hostent) {
		sys_debug(0, "Error of gethostbyname: %s", strerror(errno));
	} else {
		print_hostent(hostent);
	}
	sys_debug(0, "***test_getip_byhostname exit***");
}

int socket_common_test()
{
	func_enter();
	test_getip_byhostname();


	func_exit();
	return 0;
}

