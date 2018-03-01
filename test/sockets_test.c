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
#include <netinet/tcp.h>
#include <net/if.h>

#include <sockets.h>
#include <log_util.h>
#include <utils.h>

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

	if (ht->h_name) {
		printf("official name");
		printf("\t:%s\n", ht->h_name);
	}

	if (ht->h_aliases && *ht->h_aliases) {
		printf("alias name");
		for (; *ht->h_aliases; ht->h_aliases ++)
			printf("\t:%s\n", *ht->h_aliases);
	}

	if (ht->h_addr_list && *ht->h_addr_list) {
		printf("IP");
		for (; *ht->h_addr_list; ht->h_addr_list ++) {
			char *ipstr = *ht->h_addr_list;
			printf("\t\t:%d.%d.%d.%d\n",
				(uint8_t)ipstr[0], (uint8_t)ipstr[1], (uint8_t)ipstr[2], (uint8_t)ipstr[3]);
		}
	}
}

static void test_getip_byhostname(int argc, char *argv[])
{
	char const *host_name = NULL;
	char ip_array[4][16] = { { 0 }, };
	int i = 0;
	sys_debug(0, "***test_getip_byhostname enter***");
	if (argc == 1)
		host_name = TEST_DNS_NAME;
	else if (argc == 2)
		host_name = argv[1];
	else
		exit(0);
	getip_byhostname(host_name, ip_array);

	sys_debug(0, "DNS:\t%s", host_name);
	for (; i < ARRAY_SIZE(ip_array); i ++) {
		if (!ip_array[i][0]) break;
		sys_debug(0, "IP:\t%s", ip_array[i]);
	}

	sys_debug(0, "***test_getip_byhostname exit***");
}

static test_raw_socket()
{

}

int socket_common_test(int argc, char *argv[])
{
	func_enter();
	//test_getip_byhostname(argc, argv);
	//check_socket_options();
	test_raw_socket();

	func_exit();
	return 0;
}

