#ifndef _KERNEL_HLIST_H
#define _KERNEL_HLIST_H
#include <stddef.h>
#include <sys/types.h>

#ifndef container_of

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr); \
	(type *) ((char *)__mptr - offsetof(type, member));})
#endif

/*
	prefetch(x) attempts to pre-emptively get the memory pointed to
	by address "x" into the CPU L1 cache. 
	prefetch(x) should not cause any kind of exception, prefetch(0) is
	specifically ok.

	prefetch() should be defined by the architecture, if not, the 
	#define below provides a no-op define.	
	
	There are 3 prefetch() macros:
	
	prefetch(x)  	- prefetches the cacheline at "x" for read
	prefetchw(x)	- prefetches the cacheline at "x" for write
	spin_lock_prefetch(x) - prefectches the spinlock *x for taking
	
	there is also PREFETCH_STRIDE which is the architecure-prefered 
	"lookahead" size for prefetching streamed operations.
	
*/

/*
 *	These cannot be do{}while(0) macros. See the mental gymnastics in
 *	the loop macro.
 */
 
#ifndef ARCH_HAS_PREFETCH
static inline void prefetch(const void *x) {;}
#endif

#ifndef ARCH_HAS_PREFETCHW
static inline void prefetchw(const void *x) {;}
#endif

/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */
struct hlist_node {
	struct hlist_node *next, **pprev;
};

struct hlist_head {
	struct hlist_node *first;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = { .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}

static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
	*pprev = next;
	if (next)
		next->pprev = pprev;
}

/**
 * @n: the element to delete from the hash list.
 */
static inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = NULL;
	n->pprev = NULL;
}

static inline void hlist_del_init(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		INIT_HLIST_NODE(n);
	}
}

/*
 * hlist_replace_rcu - replace old entry by new one
 * @old : the element to be replaced
 * @new : the new element to insert
 *
 * The old entry will be replaced with the new entry atomically.
 */
static inline void hlist_replace_rcu(struct hlist_node *old,
					struct hlist_node *new)
{
	struct hlist_node *next = old->next;

	new->next = new;
	new->pprev = old->pprev;
	// smp_wmb();
	if (next)
		new->next->pprev = &new->next;
	*new->pprev = new;
	old->pprev = NULL;
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

/**
 * hlist_add_head_rcu
 * @n: the element to add to the hash list.
 * @h: the list to add to.
 *
 * Description:
 * Adds the specified element to the specified hlist,
 * while permitting racing traversals.
 *
 * The caller must take whatever precautions are necessary
 * (such as holding appropriate locks) to avoid racing
 * with another list-mutation primitive, such as hlist_add_head_rcu()
 * or hlist_del_rcu(), running on this same list.
 * However, it is perfectly legal to run concurrently with
 * the _rcu list-traversal primitives, such as
 * hlist_for_each_entry_rcu(), used to prevent memory-consistency
 * problems on Alpha CPUs.  Regardless of the type of CPU, the
 * list-traversal primitive must be guarded by rcu_read_lock().
 */
static inline void hlist_add_head_rcu(struct hlist_node *n,
					struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	n->pprev = &h->first;
	//smp_wmb();
	if (first)
		first->pprev = &n->next;
	h->first = n;
}

/* next must be != NULL */
static inline void hlist_add_before(struct hlist_node *n,
					struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

static inline void hlist_add_after(struct hlist_node *n,
					struct hlist_node *next)
{
	next->next = n->next;
	n->next = next;
	next->pprev = &n->next;

	if(next->next)
		next->next->pprev  = &next->next;
}

/**
 * hlist_add_before_rcu
 * @n: the new element to add to the hash list.
 * @next: the existing element to add the new element before.
 *
 * Description:
 * Adds the specified element to the specified hlist
 * before the specified node while permitting racing traversals.
 *
 * The caller must take whatever precautions are necessary
 * (such as holding appropriate locks) to avoid racing
 * with another list-mutation primitive, such as hlist_add_head_rcu()
 * or hlist_del_rcu(), running on this same list.
 * However, it is perfectly legal to run concurrently with
 * the _rcu list-traversal primitives, such as
 * hlist_for_each_entry_rcu(), used to prevent memory-consistency
 * problems on Alpha CPUs.
 */
static inline void hlist_add_before_rcu(struct hlist_node *n,
					struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	//smp_wmb();
	next->pprev = &n->next;
	*(n->pprev) = n;
}

/**
 * hlist_add_after_rcu
 * @prev: the existing element to add the new element after.
 * @n: the new element to add to the hash list.
 *
 * Description:
 * Adds the specified element to the specified hlist
 * after the specified node while permitting racing traversals.
 *
 * The caller must take whatever precautions are necessary
 * (such as holding appropriate locks) to avoid racing
 * with another list-mutation primitive, such as hlist_add_head_rcu()
 * or hlist_del_rcu(), running on this same list.
 * However, it is perfectly legal to run concurrently with
 * the _rcu list-traversal primitives, such as
 * hlist_for_each_entry_rcu(), used to prevent memory-consistency
 * problems on Alpha CPUs.
 */
static inline void hlist_add_after_rcu(struct hlist_node *prev,
				       struct hlist_node *n)
{
	n->next = prev->next;
	n->pprev = &prev->next;
	//smp_wmb();
	prev->next = n;
	if (n->next)
		n->next->pprev = &n->next;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
	for (pos = (head)->first; pos && ({ prefetch(pos->next); 1; }); \
	     pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
	for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
	     pos = n)

/**
 * hlist_for_each_entry	- iterate over list of given type
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry(tpos, pos, head, member)			 \
	for (pos = (head)->first;					 \
	     pos && ({ prefetch(pos->next); 1;}) &&			 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

/**
 * hlist_for_each_entry_continue - iterate over a hlist continuing after current point
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_continue(tpos, pos, member)		 \
	for (pos = (pos)->next;						 \
	     pos && ({ prefetch(pos->next); 1;}) &&			 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from current point
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_from(tpos, pos, member)			 \
	for (; pos && ({ prefetch(pos->next); 1;}) &&			 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

/**
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @n:		another &struct hlist_node to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_safe(tpos, pos, n, head, member) 		 \
	for (pos = (head)->first;					 \
	     pos && ({ n = pos->next; 1; }) && 				 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = n)

/**
 * hlist_for_each_entry_rcu - iterate over rcu list of given type
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 *
 * This list-traversal primitive may safely run concurrently with
 * the _rcu list-mutation primitives such as hlist_add_head_rcu()
 * as long as the traversal is guarded by rcu_read_lock().
 */
#if 0
#define hlist_for_each_entry_rcu(tpos, pos, head, member)		 \
	for (pos = (head)->first;					 \
	     rcu_dereference(pos) && ({ prefetch(pos->next); 1;}) &&	 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)
#endif

#endif /* _KERNEL_HLIST_H */
