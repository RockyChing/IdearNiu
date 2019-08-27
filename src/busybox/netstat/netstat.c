#include <common_header.h>

#include <arpa/inet.h>
#include <xfuncs.h>

#define NETSTAT_CONNECTED 0x01
#define NETSTAT_LISTENING 0x02
#define NETSTAT_NUMERIC   0x04
/* Must match getopt32 option string */
#define NETSTAT_TCP       0x10
#define NETSTAT_UDP       0x20
#define NETSTAT_RAW       0x40
#define NETSTAT_UNIX      0x80
#define NETSTAT_ALLPROTO  (NETSTAT_TCP|NETSTAT_UDP|NETSTAT_RAW|NETSTAT_UNIX)

struct inet_params {
	int local_port, rem_port, state, uid;
	union {
		struct sockaddr     sa;
		struct sockaddr_in  sin;
	} localaddr, remaddr;
	unsigned long rxq, txq, inode;
};

static const char *const tcp_state[] = {
	"",
	"ESTABLISHED",
	"SYN_SENT",
	"SYN_RECV",
	"FIN_WAIT1",
	"FIN_WAIT2",
	"TIME_WAIT",
	"CLOSE",
	"CLOSE_WAIT",
	"LAST_ACK",
	"LISTEN",
	"CLOSING"
};

static int netstat_flag = NETSTAT_TCP;

static void build_ipv4_addr(char* local_addr, struct sockaddr_in* localaddr)
{
	sscanf(local_addr, "%X", &localaddr->sin_addr.s_addr);
	localaddr->sin_family = AF_INET;
}

static int scan_inet_proc_line(struct inet_params *param, char *line)
{
	int num;
	char local_addr[33], rem_addr[33]; /* 32 + 1 for NUL */

	num = sscanf(line,
			"%*d: %32[0-9A-Fa-f]:%X "
			"%32[0-9A-Fa-f]:%X %X "
			"%lX:%lX %*X:%*X "
			"%*X %d %*d %lu ",
			local_addr, &param->local_port,
			rem_addr, &param->rem_port, &param->state,
			&param->txq, &param->rxq,
			&param->uid, &param->inode);
	if (num < 9) {
		return 1; /* error */
	}

	if (strlen(local_addr) > 8) {
		// build_ipv6_addr(local_addr, &param->localaddr.sin6);
		// build_ipv6_addr(rem_addr, &param->remaddr.sin6);
	} else {
		build_ipv4_addr(local_addr, &param->localaddr.sin);
		build_ipv4_addr(rem_addr, &param->remaddr.sin);
	}
	return 0;
}

static int tcp_do_one(char *line)
{
	struct inet_params param;

	memset(&param, 0, sizeof(param));
	if (scan_inet_proc_line(&param, line))
		return 1;

	if (param.state == 0x0A) {
		char *ipstr = sockaddr2str(&param.localaddr.sa, 0);
		printf("Listen %s:%d\n", ipstr, param.local_port);
		free(ipstr);
	}

	return 0;
}

static void do_info(const char *file, int (*proc)(char *))
{
	int lnr;
	FILE *procinfo;
	char line[256] = { 0 };

	/* _stdin is just to save "r" param */
	procinfo = fopen(file, "r");
	if (procinfo == NULL) {
		return;
	}

	lnr = 0;
	/* 3: 0100007F:13AD 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 3177 1 ffffffc006278000 100 0 0 10 0  */
	while (fgets(line, sizeof(line), procinfo) != NULL) {
		/* line 0 is skipped */
		if (lnr && proc(line))
			log_warn("%s: bogus data on line %d", file, lnr + 1);
		lnr++;
	}
	fclose(procinfo);
}


int main(int argc, char *argv[])
{
	switch (netstat_flag) {
	case NETSTAT_TCP:
		do_info("/proc/net/tcp", tcp_do_one);
		break;
	default:
		break;
	}

	return 0;
}

