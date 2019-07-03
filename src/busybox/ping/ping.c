#include <common_header.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#define LOG_TAG "PING"

#include <log_util.h>

typedef struct len_and_sockaddr {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
	} u;
} len_and_sockaddr;


enum {
	DEFDATALEN = 56,
	MAXIPLEN = 60,
	MAXICMPLEN = 76,
	MAX_DUP_CHK = (8 * 128),
	MAXWAIT = 10,
	PINGINTERVAL = 10, /* 10 second */
};

static len_and_sockaddr g_las;

static void* xzalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		log_fatal("bb_msg_memory_exhausted");
	memset(ptr, 0, size);
	return ptr;
}

static unsigned long long monotonic_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/**
 * return true if little endian
 */
static int little_endian()
{
	union {
		long l;
		char c[sizeof(long)];
	} u;

	u.l = 1;
	return (u.c[sizeof(long) - 1] == 1);
}

static uint16_t inet_cksum(uint16_t *addr, int nleft)
{
	/*
	 * Our algorithm is simple, using a 32 bit accumulator,
	 * we add sequential 16 bit words to it, and at the end, fold
	 * back all the carry bits from the top 16 bits into the lower
	 * 16 bits.
	 */
	unsigned sum = 0;
	while (nleft > 1) {
		sum += *addr++;
		nleft -= 2;
	}

	/* Mop up an odd byte, if necessary */
	if (nleft == 1) {
		if (little_endian())
			sum += *(uint8_t*)addr;
		else
			sum += *(uint8_t*)addr << 8;
	}

	/* Add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
	sum += (sum >> 16);                     /* add carry */

	return (uint16_t)~sum;
}

static int setsockopt_int(int fd, int level, int optname, int optval)
{
	return setsockopt(fd, level, optname, &optval, sizeof(int));
}

static int setsockopt_1(int fd, int level, int optname)
{
	return setsockopt_int(fd, level, optname, 1);
}

static int setsockopt_SOL_SOCKET_int(int fd, int optname, int optval)
{
	return setsockopt_int(fd, SOL_SOCKET, optname, optval);
}

static int sock_close(int *sockfd)
{
	close(*sockfd);
	*sockfd = -1;
	return 0;
}

static int sock_set_blocking(int sockfd, int block)
{
	int res;
	int flags;
	flags = fcntl(sockfd, F_GETFL, 0);
	if (flags < 0) {
		error("F_GETFL on socket %d failed", sockfd);
		return -1;
	}

	if (block) {
		flags &= ~O_NONBLOCK;
	} else {
		flags |= O_NONBLOCK;
	}

	res = fcntl(sockfd, F_SETFL, flags);
	if (res < 0) {
		error("F_SETFL on socket %d failed", sockfd);
		return -1;
	}

	return res;
}

static ssize_t xsendto(int s, const void *buff, size_t len, const struct sockaddr *to,
				socklen_t tolen)
{
	ssize_t t, retry;

	retry = 0;
	for (t = 0 ; len > 0 ; ) {
		int n = sendto(s, buff + t, len, 0, to, tolen);
		if (n < 0) {
			int err_bak = errno;
			if ((err_bak == EAGAIN || err_bak == EINTR || err_bak == EINPROGRESS) && retry < 20) {
				retry ++;
				usleep(50000);
				continue;
			}

			log_debug("socket send error: %s", strerror(err_bak));
		    return -1;
		}
		t += n;
		len -= n;
		retry = 0;
	}

	return t;
}

struct ping_struct {
	int pingsock;
	unsigned datalen;
	unsigned opt_ttl;
	unsigned long ntransmitted, nreceived, nrepeats;
	uint16_t myid;
	uint8_t pattern;
	unsigned tmin, tmax; /* in us */
	unsigned long long tsum; /* in us, sum of all times */
	unsigned timeout;
	unsigned sizeof_rcv_packet;
	char *rcv_packet; /* [datalen + MAXIPLEN + MAXICMPLEN] */
	void *snd_packet; /* [datalen + ipv4/ipv6_const] */
	const char *hostname;
	const char *dotted;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
	} pingaddr;
	unsigned char rcvd_tbl[MAX_DUP_CHK / 8];
} FIX_ALIASING;
struct ping_struct G;

#define pingsock     (G.pingsock     )
#define datalen      (G.datalen     )
#define opt_ttl      (G.opt_ttl     )
#define myid         (G.myid        )
#define tmin         (G.tmin        )
#define tmax         (G.tmax        )
#define tsum         (G.tsum        )
#define timeout      (G.timeout     )
#define hostname     (G.hostname    )
#define dotted       (G.dotted      )
#define pingaddr     (G.pingaddr    )
#define rcvd_tbl     (G.rcvd_tbl    )

#define BYTE(bit)	rcvd_tbl[(bit)>>3]
#define MASK(bit)	(1 << ((bit) & 7))
#define SET(bit)	(BYTE(bit) |= MASK(bit))
#define CLR(bit)	(BYTE(bit) &= (~MASK(bit)))
#define TST(bit)	(BYTE(bit) & MASK(bit))

static void print_stats_and_exit(int junk)
{
	unsigned long ul;
	unsigned long nrecv;

	signal(SIGINT, SIG_IGN);

	nrecv = G.nreceived;
	printf("\n--- %s ping statistics ---\n"
		"%lu packets transmitted, "
		"%lu packets received, ",
		hostname, G.ntransmitted, nrecv
	);
	if (G.nrepeats)
		printf("%lu duplicates, ", G.nrepeats);
	ul = G.ntransmitted;
	if (ul != 0)
		ul = (ul - nrecv) * 100 / ul;
	printf("%lu%% packet loss\n", ul);
	if (tmin != UINT_MAX) {
		unsigned tavg = tsum / (nrecv + G.nrepeats);
		printf("round-trip min/avg/max = %u.%03u/%u.%03u/%u.%03u ms\n",
			tmin / 1000, tmin % 1000,
			tavg / 1000, tavg % 1000,
			tmax / 1000, tmax % 1000);
	}
	/* if condition is true, exit with 1 -- 'failure' */
	exit(nrecv == 0);
}

static void ping_socket_create(void)
{
	int sock;

	do {
		if (pingsock > 0)
			break;

		sock = socket(AF_INET, SOCK_RAW, 1); /* 1 == ICMP */
		if (sock < 0) {
			if (errno == EPERM)
				log_fatal("bb_msg_perm_denied_are_you_root");
			log_fatal("bb_msg_can_not_create_raw_socket");
			sleep(1);
			continue;
		}

		log_debug("create_icmp_socket OK");
		pingsock = sock;
		sock_set_blocking(pingsock, 0);
	} while (1);
}

static void sendping4(int junk)
{
	struct icmp *pkt = G.snd_packet;

	memset(pkt, G.pattern, datalen + ICMP_MINLEN + 4);
	pkt->icmp_type = ICMP_ECHO;
	/*pkt->icmp_code = 0;*/
	pkt->icmp_cksum = 0; /* cksum is calculated with this field set to 0 */
	pkt->icmp_seq = htons(G.ntransmitted); /* don't ++ here, it can be a macro */
	pkt->icmp_id = myid;

	/* If datalen < 4, we store timestamp _past_ the packet,
	 * but it's ok - we allocated 4 extra bytes in xzalloc() just in case.
	 */
	/*if (datalen >= 4)*/
		/* No hton: we'll read it back on the same machine */
		*(uint32_t*)&pkt->icmp_dun = monotonic_us();

	pkt->icmp_cksum = inet_cksum((uint16_t *) pkt, datalen + ICMP_MINLEN);

	int size_pkt = datalen + ICMP_MINLEN;
	int sz;

	CLR((uint16_t)G.ntransmitted % MAX_DUP_CHK);
	G.ntransmitted++;

	/* sizeof(pingaddr) can be larger than real sa size, but I think
	 * it doesn't matter */
	sz = xsendto(pingsock, G.snd_packet, size_pkt, &pingaddr.sa, sizeof(pingaddr));
	if (sz != size_pkt)
		log_fatal("bb_msg_write_error");
}

static void unpack_tail(int sz, uint32_t *tp,
		const char *from_str,
		uint16_t recv_seq, int ttl)
{
	unsigned char *b, m;
	const char *dupmsg = " (DUP!)";
	unsigned triptime = triptime; /* for gcc */

	if (tp) {
		/* (int32_t) cast is for hypothetical 64-bit unsigned */
		/* (doesn't hurt 32-bit real-world anyway) */
		triptime = (int32_t) ((uint32_t)monotonic_us() - *tp);
		tsum += triptime;
		if (triptime < tmin)
			tmin = triptime;
		if (triptime > tmax)
			tmax = triptime;
	}

	b = &BYTE(recv_seq % MAX_DUP_CHK);
	m = MASK(recv_seq % MAX_DUP_CHK);
	/*if TST(recv_seq % MAX_DUP_CHK):*/
	if (*b & m) {
		++G.nrepeats;
	} else {
		/*SET(recv_seq % MAX_DUP_CHK):*/
		*b |= m;
		++G.nreceived;
		dupmsg += 7;
	}

	printf("%d bytes from %s: seq=%u ttl=%d", sz,
		from_str, recv_seq, ttl);
	if (tp)
		printf(" time=%u.%03u ms", triptime / 1000, triptime % 1000);
	puts(dupmsg);
	fflush(NULL);
}
static void unpack4(char *buf, int sz, struct sockaddr_in *from)
{
	struct icmp *icmppkt;
	struct iphdr *iphdr;
	int hlen;

	/* discard if too short */
	if (sz < (datalen + ICMP_MINLEN))
		return;

	/* check IP header */
	iphdr = (struct iphdr *) buf;
	hlen = iphdr->ihl << 2;
	sz -= hlen;
	icmppkt = (struct icmp *) (buf + hlen);
	if (icmppkt->icmp_id != myid)
		return;				/* not our ping */

	if (icmppkt->icmp_type == ICMP_ECHOREPLY) {
		uint16_t recv_seq = ntohs(icmppkt->icmp_seq);
		uint32_t *tp = NULL;

		if (sz >= ICMP_MINLEN + sizeof(uint32_t))
			tp = (uint32_t *) icmppkt->icmp_data;
		unpack_tail(sz, tp, dotted,
			recv_seq, iphdr->ttl);
	} else if (icmppkt->icmp_type != ICMP_ECHO) {
		log_fatal("warning: got ICMP %d", icmppkt->icmp_type);
	}
}

static void ping4(len_and_sockaddr *lsa)
{
	int sockopt;

	pingaddr.sin = lsa->u.sin;

	/* enable broadcast pings */
	// setsockopt_broadcast(pingsock);

	/* set recv buf (needed if we can get lots of responses: flood ping,
	 * broadcast ping etc) */
	sockopt = (datalen * 2) + 7 * 1024; /* giving it a bit of extra room */
	setsockopt_SOL_SOCKET_int(pingsock, SO_RCVBUF, sockopt);

	if (opt_ttl != 0) {
		setsockopt_int(pingsock, IPPROTO_IP, IP_TTL, opt_ttl);
		/* above doesn't affect packets sent to bcast IP, so... */
		setsockopt_int(pingsock, IPPROTO_IP, IP_MULTICAST_TTL, opt_ttl);
	}

	/* start the ping's going ... */
	sendping4(0);

	/* listen for replies */
	while (pingsock > 0) {
		struct sockaddr_in from;
		socklen_t fromlen = (socklen_t) sizeof(from);

		struct timeval tv;
		fd_set rfds;
		int ret, c;
		int err_bak;

		memset(&tv, 0, sizeof(tv));
		FD_ZERO(&rfds);
		FD_SET(pingsock, &rfds);

		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		ret = select(pingsock + 1, &rfds, NULL, NULL, &tv);
		if (ret > 0) {
			if (FD_ISSET(pingsock, &rfds)) {
				c = recvfrom(pingsock, G.rcv_packet, G.sizeof_rcv_packet, 0, (struct sockaddr *) &from, &fromlen);
				if (c < 0) {
					err_bak = errno;
					if (err_bak == EAGAIN || err_bak == EINTR || err_bak == EINPROGRESS) {
						usleep(500000);
						continue;
					}

					log_fatal("recvfrom failed: %s", strerror(errno));
					sock_close(&pingsock);
					break;
				}

				log_debug("recv %d bytes", c);
				unpack4(G.rcv_packet, c, &from);
				if (c == 84)
					break;
			}
		} else if (ret == 0) {
			/* time expires, send data */
			debug("no data in 5 seconds");
			sock_close(&pingsock);
			break;
		} else {
			log_fatal("select() complains: %s", strerror(errno));
			sock_close(&pingsock);
			break;
		}
	}
}

static void ping_addr(const char *ip_dotted)
{
	struct sockaddr_in *psin = &g_las.u.sin;
	psin->sin_family = AF_INET; /* address family */
	psin->sin_port = ntohs(0);  /* port number */
	psin->sin_addr.s_addr = inet_addr(ip_dotted);
}

static void ping_init(void)
{
	pingsock = -1;
	datalen = DEFDATALEN;
	timeout = 5;
	tmin = UINT_MAX;

	myid = (uint16_t) getpid();
	hostname = "test-api-cnca.midea.com";
	dotted = "47.96.39.195";

	G.sizeof_rcv_packet = datalen + MAXIPLEN + MAXICMPLEN;
	G.rcv_packet = xzalloc(G.sizeof_rcv_packet);
	G.snd_packet = xzalloc(datalen + ICMP_MINLEN + 4);

	ping_addr(dotted);
}

int ping_main(int argc, char **argv)
{
	signal(SIGINT, print_stats_and_exit);
	ping_init();

	while (1) {
		ping_socket_create();
		ping4(&g_las);
		sleep(5);
	}

	return 0;
}

