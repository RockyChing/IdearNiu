/*
 * Port from Busybox version: 1.22.1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

#include "log_util.h"
#include "xfuncs.h"
#include "libbb.h"

#ifndef UINT_MAX
#define UINT_MAX	(~0U)
#endif


/**
 * 0 - simple version
 * 1 - full version
 */
#define ENABLE_FEATURE_FANCY_PING 1

enum {
	DEFDATALEN = 56,
	MAXIPLEN = 60,
	MAXICMPLEN = 76,
	MAX_DUP_CHK = (8 * 128),
	MAXWAIT = 10,
	PINGINTERVAL = 1, /* 1 second */
	pingsock = 0,
};

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

/* host: "1.2.3.4[:port]", "www.google.com[:port]"
 * port: if neither of above specifies port # */
static len_and_sockaddr* str2sockaddr(const char *host, int port, sa_family_t af)
{
	af = AF_INET;
	int rc;
	len_and_sockaddr *r = NULL;
	struct addrinfo *result = NULL;
	struct addrinfo *used_res;
	const char *org_host = host; /* only for error msg */
	struct addrinfo hint;

	memset(&hint, 0 , sizeof(hint));
	hint.ai_family = af;
	/* Need SOCK_STREAM, or else we get each address thrice (or more)
	 * for each possible socket type (tcp,udp,raw...): */
	hint.ai_socktype = SOCK_STREAM;
	rc = getaddrinfo(host, NULL, &hint, &result);
	if (rc || !result) {
		error("bad address '%s'", org_host);
		goto ret;
	}

	used_res = result;
	r = malloc(LSA_LEN_SIZE + used_res->ai_addrlen);
	r->len = used_res->ai_addrlen;
	memcpy(&r->u.sa, used_res->ai_addr, used_res->ai_addrlen);

 ret:
	if (result)
		freeaddrinfo(result);
	return r;
}

static len_and_sockaddr *xhost_and_af2sockaddr(const char *host, int port, sa_family_t af)
{
	return str2sockaddr(host, port, af);
}

















static void create_icmp_socket(void)
#define create_icmp_socket(lsa) create_icmp_socket()
{
	int sock = socket(AF_INET, SOCK_RAW, 1); /* 1 == ICMP */
	if (sock < 0) {
		if (errno != EPERM)
			error("bb_msg_can_not_create_raw_socket");
		/* We don't have root privileges.  Try SOCK_DGRAM instead.
		 * Linux needs net.ipv4.ping_group_range for this to work.
		 * MacOSX allows ICMP_ECHO, ICMP_TSTAMP or ICMP_MASKREQ
		 */
		sock = socket(AF_INET, SOCK_DGRAM, 1); /* 1 == ICMP */
		if (sock < 0)
			error("bb_msg_perm_denied_are_you_root");
	}

	if (sock < 0) {
		error("cannot create_icmp_socket");
		abort();
	} else {
		xmove_fd(sock, pingsock);
	}
}

#if !ENABLE_FEATURE_FANCY_PING

/* Simple version */

struct globals {
	char *hostname;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];
};

static struct globals global_data = { 0 };
static uint16_t nsend = 0;
#define G (*(struct globals*)&global_data)
#define INIT_G() do { } while (0)

static void noresp(int ign)
{
	printf("No response from %s\n", G.hostname);
	exit(0);
}

static void ping4(len_and_sockaddr *lsa)
{
	struct icmp *pkt;
	int c;

	pkt = (struct icmp *) G.packet;
	memset(pkt, 0, sizeof(G.packet));
	pkt->icmp_type = ICMP_ECHO;
#if 1
	/* fill data */
	pkt->icmp_code = 0;
	info("getpid: %d", getpid());
	pkt->icmp_id = getpid();
	pkt->icmp_seq = (uint16_t)(nsend ++);
	memset(pkt->icmp_data, 0xa5, 56); /* fill with pattern */
#endif
	pkt->icmp_cksum = inet_cksum((uint16_t *) pkt, sizeof(G.packet));

	info("send:");
	dump(G.packet, (DEFDATALEN + ICMP_MINLEN));
	xsendto(pingsock, G.packet, DEFDATALEN + ICMP_MINLEN, &lsa->u.sa, lsa->len);

	/* listen for replies */
	while (1) {
		c = recv(pingsock, G.packet, sizeof(G.packet), 0);
		if (c < 0) {
			if (errno != EINTR)
				error("recvfrom");
			continue;
		}

		info("\nrecv:");
		dump(G.packet, c);
		if (c >= 76) {			/* ip + icmp */
			struct iphdr *iphdr = (struct iphdr *) G.packet;

			pkt = (struct icmp *) (G.packet + (iphdr->ihl << 2));	/* skip ip hdr */
			if (pkt->icmp_type == ICMP_ECHOREPLY)
				break;
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		close(pingsock);
}

#define common_ping_main(af, argv) common_ping_main(argv)

static int common_ping_main(sa_family_t af, char **argv)
{
	len_and_sockaddr *lsa;

	INIT_G();
	argv++;

	G.hostname = *argv;
	if (!G.hostname) {
		error("ping need HOSTNAME");
		abort();
	}

	lsa = xhost_and_af2sockaddr(G.hostname, 0, AF_INET);
	/* Set timer _after_ DNS resolution */
	signal(SIGALRM, noresp);
	alarm(5); /* give the host 5000ms to respond */

	create_icmp_socket(lsa);
	ping4(lsa);
	printf("%s is alive!\n", G.hostname);
	return EXIT_SUCCESS;
}

#else /* FEATURE_FANCY_PING */
#include "time_util.h"

/* Full(er) version */
#define OPT_STRING ("qvc:s:t:w:W:I:n4" IF_PING6("6"))
enum {
	OPT_QUIET = 1 << 0,
	OPT_VERBOSE = 1 << 1,
	OPT_c = 1 << 2,
	OPT_s = 1 << 3,
	OPT_t = 1 << 4,
	OPT_w = 1 << 5,
	OPT_W = 1 << 6,
	OPT_I = 1 << 7,
	/*OPT_n = 1 << 8, - ignored */
	OPT_IPV4 = 1 << 9,
};


struct globals {
	int if_index;
	char *str_I;
	len_and_sockaddr *source_lsa;
	unsigned datalen;
	unsigned pingcount; /* must be int-sized */
	unsigned opt_ttl;
	unsigned long ntransmitted, nreceived, nrepeats;
	uint16_t myid;
	unsigned tmin, tmax; /* in us */
	unsigned long long tsum; /* in us, sum of all times */
	unsigned deadline;
	unsigned timeout;
	unsigned total_secs;
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
};

static struct globals global_data = { 0 };
#define G (*(struct globals*)&global_data)
#define if_index     (G.if_index    )
#define source_lsa   (G.source_lsa  )
#define str_I        (G.str_I       )
#define datalen      (G.datalen     )
#define pingcount    (G.pingcount   )
#define opt_ttl      (G.opt_ttl     )
#define myid         (G.myid        )
#define tmin         (G.tmin        )
#define tmax         (G.tmax        )
#define tsum         (G.tsum        )
#define deadline     (G.deadline    )
#define timeout      (G.timeout     )
#define total_secs   (G.total_secs  )
#define hostname     (G.hostname    )
#define dotted       (G.dotted      )
#define pingaddr     (G.pingaddr    )
#define rcvd_tbl     (G.rcvd_tbl    )

#define INIT_G() do { \
	datalen = DEFDATALEN; \
	timeout = MAXWAIT; \
	tmin = UINT_MAX; \
} while (0)


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
	exit(nrecv == 0 || (deadline && nrecv < pingcount));
}

static void sendping_tail(void (*sp)(int), int size_pkt)
{
	int sz;

	CLR((uint16_t)G.ntransmitted % MAX_DUP_CHK);
	G.ntransmitted++;

	size_pkt += datalen;

	/* sizeof(pingaddr) can be larger than real sa size, but I think
	 * it doesn't matter */
	sz = xsendto(pingsock, G.snd_packet, size_pkt, &pingaddr.sa, sizeof(pingaddr));
	if (sz != size_pkt)
		error("bb_msg_write_error");

	if (pingcount == 0 || deadline || G.ntransmitted < pingcount) {
		/* Didn't send all pings yet - schedule next in 1s */
		signal(SIGALRM, sp);
		if (deadline) {
			total_secs += PINGINTERVAL;
			if (total_secs >= deadline)
				signal(SIGALRM, print_stats_and_exit);
		}
		alarm(PINGINTERVAL);
	} else { /* -c NN, and all NN are sent (and no deadline) */
		/* Wait for the last ping to come back.
		 * -W timeout: wait for a response in seconds.
		 * Affects only timeout in absense of any responses,
		 * otherwise ping waits for two RTTs. */
		unsigned expire = timeout;

		if (G.nreceived) {
			/* approx. 2*tmax, in seconds (2 RTT) */
			expire = tmax / (512*1024);
			if (expire == 0)
				expire = 1;
		}
		signal(SIGALRM, print_stats_and_exit);
		alarm(expire);
	}
}

static void sendping4(int junk)
{
	struct icmp *pkt = G.snd_packet;

	//memset(pkt, 0, datalen + ICMP_MINLEN + 4); - G.snd_packet was xzalloced
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

	sendping_tail(sendping4, ICMP_MINLEN);
}

static const char *icmp_type_name(int id)
{
	switch (id) {
	case ICMP_ECHOREPLY:      return "Echo Reply";
	case ICMP_DEST_UNREACH:   return "Destination Unreachable";
	case ICMP_SOURCE_QUENCH:  return "Source Quench";
	case ICMP_REDIRECT:       return "Redirect (change route)";
	case ICMP_ECHO:           return "Echo Request";
	case ICMP_TIME_EXCEEDED:  return "Time Exceeded";
	case ICMP_PARAMETERPROB:  return "Parameter Problem";
	case ICMP_TIMESTAMP:      return "Timestamp Request";
	case ICMP_TIMESTAMPREPLY: return "Timestamp Reply";
	case ICMP_INFO_REQUEST:   return "Information Request";
	case ICMP_INFO_REPLY:     return "Information Reply";
	case ICMP_ADDRESS:        return "Address Mask Request";
	case ICMP_ADDRESSREPLY:   return "Address Mask Reply";
	default:                  return "unknown ICMP type";
	}
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
		unpack_tail(sz, tp,
			inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr),
			recv_seq, iphdr->ttl);
	} else if (icmppkt->icmp_type != ICMP_ECHO) {
		error("warning: got ICMP %d (%s)",
				icmppkt->icmp_type,
				icmp_type_name(icmppkt->icmp_type));
	}
}

static void ping4(len_and_sockaddr *lsa)
{
	int sockopt;

	pingaddr.sin = lsa->u.sin;
	if (source_lsa) {
		if (setsockopt(pingsock, IPPROTO_IP, IP_MULTICAST_IF,
				&source_lsa->u.sa, source_lsa->len))
			error("can't set multicast source interface");
		xbind(pingsock, &source_lsa->u.sa, source_lsa->len);
	}

	/* set recv buf (needed if we can get lots of responses: flood ping,
	 * broadcast ping etc) */
	sockopt = (datalen * 2) + 7 * 1024; /* giving it a bit of extra room */
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, &sockopt, sizeof(sockopt));

	if (opt_ttl != 0) {
		setsockopt(pingsock, IPPROTO_IP, IP_TTL, &opt_ttl, sizeof(opt_ttl));
		/* above doesnt affect packets sent to bcast IP, so... */
		setsockopt(pingsock, IPPROTO_IP, IP_MULTICAST_TTL, &opt_ttl, sizeof(opt_ttl));
	}

	signal(SIGINT, print_stats_and_exit);

	/* start the ping's going ... */
	sendping4(0);

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		socklen_t fromlen = (socklen_t) sizeof(from);
		int c;

		c = recvfrom(pingsock, G.rcv_packet, G.sizeof_rcv_packet, 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno != EINTR)
				error("recvfrom");
			continue;
		}
		unpack4(G.rcv_packet, c, &from);
		if (pingcount && G.nreceived >= pingcount)
			break;
	}
}

static void ping(len_and_sockaddr *lsa)
{
	printf("PING %s (%s)", hostname, dotted);
	if (source_lsa) {
		//printf(" from %s",
		//	xmalloc_sockaddr2dotted_noport(&source_lsa->u.sa));
	}
	printf(": %d data bytes\n", datalen);

	create_icmp_socket(lsa);

	G.sizeof_rcv_packet = datalen + MAXIPLEN + MAXICMPLEN;
	G.rcv_packet = xzalloc(G.sizeof_rcv_packet);
	G.snd_packet = xzalloc(datalen + ICMP_MINLEN + 4);
	ping4(lsa);
}

static int common_ping_main(int opt, char **argv)
{
	len_and_sockaddr *lsa;

	INIT_G();

	//opt |= getopt32(argv, OPT_STRING, &pingcount, &str_s, &opt_ttl, &deadline, &timeout, &str_I);
	pingcount = 3;
	myid = (uint16_t) getpid();
	hostname = argv[optind];
	lsa = xhost_and_af2sockaddr(hostname, 0, AF_INET);

	if (source_lsa && source_lsa->u.sa.sa_family != lsa->u.sa.sa_family)
		/* leaking it here... */
		source_lsa = NULL;

	dotted = xmalloc_sockaddr2dotted_noport(&lsa->u.sa);
	ping(lsa);
	print_stats_and_exit(0);
	return 0;
}
#endif /* FEATURE_FANCY_PING */


int main(int argc, char **argv)
{
#if !ENABLE_FEATURE_FANCY_PING
	return common_ping_main(AF_UNSPEC, argv);
#else
	return common_ping_main(0, argv);
#endif
}

