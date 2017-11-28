#include <stdio.h>
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
#include <signal.h>

#include <sockets.h>
#include <utils.h>
#include <log_util.h>

struct ping_struct {
	void (*ping_process)(char *, ssize_t, struct msghdr *, struct timeval*);
	void (*ping_init)(const char *);
	void (*ping_send)(void);
	void (*ping_deinit)(struct ping_struct *);
	struct sockaddr *sasend; /* sockaddr{} for send, from getaddrinfo*/
	struct sockaddr *sarecv; /* sockaddr{} for receiving */
	char *host;
	char *remote;
	socklen_t salen; /* length of sockaddr {}s */
	int sockfd;
	int verbose;
	int nsend;
	int datalen;
	pid_t pid;
};

static char sendbuf[BUFSIZE];

/* function prototypes */
static void ping_init(const char *ip);
static void ping_deinit(struct ping_struct *pping);
static void ping_send(void);
static void ping_process(char *, ssize_t, struct msghdr *, struct timeval *);

static struct ping_struct _ping_data = {
	.ping_init = ping_init,
	.ping_send = ping_send,
	.ping_process = ping_process,
	.ping_deinit = ping_deinit,
	.host = NULL,
	.remote = NULL,
	.sasend = NULL,
	.sarecv = NULL,
};
static struct ping_struct *ping_data = &_ping_data;

static void ping_alarm (int signo)
{
	(*ping_data->ping_send)();
	alarm(1);
}

static void tv_sub (struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0) { /* out -= in */
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

static uint16_t ping_cksum (uint16_t * addr, int len)
{
	int nleft = len;
	uint32_t sum = 0;
	uint16_t *w = addr;
	uint16_t answer = 0;
	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		* (unsigned char *) (&answer) = * (unsigned char *) w;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
	sum += (sum >> 16); /* add carry */
	answer = ~sum; /* truncate to 16 bits */
	return (answer);
}

static void ping_init(const char *ip)
{
	struct sockaddr_in *sin;
	const socklen_t addrlen = sizeof(struct sockaddr_in);

	sin = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	if (!sin) {
		sys_debug(1, "ping malloc return NULL pointer");
		exit(0);
	}

	bzero(sin, addrlen);
	sin->sin_family = AF_INET;
	/* in_addr_t inet_addr(const char *cp); */
	sin->sin_addr.s_addr = inet_addr(ip);

	ping_data->remote = strdup(ip);
	ping_data->sasend = (struct sockaddr *) sin;
	ping_data->salen = addrlen;
	ping_data->verbose = 0;
	ping_data->nsend = 0;
	ping_data->datalen = 56; /* data that goes with ICMP echo request */
	ping_data->pid = getpid() & 0xffff; /* ICMP ID field is 16 bits */

	ping_data->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping_data->sockfd < 0) {
		sys_debug(1, "ERROR: ping invalid sockfd");
		exit(0);
	}
}

static void ping_process(char *ptr, ssize_t len, struct msghdr *msg, struct timeval *tvrecv)
{
	int hlenl, icmplen;
	double rtt;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;

	ip = (struct ip *) ptr; /* start of IP header */
	/*
	 * The header length field is the length of the entire IP header,
	 * including any options, in whole 32-bit words.
	 */
	hlenl = ip->ip_hl << 2; /* length of IP header */
	if (ip->ip_p != IPPROTO_ICMP)
		return; /* not ICMP */
	icmp = (struct icmp *) (ptr + hlenl); /* start of ICMP header */
	if ((icmplen = len - hlenl) < 8)
		return; /* malformed packet */

	if (icmp->icmp_type == ICMP_ECHOREPLY) {
		if (icmp->icmp_id != ping_data->pid)
			return; /* not a response to our ECHO_REQUEST */
		if (icmplen < 16) /* icmp header + sizeof(struct timeval) */
			return; /* not enough data to use */
		tvsend = (struct timeval *) icmp->icmp_data;
		tv_sub (tvrecv, tvsend);
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;
		printf ("%d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n",
				icmplen, ping_data->remote,
				icmp->icmp_seq, ip->ip_ttl, rtt);
	} else if (ping_data->verbose) {
		printf (" %d bytes from %s: type = %d, code = %d\n",
			icmplen, ping_data->remote,
			icmp->icmp_type, icmp->icmp_code);
	}
}

static void ping_send(void)
{
	int len;
	struct icmp *icmp;

	icmp = (struct icmp *) sendbuf;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = ping_data->pid;
	icmp->icmp_seq = ping_data->nsend ++;

	memset (icmp->icmp_data, 0xa5, ping_data->datalen); /* fill with pattern */
	gettimeofday ((struct timeval *) icmp->icmp_data, NULL);
	len = 8 + ping_data->datalen; /* checksum ICMP header and data */
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = ping_cksum ((u_short *) icmp, len);
	sendto(ping_data->sockfd, sendbuf, len, 0, ping_data->sasend, ping_data->salen);
}

void ping_readloop(void)
{
	int size;
	char recvbuf[BUFSIZE];
	char controlbuf[BUFSIZE];
	struct msghdr msg;
	struct iovec iov;
	ssize_t n;
	struct timeval tval;

	size = 60 * 1024; /* OK if setsockopt fails */
	setsockopt(ping_data->sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size));
	ping_alarm(SIGALRM); /* send first packet */
	iov.iov_base = recvbuf;
	iov.iov_len = sizeof(recvbuf);
	msg.msg_name = ping_data->sarecv;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = controlbuf;

	for (;;) {
		msg.msg_namelen = ping_data->salen;
		msg.msg_controllen = sizeof(controlbuf);
		n = recvmsg(ping_data->sockfd, &msg, 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			else
				printf("------------------recvmsg error\n\n");
		}

		gettimeofday (&tval, NULL);
		(*ping_data->ping_process)(recvbuf, n, &msg, &tval);
	}
}

static void ping_deinit(struct ping_struct *pping)
{
	if (!pping)
		return;
	if (pping->sasend)
		free(pping->sasend);
	else if (pping->sarecv)
		free(pping->sarecv);
	else if (pping->host)
		free(pping->host);
	else if (pping->remote)
		free(pping->remote);
	memset(pping, 0, sizeof(*pping));
}

/**
 * A ping to specific IP
 * @ip to ping
 * @timeout in millisecond
 * Note, only the superuser can create a raw socket
 */
int ping(const char *ip)
{
	setuid(getuid()); /* don't need special permissions any more */
	signal(SIGALRM, ping_alarm);

	if (ping_data->ping_init)
		(*ping_data->ping_init)(ip);

	ping_readloop();

	if (ping_data->ping_deinit)
		(*ping_data->ping_deinit)(ping_data);
	return 0;;
}

