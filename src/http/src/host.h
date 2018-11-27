#ifndef HOST_H
#define HOST_H

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif

struct url;
struct address_list;

/* This struct defines an IP address, tagged with family type.  */
typedef struct {
	/* Address family, one of AF_INET or AF_INET6. */
	int family;

	/* The actual data, in the form of struct in_addr or in6_addr: */
	union {
		struct in_addr d4;      /* IPv4 address */
	} data;
} ip_address;

/* IP_INADDR_DATA macro returns a void pointer that can be interpreted
   as a pointer to struct in_addr in IPv4 context or a pointer to
   struct in6_addr in IPv4 context.  This pointer can be passed to
   functions that work on either, such as inet_ntop.  */
#define IP_INADDR_DATA(x) ((void *) &(x)->data)

enum {
	LH_SILENT  = 1,
	LH_BIND    = 2,
	LH_REFRESH = 4
};
struct address_list *lookup_host (const char *, int);

void address_list_get_bounds (const struct address_list *, int *, int *);
const ip_address *address_list_address_at (const struct address_list *, int);
bool address_list_contains (const struct address_list *, const ip_address *);
void address_list_set_faulty (struct address_list *, int);
void address_list_set_connected (struct address_list *);
bool address_list_connected_p (const struct address_list *);
void address_list_release (struct address_list *);

const char *print_address (const ip_address *);

bool is_valid_ip_address (const char *name);

bool accept_domain (struct url *);
bool sufmatch (const char **, const char *);

void host_cleanup (void);

#endif /* HOST_H */

