#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "retr.h"
#include "url.h"
#include "recur.h"
#include "http.h"
#include "host.h"
#include "connect.h"
#include "hash.h"
#include "convert.h"
#include "ptimer.h"
#include "progress.h"

/* Total size of downloaded files.  Used to enforce quota.  */
SUM_SIZE_INT total_downloaded_bytes;

/* Total download time in seconds. */
double total_download_time;

/* If non-NULL, the stream to which output should be written.  This
   stream is initialized when `-O' is used.  */
FILE *output_stream;

bool output_stream_regular;

static struct {
  wgint chunk_bytes;
  double chunk_start;
  double sleep_adjust;
} limit_data;

static void
limit_bandwidth_reset (void)
{
  xzero (limit_data);
}

/* Limit the bandwidth by pausing the download for an amount of time.
   BYTES is the number of bytes received from the network, and TIMER
   is the timer that started at the beginning of download.  */

static void
limit_bandwidth (wgint bytes, struct ptimer *timer)
{
  double delta_t = ptimer_read (timer) - limit_data.chunk_start;
  double expected;

  limit_data.chunk_bytes += bytes;

  /* Calculate the amount of time we expect downloading the chunk
     should take.  If in reality it took less time, sleep to
     compensate for the difference.  */
  expected = (double) limit_data.chunk_bytes / opt.limit_rate;

  if (expected > delta_t)
    {
      double slp = expected - delta_t + limit_data.sleep_adjust;
      double t0, t1;
      if (slp < 0.2)
        {
          DEBUGP (("deferring a %.2f ms sleep (%s/%.2f).\n",
                   slp * 1000, number_to_static_string (limit_data.chunk_bytes),
                   delta_t));
          return;
        }
      DEBUGP (("\nsleeping %.2f ms for %s bytes, adjust %.2f ms\n",
               slp * 1000, number_to_static_string (limit_data.chunk_bytes),
               limit_data.sleep_adjust));

      t0 = ptimer_read (timer);
      xsleep (slp);
      t1 = ptimer_measure (timer);

      /* Due to scheduling, we probably slept slightly longer (or
         shorter) than desired.  Calculate the difference between the
         desired and the actual sleep, and adjust the next sleep by
         that amount.  */
      limit_data.sleep_adjust = slp - (t1 - t0);
      /* If sleep_adjust is very large, it's likely due to suspension
         and not clock inaccuracy.  Don't enforce those.  */
      if (limit_data.sleep_adjust > 0.5)
        limit_data.sleep_adjust = 0.5;
      else if (limit_data.sleep_adjust < -0.5)
        limit_data.sleep_adjust = -0.5;
    }

  limit_data.chunk_bytes = 0;
  limit_data.chunk_start = ptimer_read (timer);
}

/* Write data in BUF to OUT.  However, if *SKIP is non-zero, skip that
   amount of data and decrease SKIP.  Increment *TOTAL by the amount
   of data written.  If OUT2 is not NULL, also write BUF to OUT2.
   In case of error writing to OUT, -1 is returned.  In case of error
   writing to OUT2, -2 is returned.  Return 1 if the whole BUF was
   skipped.  */

static int
write_data (FILE *out, FILE *out2, const char *buf, int bufsize,
            wgint *skip, wgint *written)
{
  if (out == NULL && out2 == NULL)
    return 1;
  if (*skip > bufsize)
    {
      *skip -= bufsize;
      return 1;
    }
  if (*skip)
    {
      buf += *skip;
      bufsize -= *skip;
      *skip = 0;
      if (bufsize == 0)
        return 1;
    }

  if (out != NULL)
    fwrite (buf, 1, bufsize, out);
  if (out2 != NULL)
    fwrite (buf, 1, bufsize, out2);
  *written += bufsize;

  /* Immediately flush the downloaded data.  This should not hinder
     performance: fast downloads will arrive in large 16K chunks
     (which stdio would write out immediately anyway), and slow
     downloads wouldn't be limited by disk speed.  */

  /* 2005-04-20 SMS.
     Perhaps it shouldn't hinder performance, but it sure does, at least
     on VMS (more than 2X).  Rather than speculate on what it should or
     shouldn't do, it might make more sense to test it.  Even better, it
     might be nice to explain what possible benefit it could offer, as
     it appears to be a clear invitation to poor performance with no
     actual justification.  (Also, why 16K?  Anyone test other values?)
  */
  if (out != NULL)
    fflush (out);
  if (out2 != NULL)
    fflush (out2);

  if (out != NULL && ferror (out))
    return -1;
  else if (out2 != NULL && ferror (out2))
    return -2;
  else
    return 0;
}

/* Read the contents of file descriptor FD until it the connection
   terminates or a read error occurs.  The data is read in portions of
   up to 16K and written to OUT as it arrives.  If opt.verbose is set,
   the progress is shown.

   TOREAD is the amount of data expected to arrive, normally only used
   by the progress gauge.

   STARTPOS is the position from which the download starts, used by
   the progress gauge.  If QTYREAD is non-NULL, the value it points to
   is incremented by the amount of data read from the network.  If
   QTYWRITTEN is non-NULL, the value it points to is incremented by
   the amount of data written to disk.  The time it took to download
   the data is stored to ELAPSED.

   If OUT2 is non-NULL, the contents is also written to OUT2.
   OUT2 will get an exact copy of the response: if this is a chunked
   response, everything -- including the chunk headers -- is written
   to OUT2.  (OUT will only get the unchunked response.)

   The function exits and returns the amount of data read.  In case of
   error while reading data, -1 is returned.  In case of error while
   writing data to OUT, -2 is returned.  In case of error while writing
   data to OUT2, -3 is returned.  */
int fd_read_body (const char *downloaded_filename, int fd, FILE *out, wgint toread, wgint startpos,
              wgint *qtyread, wgint *qtywritten, double *elapsed, int flags)
{
	log_debug("BUFSIZ: %d", BUFSIZ);
	log_debug("fd_read_body downloaded_filename: %s", downloaded_filename);
	log_debug("fd_read_body toread: %d", toread);
	log_debug("fd_read_body startpos: %d", startpos);
	log_debug("fd_read_body qtyread: %d", (qtyread ? *qtyread : -1));
	log_debug("fd_read_body qtywritten: %d", (qtywritten ? *qtywritten : -1));
	log_debug("fd_read_body elapsed: %f", (elapsed ? *elapsed : -0.1));

	int ret = 0;
#undef max
#define max(a,b) ((a) > (b) ? (a) : (b))
	int dlbufsize = max (BUFSIZ, 8 * 1024);
	char *dlbuf = xmalloc(dlbufsize);

	struct ptimer *timer = NULL;
	double last_successful_read_tm = 0;

	/* The progress gauge, set according to the user preferences. */
	void *progress = NULL;

	/* Non-zero if the progress gauge is interactive, i.e. if it can
	continually update the display.  When true, smaller timeout
	values are used so that the gauge can update the display when
	data arrives slowly. */
	bool progress_interactive = false;

	bool exact = !!(flags & rb_read_exactly);

	/* Used only by HTTP/HTTPS chunked transfer encoding.  */
	bool chunked = flags & rb_chunked_transfer_encoding;
	wgint skip = 0;

	/* How much data we've read/written.  */
	wgint sum_read = 0;
	wgint sum_written = 0;
	wgint remaining_chunk_size = 0;

  	if (flags & rb_skip_startpos)
		skip = startpos;

	if (opt.show_progress) {
		const char *filename_progress;
		/* If we're skipping STARTPOS bytes, pass 0 as the INITIAL
			argument to progress_create because the indicator doesn't
			(yet) know about "skipping" data.  */
		wgint start = skip ? 0 : startpos;
		if (opt.dir_prefix) {
			log_debug("opt.dir_prefix: %s", opt.dir_prefix);
			filename_progress = downloaded_filename + strlen (opt.dir_prefix) + 1;
		} else
			filename_progress = downloaded_filename;
		progress = progress_create(filename_progress, start, start + toread);
		progress_interactive = progress_interactive_p(progress);
	}

	log_debug("opt.limit_rate: %d", opt.limit_rate);
  	if (opt.limit_rate)
		limit_bandwidth_reset();

	/* A timer is needed for tracking progress, for throttling, and for
		tracking elapsed time.  If either of these are requested, start
		the timer.  */
	if (progress || opt.limit_rate || elapsed) {
		timer = ptimer_new();
		last_successful_read_tm = 0;
	}

	/* Use a smaller buffer for low requested bandwidths.  For example,
		with --limit-rate=2k, it doesn't make sense to slurp in 16K of
		data and then sleep for 8s.  With buffer size equal to the limit,
		we never have to sleep for more than one second.  */
	if (opt.limit_rate && opt.limit_rate < dlbufsize)
		dlbufsize = opt.limit_rate;

	/* Read from FD while there is data to read.  Normally toread==0
		means that it is unknown how much data is to arrive.  However, if
		EXACT is set, then toread==0 means what it says: that no data
		should be read.  */
	while (!exact || (sum_read < toread)) {
		int rdsize;
		double tmout = opt.read_timeout;

		if (chunked) {
			if (remaining_chunk_size == 0) {
				char *line = fd_read_line (fd);
				char *endl;
				if (line == NULL) {
					ret = -1;
					break;
				}

				remaining_chunk_size = strtol(line, &endl, 16);
				xfree(line);

				if (remaining_chunk_size < 0) {
					ret = -1;
					break;
				}

				if (remaining_chunk_size == 0) {
					ret = 0;
					line = fd_read_line(fd);
					if (line == NULL)
						ret = -1;
					else {
						xfree (line);
					}
					break;
				}
			}

			rdsize = MIN (remaining_chunk_size, dlbufsize);
		} else
			rdsize = exact ? MIN (toread - sum_read, dlbufsize) : dlbufsize;

		if (progress_interactive) {
			/* For interactive progress gauges, always specify a ~1s
				timeout, so that the gauge can be updated regularly even
				when the data arrives very slowly or stalls.  */
			tmout = 0.95;
			// log_debug("opt.read_timeout: %d", opt.read_timeout);
			if (opt.read_timeout) {
				double waittm;
				waittm = ptimer_read (timer) - last_successful_read_tm;
				if (waittm + tmout > opt.read_timeout) {
					/* Don't let total idle time exceed read timeout. */
					tmout = opt.read_timeout - waittm;
					if (tmout < 0) {
						/* We've already exceeded the timeout. */
						ret = -1, errno = ETIMEDOUT;
						break;
					}
				}
			}
		}

		ret = fd_read(fd, dlbuf, rdsize, tmout);
		if (progress_interactive && ret < 0 && errno == ETIMEDOUT)
			ret = 0;                /* interactive timeout, handled above */
		else if (ret <= 0)
			break;                  /* EOF or read error */

		if (progress || opt.limit_rate || elapsed) {
			ptimer_measure (timer);
			if (ret > 0)
				last_successful_read_tm = ptimer_read (timer);
		}

		if (ret > 0) {
			int write_res;

			sum_read += ret;
			write_res = write_data (out, NULL, dlbuf, ret, &skip, &sum_written);
			if (write_res < 0) {
				ret = (write_res == -3) ? -3 : -2;
				goto out;
			}
		}

		if (chunked) {
			remaining_chunk_size -= ret;
			if (remaining_chunk_size == 0) {
				char *line = fd_read_line (fd);
				if (line == NULL) {
					ret = -1;
					break;
				} else {
					xfree (line);
				}
			}
		}

		if (opt.limit_rate)
			limit_bandwidth (ret, timer);

		if (progress)
			progress_update (progress, ret, ptimer_read (timer));
	}

	if (ret < -1)
		ret = -1;
out:
	if (progress)
		progress_finish(progress, ptimer_read(timer));

	if (elapsed)
		*elapsed = ptimer_read(timer);
	if (timer)
		ptimer_destroy(timer);

	if (qtyread)
		*qtyread += sum_read;
	if (qtywritten)
		*qtywritten += sum_written;

	xfree(dlbuf);
	return ret;
}

/* Read a hunk of data from FD, up until a terminator.  The hunk is
   limited by whatever the TERMINATOR callback chooses as its
   terminator.  For example, if terminator stops at newline, the hunk
   will consist of a line of data; if terminator stops at two
   newlines, it can be used to read the head of an HTTP response.
   Upon determining the boundary, the function returns the data (up to
   the terminator) in malloc-allocated storage.

   In case of read error, NULL is returned.  In case of EOF and no
   data read, NULL is returned and errno set to 0.  In case of having
   read some data, but encountering EOF before seeing the terminator,
   the data that has been read is returned, but it will (obviously)
   not contain the terminator.

   The TERMINATOR function is called with three arguments: the
   beginning of the data read so far, the beginning of the current
   block of peeked-at data, and the length of the current block.
   Depending on its needs, the function is free to choose whether to
   analyze all data or just the newly arrived data.  If TERMINATOR
   returns NULL, it means that the terminator has not been seen.
   Otherwise it should return a pointer to the charactre immediately
   following the terminator.

   The idea is to be able to read a line of input, or otherwise a hunk
   of text, such as the head of an HTTP request, without crossing the
   boundary, so that the next call to fd_read etc. reads the data
   after the hunk.  To achieve that, this function does the following:

   1. Peek at incoming data.

   2. Determine whether the peeked data, along with the previously
      read data, includes the terminator.

      2a. If yes, read the data until the end of the terminator, and
          exit.

      2b. If no, read the peeked data and goto 1.

   The function is careful to assume as little as possible about the
   implementation of peeking.  For example, every peek is followed by
   a read.  If the read returns a different amount of data, the
   process is retried until all data arrives safely.

   SIZEHINT is the buffer size sufficient to hold all the data in the
   typical case (it is used as the initial buffer size).  MAXSIZE is
   the maximum amount of memory this function is allowed to allocate,
   or 0 if no upper limit is to be enforced.

   This function should be used as a building block for other
   functions -- see fd_read_line as a simple example.  */
char *fd_read_hunk (int fd, hunk_terminator_t terminator, long sizehint, long maxsize)
{
	long bufsize = sizehint;
	char *hunk = xmalloc (bufsize);
	int tail = 0;                 /* tail position in HUNK */

	assert (!maxsize || maxsize >= bufsize);
	while (1) {
		const char *end;
		int pklen, rdlen, remain;

		/* First, peek at the available data. */
		pklen = fd_peek(fd, hunk + tail, bufsize - 1 - tail, -1);
		if (pklen < 0) {
			xfree(hunk);
			return NULL;
		}

		log_info("pklen: %d", pklen);
		log_info("tail: %d", tail);
		end = terminator(hunk, hunk + tail, pklen);
		log_info("hunk: %p", hunk);
		log_info("end: %p", end);
		if (end) {
			/* The data contains the terminator: we'll drain the data up
				to the end of the terminator.  */
			remain = end - (hunk + tail);
			log_info("remain: %d", remain);
			assert (remain >= 0);
			if (remain == 0) {
				/* No more data needs to be read. */
				hunk[tail] = '\0';
				return hunk;
			}

			if (bufsize - 1 < tail + remain) {
				bufsize = tail + remain + 1;
				hunk = xrealloc (hunk, bufsize);
			}
		} else
			/* No terminator: simply read the data we know is (or should
				be) available.  */
			remain = pklen;


		/* Now, read the data.  Note that we make no assumptions about
			how much data we'll get.  (Some TCP stacks are notorious for
			read returning less data than the previous MSG_PEEK.)  */
		log_info("remain: %d", remain);
		rdlen = fd_read(fd, hunk + tail, remain, 0);
		log_info("rdlen: %d", rdlen);
		if (rdlen < 0) {
			xfree (hunk);
			return NULL;
		}

		tail += rdlen;
		hunk[tail] = '\0';

		if (rdlen == 0) {
			if (tail == 0) {
				/* EOF without anything having been read */
				xfree (hunk);
				errno = 0;
				return NULL;
			} else
				/* EOF seen: return the data we've read. */
				return hunk;
		}

		if (end && rdlen == remain)
			/* The terminator was seen and the remaining data drained --
				we got what we came for.  */
			return hunk;

		/* Keep looping until all the data arrives. */
		if (tail == bufsize - 1) {
			/* Double the buffer size, but refuse to allocate more than
				MAXSIZE bytes.  */
			if (maxsize && bufsize >= maxsize) {
				xfree (hunk);
				errno = ENOMEM;
				return NULL;
			}
			bufsize <<= 1;
			if (maxsize && bufsize > maxsize)
				bufsize = maxsize;
			hunk = xrealloc (hunk, bufsize);
		}
	}
}

static const char *
line_terminator (const char *start, const char *peeked, int peeklen)
{
  	const char *p = memchr (peeked, '\n', peeklen);
	if (p)
    	/* p+1 because the line must include '\n' */
    	return p + 1;
  	return NULL;
}

/* The maximum size of the single line we agree to accept.  This is
   not meant to impose an arbitrary limit, but to protect the user
   from Wget slurping up available memory upon encountering malicious
   or buggy server output.  Define it to 0 to remove the limit.  */
#define FD_READ_LINE_MAX 4096

/* Read one line from FD and return it.  The line is allocated using
   malloc, but is never larger than FD_READ_LINE_MAX.

   If an error occurs, or if no data can be read, NULL is returned.
   In the former case errno indicates the error condition, and in the
   latter case, errno is NULL.  */
char *fd_read_line (int fd)
{
	return fd_read_hunk (fd, line_terminator, 128, FD_READ_LINE_MAX);
}

/* Return a printed representation of the download rate, along with
   the units appropriate for the download speed.  */

const char *
retr_rate (wgint bytes, double secs)
{
  static char res[20];
  static const char *rate_names[] = {"B/s", "KB/s", "MB/s", "GB/s" };
  static const char *rate_names_bits[] = {"b/s", "Kb/s", "Mb/s", "Gb/s" };
  int units;

  double dlrate = calc_rate (bytes, secs, &units);
  /* Use more digits for smaller numbers (regardless of unit used),
     e.g. "1022", "247", "12.5", "2.38".  */
  snprintf (res, sizeof(res), "%.*f %s",
           dlrate >= 99.95 ? 0 : dlrate >= 9.995 ? 1 : 2,
           dlrate, !opt.report_bps ? rate_names[units]: rate_names_bits[units]);

  return res;
}

/* Calculate the download rate and trim it as appropriate for the
   speed.  Appropriate means that if rate is greater than 1K/s,
   kilobytes are used, and if rate is greater than 1MB/s, megabytes
   are used.

   UNITS is zero for B/s, one for KB/s, two for MB/s, and three for
   GB/s.  */

double
calc_rate (wgint bytes, double secs, int *units)
{
  double dlrate;
  double bibyte = 1000.0;

  if (!opt.report_bps)
    bibyte = 1024.0;


  assert (secs >= 0);
  assert (bytes >= 0);

  if (secs == 0)
    /* If elapsed time is exactly zero, it means we're under the
       resolution of the timer.  This can easily happen on systems
       that use time() for the timer.  Since the interval lies between
       0 and the timer's resolution, assume half the resolution.  */
    secs = ptimer_resolution () / 2.0;

  dlrate = convert_to_bits (bytes) / secs;
  if (dlrate < bibyte)
    *units = 0;
  else if (dlrate < (bibyte * bibyte))
    *units = 1, dlrate /= bibyte;
  else if (dlrate < (bibyte * bibyte * bibyte))
    *units = 2, dlrate /= (bibyte * bibyte);

  else
    /* Maybe someone will need this, one day. */
    *units = 3, dlrate /= (bibyte * bibyte * bibyte);

  return dlrate;
}


/* Retrieve the given URL.  Decides which loop to call -- HTTP, FTP,
   FTP, proxy, etc.  */

/* #### This function should be rewritten so it doesn't return from
   multiple points. */

static void dump_url_struct(struct url *u)
{
	if (u) {
		if (u->url)
			debug("url->url: \t%s", u->url);
		debug("url->scheme: \t%d", u->scheme);

		if (u->host)
			debug("url->host: \t%s", u->host);
		debug("url->port: \t%d", u->port);

		if (u->path)
			debug("url->path: \t%s", u->path);
		if (u->params)
			debug("url->params: \t%s", u->params);
		if (u->query)
			debug("url->query: \t%s", u->query);
		if (u->fragment)
			debug("url->fragment: \t%s", u->fragment);
		if (u->dir)
			debug("url->dir: \t%s", u->dir);
		if (u->file)
			debug("url->file: \t%s", u->file);
	}
}

uerr_t retrieve_url (struct url * orig_parsed, char **file,
              char **newloc, const char *refurl, int *dt, bool recursive, bool register_status)
{
	uerr_t result;
	int dummy;
	char *mynewloc;
	struct url *u = orig_parsed;
	char *local_file = NULL;

	/* If dt is NULL, use local storage.  */
	if (!dt) {
		dt = &dummy;
		dummy = 0;
	}

	if (newloc)
		*newloc = NULL;
	if (file)
		*file = NULL;

	if (!refurl)
		refurl = opt.referer;


	result = NOCONERROR;
	mynewloc = NULL;

	if (u->scheme == SCHEME_HTTP
#ifdef HAVE_SSL
		|| u->scheme == SCHEME_HTTPS
#endif
			) {
		result = http_loop(u, orig_parsed, &mynewloc, &local_file, refurl, dt);
	}

	xfree(mynewloc);
	if (file)
		*file = local_file ? local_file : NULL;
	else
		xfree(local_file);

	if (orig_parsed != u) {
		url_free (u);
	}

	if (newloc)
		*newloc = NULL;

	return result;
}

/* Find the URLs in the file and call retrieve_url() for each of them.
   If HTML is true, treat the file as HTML, and construct the URLs
   accordingly.  */

uerr_t
retrieve_from_file (const char *file, bool html, int *count)
{
  return 0;
}

/* Print `giving up', or `retrying', depending on the impending
   action.  N1 and N2 are the attempt number and the attempt limit.  */
void
printwhat (int n1, int n2)
{
  logputs (LOG_VERBOSE, (n1 == n2) ? ("Giving up.\n\n") : ("Retrying.\n\n"));
}

/* If opt.wait or opt.waitretry are specified, and if certain
   conditions are met, sleep the appropriate number of seconds.  See
   the documentation of --wait and --waitretry for more information.

   COUNT is the count of current retrieval, beginning with 1. */

void
sleep_between_retrievals (int count)
{
  static bool first_retrieval = true;

  if (first_retrieval)
    {
      /* Don't sleep before the very first retrieval. */
      first_retrieval = false;
      return;
    }

  if (opt.waitretry && count > 1)
    {
      /* If opt.waitretry is specified and this is a retry, wait for
         COUNT-1 number of seconds, or for opt.waitretry seconds.  */
      if (count <= opt.waitretry)
        xsleep (count - 1);
      else
        xsleep (opt.waitretry);
    }
  else if (opt.wait)
    {
      if (!opt.random_wait || count > 1)
        /* If random-wait is not specified, or if we are sleeping
           between retries of the same download, sleep the fixed
           interval.  */
        xsleep (opt.wait);
      else
        {
          /* Sleep a random amount of time averaging in opt.wait
             seconds.  The sleeping amount ranges from 0.5*opt.wait to
             1.5*opt.wait.  */
          double waitsecs = (0.5 + random_float ()) * opt.wait;
          DEBUGP (("sleep_between_retrievals: avg=%f,sleep=%f\n",
                   opt.wait, waitsecs));
          xsleep (waitsecs);
        }
    }
}

/* Free the linked list of urlpos.  */
void
free_urlpos (struct urlpos *l)
{
  while (l)
    {
      struct urlpos *next = l->next;
      if (l->url)
        url_free (l->url);
      xfree (l->local_name);
      xfree (l);
      l = next;
    }
}

/* Rotate FNAME opt.backups times */
void
rotate_backups(const char *fname)
{
#define SEP "."
#define AVSL 0

  int maxlen = strlen (fname) + sizeof (SEP) + numdigit (opt.backups) + AVSL;
  char *from = alloca (maxlen);
  char *to = alloca (maxlen);
  struct stat sb;
  int i;

  if (stat (fname, &sb) == 0)
    if (S_ISREG (sb.st_mode) == 0)
      return;

  for (i = opt.backups; i > 1; i--)
    {
      snprintf (to, maxlen, "%s%s%d", fname, SEP, i);
      snprintf (from, maxlen, "%s%s%d", fname, SEP, i - 1);
      if (rename (from, to))
        logprintf (LOG_NOTQUIET, "Failed to rename %s to %s: (%d) %s\n",
                   from, to, errno, strerror (errno));
    }

  snprintf (to, maxlen, "%s%s%d", fname, SEP, 1);
  if (rename(fname, to))
    logprintf (LOG_NOTQUIET, "Failed to rename %s to %s: (%d) %s\n",
               fname, to, errno, strerror (errno));
}


/* Set the file parameter to point to the local file string.  */
void
set_local_file (const char **file, const char *default_file)
{
    *file = default_file;
}

