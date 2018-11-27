#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "url.h"
#include "recur.h"
#include "utils.h"
#include "retr.h"
#include "host.h"
#include "res.h"
#include "convert.h"

/* Functions for maintaining the URL queue.  */

struct queue_element {
  const char *url;              /* the URL to download */
  const char *referer;          /* the referring document */
  int depth;                    /* the depth */
  bool html_allowed;            /* whether the document is allowed to
                                   be treated as HTML. */
  bool css_allowed;             /* whether the document is allowed to
                                   be treated as CSS. */
  struct queue_element *next;   /* next element in queue */
};

struct url_queue {
  struct queue_element *head;
  struct queue_element *tail;
  int count, maxcount;
};

/* Create a URL queue. */

static struct url_queue *
url_queue_new (void)
{
  struct url_queue *queue = xnew0 (struct url_queue);
  return queue;
}

/* Delete a URL queue. */

static void
url_queue_delete (struct url_queue *queue)
{
  xfree (queue);
}

/* Enqueue a URL in the queue.  The queue is FIFO: the items will be
   retrieved ("dequeued") from the queue in the order they were placed
   into it.  */

static void
url_enqueue (struct url_queue *queue,
             const char *url, const char *referer, int depth,
             bool html_allowed, bool css_allowed)
{
  struct queue_element *qel = xnew (struct queue_element);
  qel->url = url;
  qel->referer = referer;
  qel->depth = depth;
  qel->html_allowed = html_allowed;
  qel->css_allowed = css_allowed;
  qel->next = NULL;

  ++queue->count;
  if (queue->count > queue->maxcount)
    queue->maxcount = queue->count;

  DEBUGP (("Enqueuing %s at depth %d\n", url, depth));
  DEBUGP (("Queue count %d, maxcount %d.\n", queue->count, queue->maxcount));

  if (queue->tail)
    queue->tail->next = qel;
  queue->tail = qel;

  if (!queue->head)
    queue->head = queue->tail;
}

/* Take a URL out of the queue.  Return true if this operation
   succeeded, or false if the queue is empty.  */

static bool
url_dequeue (struct url_queue *queue,
             const char **url, const char **referer, int *depth,
             bool *html_allowed, bool *css_allowed)
{
  struct queue_element *qel = queue->head;

  if (!qel)
    return false;

  queue->head = queue->head->next;
  if (!queue->head)
    queue->tail = NULL;

  *url = qel->url;
  *referer = qel->referer;
  *depth = qel->depth;
  *html_allowed = qel->html_allowed;
  *css_allowed = qel->css_allowed;

  --queue->count;

  DEBUGP (("Dequeuing %s at depth %d\n",qel->url, qel->depth));
  DEBUGP (("Queue count %d, maxcount %d.\n", queue->count, queue->maxcount));

  xfree (qel);
  return true;
}

static void blacklist_add (struct hash_table *blacklist, const char *url)
{
  char *url_unescaped = xstrdup (url);

  url_unescape (url_unescaped);
  xfree (url_unescaped);
}

static int blacklist_contains (struct hash_table *blacklist, const char *url)
{
  return 0;
}

typedef enum
{
  WG_RR_SUCCESS, WG_RR_BLACKLIST, WG_RR_NOTHTTPS, WG_RR_NONHTTP, WG_RR_ABSOLUTE,
  WG_RR_DOMAIN, WG_RR_PARENT, WG_RR_LIST, WG_RR_REGEX, WG_RR_RULES,
  WG_RR_SPANNEDHOST, WG_RR_ROBOTS
} reject_reason;

static reject_reason download_child (const struct urlpos *, struct url *, int,
                              struct url *, struct hash_table *);
static void write_reject_log_header (FILE *);
static void write_reject_log_reason (FILE *, reject_reason,
                              const struct url *, const struct url *);

/* Based on the context provided by retrieve_tree, decide whether a
   URL is to be descended to.  This is only ever called from

   The most expensive checks (such as those for robots) are memoized
   by storing these URLs to BLACKLIST.  This may or may not help.  It
   will help if those URLs are encountered many times.  */

static reject_reason
download_child (const struct urlpos *upos, struct url *parent, int depth,
                  struct url *start_url_parsed, struct hash_table *blacklist)
{
  struct url *u = upos->url;
  const char *url = u->url;
  bool u_scheme_like_http;
  reject_reason reason = WG_RR_SUCCESS;

  DEBUGP (("Deciding whether to enqueue \"%s\".\n", url));

  if (blacklist_contains (blacklist, url))
    {
      DEBUGP (("Already on the black list.\n"));
      reason = WG_RR_BLACKLIST;
      goto out;
    }

  /* Several things to check for:
     1. if scheme is not https and https_only requested
     2. if scheme is not http, and we don't load it
     3. check for relative links (if relative_only is set)
     4. check for domain
     5. check for no-parent
     6. check for excludes && includes
     7. check for suffix
     8. check for same host (if spanhost is unset), with possible
     gethostbyname baggage
     9. check for robots.txt

     Addendum: If the URL is FTP, and it is to be loaded, only the
     domain and suffix settings are "stronger".

     Note that .html files will get loaded regardless of suffix rules
     (but that is remedied later with unlink) unless the depth equals
     the maximum depth.

     More time- and memory- consuming tests should be put later on
     the list.  */

  /* Determine whether URL under consideration has a HTTP-like scheme. */
  u_scheme_like_http = schemes_are_similar_p (u->scheme, SCHEME_HTTP);

  /* 1. Schemes other than HTTP are normally not recursed into. */
  if (!u_scheme_like_http && !((u->scheme == SCHEME_FTP
#ifdef HAVE_SSL
      || u->scheme == SCHEME_FTPS
#endif
      )))
    {
      DEBUGP (("Not following non-HTTP schemes.\n"));
      reason = WG_RR_NONHTTP;
      goto out;
    }

  /* 2. If it is an absolute link and they are not followed, throw it
     out.  */
  if (u_scheme_like_http)
    if (opt.relative_only && !upos->link_relative_p)
      {
        DEBUGP (("It doesn't really look like a relative link.\n"));
        reason = WG_RR_ABSOLUTE;
        goto out;
      }

  /* 3. If its domain is not to be accepted/looked-up, chuck it
     out.  */
  if (!accept_domain (u))
    {
      DEBUGP (("The domain was not accepted.\n"));
      reason = WG_RR_DOMAIN;
      goto out;
    }

  /* 4. Check for parent directory.

     If we descended to a different host or changed the scheme, ignore
     opt.no_parent.  Also ignore it for documents needed to display
     the parent page when in -p mode.  */
  if (opt.no_parent
      && schemes_are_similar_p (u->scheme, start_url_parsed->scheme)
      && 0 == strcasecmp (u->host, start_url_parsed->host)
      && (u->scheme != start_url_parsed->scheme
          || u->port == start_url_parsed->port))
    {
      if (!subdir_p (start_url_parsed->dir, u->dir))
        {
          DEBUGP (("Going to \"%s\" would escape \"%s\" with no_parent on.\n",
                   u->dir, start_url_parsed->dir));
          reason = WG_RR_PARENT;
          goto out;
        }
    }

  /* 5. If the file does not match the acceptance list, or is on the
     rejection list, chuck it out.  The same goes for the directory
     exclusion and inclusion lists.  */
  if (opt.includes || opt.excludes)
    {
      if (!accdir (u->dir))
        {
          DEBUGP (("%s (%s) is excluded/not-included.\n", url, u->dir));
          reason = WG_RR_LIST;
          goto out;
        }
    }
  if (!accept_url (url))
    {
      DEBUGP (("%s is excluded/not-included through regex.\n", url));
      reason = WG_RR_REGEX;
      goto out;
    }

  /* 6. Check for acceptance/rejection rules.  We ignore these rules
     for directories (no file name to match) and for non-leaf HTMLs,
     which can lead to other files that do need to be downloaded.  (-p
     automatically implies non-leaf because with -p we can, if
     necessary, overstep the maximum depth to get the page requisites.)  */
  if (u->file[0] != '\0' && !(has_html_suffix_p (u->file)))
    {
      if (!acceptable (u->file))
        {
          DEBUGP (("%s (%s) does not match acc/rej rules.\n",
                   url, u->file));
          reason = WG_RR_RULES;
          goto out;
        }
    }

  /* 7. */
  if (schemes_are_similar_p (u->scheme, parent->scheme))
    if (0 != strcasecmp (parent->host, u->host))
      {
        DEBUGP (("This is not the same hostname as the parent's (%s and %s).\n",
                 u->host, parent->host));
        reason = WG_RR_SPANNEDHOST;
        goto out;
      }
  out:

  if (reason == WG_RR_SUCCESS)
    /* The URL has passed all the tests.  It can be placed in the
       download queue. */
    DEBUGP (("Decided to load it.\n"));
  else
    DEBUGP (("Decided NOT to load it.\n"));

  return reason;
}

/* This function writes the rejected log header. */
static void
write_reject_log_header (FILE *f)
{
  if (!f)
    return;

  /* Note: Update this header when columns change in any way. */
  fprintf (f, "REASON\t"
    "U_URL\tU_SCHEME\tU_HOST\tU_PORT\tU_PATH\tU_PARAMS\tU_QUERY\tU_FRAGMENT\t"
    "P_URL\tP_SCHEME\tP_HOST\tP_PORT\tP_PATH\tP_PARAMS\tP_QUERY\tP_FRAGMENT\n");
}

/* This function writes a URL to the reject log. Internal use only. */
static void
write_reject_log_url (FILE *fp, const struct url *url)
{
  const char *escaped_str;
  const char *scheme_str;

  if (!fp)
    return;

  escaped_str = url_escape (url->url);

  switch (url->scheme)
    {
      case SCHEME_HTTP:  scheme_str = "SCHEME_HTTP";    break;
#ifdef HAVE_SSL
      case SCHEME_HTTPS: scheme_str = "SCHEME_HTTPS";   break;
      case SCHEME_FTPS:  scheme_str = "SCHEME_FTPS";    break;
#endif
      case SCHEME_FTP:   scheme_str = "SCHEME_FTP";     break;
      default:           scheme_str = "SCHEME_INVALID"; break;
    }

  fprintf (fp, "%s\t%s\t%s\t%i\t%s\t%s\t%s\t%s",
    escaped_str,
    scheme_str,
    url->host,
    url->port,
    url->path,
    url->params ? url->params : "",
    url->query ? url->query : "",
    url->fragment ? url->fragment : "");

  xfree (escaped_str);
}

/* This function writes out information on why a URL was rejected and its
   context from download_child such as the URL being rejected and it's
   parent's URL. The format it uses is comma separated values but with tabs. */
static void
write_reject_log_reason (FILE *fp, reject_reason reason,
                         const struct url *url, const struct url *parent)
{
  const char *reason_str;

  if (!fp)
    return;

  switch (reason)
    {
      case WG_RR_SUCCESS:     reason_str = "SUCCESS";     break;
      case WG_RR_BLACKLIST:   reason_str = "BLACKLIST";   break;
      case WG_RR_NOTHTTPS:    reason_str = "NOTHTTPS";    break;
      case WG_RR_NONHTTP:     reason_str = "NONHTTP";     break;
      case WG_RR_ABSOLUTE:    reason_str = "ABSOLUTE";    break;
      case WG_RR_DOMAIN:      reason_str = "DOMAIN";      break;
      case WG_RR_PARENT:      reason_str = "PARENT";      break;
      case WG_RR_LIST:        reason_str = "LIST";        break;
      case WG_RR_REGEX:       reason_str = "REGEX";       break;
      case WG_RR_RULES:       reason_str = "RULES";       break;
      case WG_RR_SPANNEDHOST: reason_str = "SPANNEDHOST"; break;
      case WG_RR_ROBOTS:      reason_str = "ROBOTS";      break;
      default:                reason_str = "UNKNOWN";     break;
    }

  fprintf (fp, "%s\t", reason_str);
  write_reject_log_url (fp, url);
  fprintf (fp, "\t");
  write_reject_log_url (fp, parent);
  fprintf (fp, "\n");
}

/* vim:set sts=2 sw=2 cino+={s: */
