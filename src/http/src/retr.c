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
#include "convert.h"

/* Total size of downloaded files.  Used to enforce quota.  */
SUM_SIZE_INT total_downloaded_bytes;

/* Total download time in seconds. */
double total_download_time;

/* If non-NULL, the stream to which output should be written.  This
   stream is initialized when `-O' is used.  */
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

/* Write data in BUF to OUT.  However, if *SKIP is non-zero, skip that
   amount of data and decrease SKIP.  Increment *TOTAL by the amount
   of data written.  If OUT2 is not NULL, also write BUF to OUT2.
   In case of error writing to OUT, -1 is returned.  In case of error
   writing to OUT2, -2 is returned.  Return 1 if the whole BUF was
   skipped.  */
static int write_data(FILE *out, const char *buf, int bufsize, wgint *written)
{
	if (out == NULL)
		return 1;

	if (out != NULL)
		fwrite(buf, 1, bufsize, out);
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
		fflush(out);

	if (out != NULL && ferror(out))
		return -1;
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
int fd_read_body(int fd, FILE *out, wgint toread, wgint startpos,
              wgint *qtyread, wgint *qtywritten, double *elapsed, int flags)
{
	int ret = 0;
	int dlbufsize = ((BUFSIZ) > (8 * 1024) ? (BUFSIZ) : (8 * 1024));
	char *dlbuf = xmalloc(dlbufsize);
	bool exact = !!(flags & rb_read_exactly);


	/* How much data we've read/written.  */
	wgint sum_read = 0;
	wgint sum_written = 0;

	/** Read from FD while there is data to read.  Normally toread==0
		means that it is unknown how much data is to arrive.  However, if
		EXACT is set, then toread==0 means what it says: that no data
		should be read.  */
	while (!exact || (sum_read < toread)) {
		int rdsize;
		double tmout = opt.read_timeout;
		rdsize = exact ? MIN(toread - sum_read, dlbufsize) : dlbufsize;

		ret = fd_read(fd, dlbuf, rdsize, tmout);
		if (ret < 0 && errno == ETIMEDOUT)
			ret = 0;                /* interactive timeout, handled above */
		else if (ret <= 0)
			break;                  /* EOF or read error */

		if (ret > 0) {
			int write_res;

			sum_read += ret;
			write_res = write_data(out, dlbuf, ret, &sum_written);
			if (write_res < 0) {
				ret = (write_res == -3) ? -3 : -2;
				goto out;
			}
		}
	}

	if (ret < -1)
		ret = -1;
out:
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
char *fd_read_hunk(int fd, hunk_terminator_t terminator, long sizehint, long maxsize)
{
	long bufsize = sizehint;
	char *hunk = xmalloc(bufsize);
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
			/* No terminator: simply read the data we know is (or should be) available.  */
			remain = pklen;


		/* Now, read the data.  Note that we make no assumptions about
			how much data we'll get.  (Some TCP stacks are notorious for
			read returning less data than the previous MSG_PEEK.)  */
		log_info("remain: %d", remain);
		rdlen = fd_read(fd, hunk + tail, remain, 0);
		log_info("rdlen: %d", rdlen);
		if (rdlen < 0) {
			xfree(hunk);
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

uerr_t retrieve_url(struct url *orig_parsed)
{
	uerr_t result;
	struct url *u = orig_parsed;

	result = NOCONERROR;

	if (u->scheme == SCHEME_HTTP
#ifdef HAVE_SSL
		|| u->scheme == SCHEME_HTTPS
#endif
			) {
		result = http_loop(u, orig_parsed);
	}
	if (orig_parsed != u) {
		url_free(u);
	}

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

