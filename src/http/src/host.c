#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netdb.h>
#define SET_H_ERRNO(err) ((void)(h_errno = (err)))

#include <errno.h>

#include "utils.h"
#include "host.h"
#include "url.h"

#ifndef NO_ADDRESS
#define NO_ADDRESS NO_DATA
#endif

#if !HAVE_DECL_H_ERRNO
extern int h_errno;
#endif

/* Lists of IP addresses that result from running DNS queries.  See
   lookup_host for details.  */
struct address_list {
	int count;                    /* number of adrresses */
	ip_address *addresses;        /* pointer to the string of addresses */

	int faulty;                   /* number of addresses known not to work. */
	bool connected;               /* whether we were able to connect to
	                               one of the addresses in the list,
	                               at least once. */

	int refcount;                 /* reference count; when it drops to
	                               0, the entry is freed. */
};

/* Get the bounds of the address list.  */
void address_list_get_bounds(const struct address_list *al, int *start, int *end)
{
  	*start = al->faulty;
  	*end   = al->count;
}

/* Return a pointer to the address at position POS.  */
const ip_address *address_list_address_at (const struct address_list *al, int pos)
{
	assert(pos >= al->faulty && pos < al->count);
	return al->addresses + pos;
}

/* Return true if AL contains IP, false otherwise.  */
bool address_list_contains (const struct address_list *al, const ip_address *ip)
{
	int i;
	switch (ip->family)	{
	case AF_INET:
		for (i = 0; i < al->count; i++) {
			ip_address *cur = al->addresses + i;
			if (cur->family == AF_INET && (cur->data.d4.s_addr == ip->data.d4.s_addr))
				return true;
		}
		return false;

	default:
		abort();
	}
}

/* Mark the INDEXth element of AL as faulty, so that the next time
   this address list is used, the faulty element will be skipped.  */
void address_list_set_faulty (struct address_list *al, int index)
{
	/** We assume that the address list is traversed in order, so that a
		"faulty" attempt is always preceded with all-faulty addresses,
		and this is how Wget uses it.  */
	assert (index == al->faulty);
	if (index != al->faulty) {
		logprintf(LOG_ALWAYS, "index: %d\nal->faulty: %d\n", index, al->faulty);
		logprintf(LOG_ALWAYS, ("Error in handling the address list.\n"));
		logprintf(LOG_ALWAYS, ("Please report this issue to bug-wget@gnu.org\n"));
		abort();
	}

	++al->faulty;
	if (al->faulty >= al->count)
		/* All addresses have been proven faulty.  Since there's not much
			sense in returning the user an empty address list the next
			time, we'll rather make them all clean, so that they can be
			retried anew.  */
		al->faulty = 0;
}

/* Set the "connected" flag to true.  This flag used by connect.c to
   see if the host perhaps needs to be resolved again.  */
void address_list_set_connected (struct address_list *al)
{
	al->connected = true;
}

/* Return the value of the "connected" flag. */
bool address_list_connected_p (const struct address_list *al)
{
	return al->connected;
}

/* Create an address_list from a NULL-terminated vector of IPv4
   addresses.  This kind of vector is returned by gethostbyname.  */
static struct address_list *address_list_from_ipv4_addresses (char **vec)
{
	int count, i;
	struct address_list *al = xnew0 (struct address_list);

	count = 0;
	while (vec[count])
		++count;
	assert (count > 0);

	al->addresses = xnew_array (ip_address, count);
	al->count     = count;
	al->refcount  = 1;

	for (i = 0; i < count; i++) {
		ip_address *ip = &al->addresses[i];
		ip->family = AF_INET;
		memcpy (IP_INADDR_DATA (ip), vec[i], 4);
	}

	return al;
}

static void address_list_delete(struct address_list *al)
{
	xfree (al->addresses);
	xfree (al);
}

/* Mark the address list as being no longer in use.  This will reduce
   its reference count which will cause the list to be freed when the
   count reaches 0.  */
void address_list_release(struct address_list *al)
{
	--al->refcount;
	log_info("Releasing 0x%0*lx (new refcount %d).\n", PTR_FORMAT (al), al->refcount);
	if (al->refcount <= 0) {
		DEBUGP (("Deleting unused 0x%0*lx.\n", PTR_FORMAT (al)));
		address_list_delete (al);
	}
}

/* Versions of gethostbyname and getaddrinfo that support timeout. */
struct ghbnwt_context {
	const char *host_name;
	struct hostent *hptr;
};

/* Just like gethostbyname, except it times out after TIMEOUT seconds.
   In case of timeout, NULL is returned and errno is set to ETIMEDOUT.
   The function makes sure that when NULL is returned for reasons
   other than timeout, errno is reset.  */
static struct hostent *gethostbyname_with_timeout(const char *host_name, double timeout)
{
	struct ghbnwt_context ctx;
	ctx.host_name = host_name;

  	ctx.hptr = gethostbyname(ctx.host_name);

	if (!ctx.hptr)
		errno = 0;
	return ctx.hptr;
}

/* Print error messages for host errors.  */
static const char *host_errstr(int error)
{
  /* Can't use switch since some of these constants can be equal,
     which makes the compiler complain about duplicate case
     values.  */
	if (error == HOST_NOT_FOUND
			|| error == NO_RECOVERY
			|| error == NO_DATA
			|| error == NO_ADDRESS)
		return ("Unknown host");
  	else if (error == TRY_AGAIN)
		/* Message modeled after what gai_strerror returns in similar
		   circumstances.  */
		return ("Temporary failure in name resolution");
	else
		return ("Unknown error");
}

/* Return a textual representation of ADDR, i.e. the dotted quad for
   IPv4 addresses, and the colon-separated list of hex words (with all
   zeros omitted, etc.) for IPv6 addresses.  */
const char *print_address(const ip_address *addr)
{
	static char buf[64];

	if (!inet_ntop (addr->family, IP_INADDR_DATA(addr), buf, sizeof(buf)))
		snprintf (buf, sizeof(buf), "<error: %s>", strerror (errno));

	return buf;
}

/* The following two functions were adapted from glibc's
   implementation of inet_pton, written by Paul Vixie. */
static bool is_valid_ipv4_address(const char *str, const char *end)
{
	bool saw_digit = false;
	int octets = 0;
	int val = 0;

	while (str < end) {
		int ch = *str++;

		if (ch >= '0' && ch <= '9') {
			val = val * 10 + (ch - '0');

			if (val > 255)
				return false;
			if (!saw_digit) {
				if (++octets > 4)
					return false;
				saw_digit = true;
			}
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return false;
			val = 0;
			saw_digit = false;
		} else {
			return false;
		}
	}

	if (octets < 4)
		return false;

	return true;
}

/* Simple host cache, used by lookup_host to speed up resolving.  The
   cache doesn't handle TTL because Wget is a fairly short-lived
   application.  Refreshing is attempted when connect fails, though --
   see connect_to_host.  */

/* Look up HOST in DNS and return a list of IP addresses.

   This function caches its result so that, if the same host is passed
   the second time, the addresses are returned without DNS lookup.
   globally disable caching.)

   The order of the returned addresses is affected by the setting of
   opt.prefer_family: if it is set to prefer_ipv4, IPv4 addresses are
   placed at the beginning; if it is prefer_ipv6, IPv6 ones are placed
   at the beginning; otherwise, the order is left intact.  The
   relative order of addresses with the same family is left
   undisturbed in either case.

   FLAGS can be a combination of:
     LH_SILENT  - don't print the "resolving ... done" messages.
     LH_BIND    - resolve addresses for use with bind, which under
                  IPv6 means to use AI_PASSIVE flag to getaddrinfo.
                  Passive lookups are not cached under IPv6.
     LH_REFRESH - if HOST is cached, remove the entry from the cache
                  and resolve it anew.  */
struct address_list *lookup_host (const char *host, int flags)
{
	struct address_list *al;
	bool silent = !!(flags & LH_SILENT);
	double timeout = opt.dns_timeout;
	func_enter();
	log_info("dns_timeout: %d", timeout);

	/* If we're not using getaddrinfo, first check if HOST specifies a
		numeric IPv4 address.  Some implementations of gethostbyname
		(e.g. the Ultrix one and possibly Winsock) don't accept
		dotted-decimal IPv4 addresses.  */
	uint32_t addr_ipv4 = (uint32_t)inet_addr(host);
	if (addr_ipv4 != (uint32_t) -1)	{
		/* No need to cache host->addr relation, just return the address.  */
		char *vec[2];
		vec[0] = (char *)&addr_ipv4;
		vec[1] = NULL;
		return address_list_from_ipv4_addresses(vec);
	}

	logprintf(LOG_VERBOSE, "Resolving %s... ", host);
	struct hostent *hptr = gethostbyname_with_timeout(host, timeout);
	if (!hptr) {
		if (!silent) {
			if (errno != ETIMEDOUT)
				logprintf (LOG_VERBOSE, "failed: %s.\n", host_errstr(h_errno));
			else
				logputs (LOG_VERBOSE, "failed: timed out.\n");
		}
		return NULL;
	}

	/* Do older systems have h_addr_list?  */
	al = address_list_from_ipv4_addresses(hptr->h_addr_list);
	/* Print the addresses determined by DNS lookup, but no more than
	   three if show_all_dns_entries is not specified.  */
	int i;
	int printmax = al->count;

	if (printmax > 3)
		printmax = 3;

	for (i = 0; i < printmax; i++) {
		logputs (LOG_VERBOSE, print_address (al->addresses + i));
		if (i < printmax - 1)
			logputs (LOG_VERBOSE, ", ");
	}

	if (printmax != al->count)
		logputs (LOG_VERBOSE, ", ...");
	logputs (LOG_VERBOSE, "\n");

	func_exit();
	return al;
}

/* Determine whether a URL is acceptable to be followed, according to
   a list of domains to accept.  */
bool accept_domain (struct url *u)
{
	assert (u->host != NULL);
	if (opt.domains) {
		if (!sufmatch ((const char **)opt.domains, u->host))
			return false;
    }

	return true;
}

/* Check whether WHAT is matched in LIST, each element of LIST being a
   pattern to match WHAT against, using backward matching (see
   match_backwards() in utils.c).

   If an element of LIST matched, 1 is returned, 0 otherwise.  */
bool sufmatch (const char **list, const char *what)
{
  int i, j, k, lw;

  lw = strlen (what);

  for (i = 0; list[i]; i++)
    {
      j = strlen (list[i]);
      if (lw < j)
        continue; /* what is no (sub)domain of list[i] */

      for (k = lw; j >= 0 && k >= 0; j--, k--)
        if (tolower (list[i][j]) != tolower (what[k]))
          break;

      /* Domain or subdomain match
       * k == -1: exact match
       * k >= 0 && what[k] == '.': subdomain match
       */
      if (j == -1 && (k == -1 || what[k] == '.'))
        return true;
    }

  return false;
}

void host_cleanup (void)
{

}

bool is_valid_ip_address (const char *name)
{
	const char *endp;

	endp = name + strlen(name);
	if (is_valid_ipv4_address(name, endp))
		return true;
	return false;
}
