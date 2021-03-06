#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <locale.h>
#include <fcntl.h>
#include <sys/types.h>

#include "log.h"
#include "http.h"
#include "utils.h"
#include "url.h"
#include "host.h"
#include "retr.h"
#include "connect.h"
#ifdef HAVE_SSL
#include "ssl.h"
#endif
#include "convert.h"
#ifndef O_BINARY
#define O_BINARY 0
#endif


/* Forward decls. */
struct http_stat;
static char *create_authorization_line (const char *, const char *,
                                        const char *, const char *,
                                        const char *, bool *, uerr_t *);
static char *basic_authentication_encode (const char *, const char *);
static bool known_authentication_scheme_p (const char *, const char *);
static void ensure_extension (struct http_stat *, const char *, int *);
static void load_cookies (void);

#define TEXTHTML_S "text/html"
#define TEXTXHTML_S "application/xhtml+xml"
#define TEXTCSS_S "text/css"

/* Some status code validation macros: */
#define H_10X(x)        (((x) >= 100) && ((x) < 200))
#define H_20X(x)        (((x) >= 200) && ((x) < 300))
#define H_PARTIAL(x)    ((x) == HTTP_STATUS_PARTIAL_CONTENTS)
#define H_REDIRECTED(x) ((x) == HTTP_STATUS_MOVED_PERMANENTLY          \
                         || (x) == HTTP_STATUS_MOVED_TEMPORARILY       \
                         || (x) == HTTP_STATUS_SEE_OTHER               \
                         || (x) == HTTP_STATUS_TEMPORARY_REDIRECT      \
                         || (x) == HTTP_STATUS_PERMANENT_REDIRECT)

/* HTTP/1.0 status codes from RFC1945, provided for reference.  */
/* Successful 2xx.  */
#define HTTP_STATUS_OK                    200
#define HTTP_STATUS_CREATED               201
#define HTTP_STATUS_ACCEPTED              202
#define HTTP_STATUS_NO_CONTENT            204
#define HTTP_STATUS_PARTIAL_CONTENTS      206

/* Redirection 3xx.  */
#define HTTP_STATUS_MULTIPLE_CHOICES      300
#define HTTP_STATUS_MOVED_PERMANENTLY     301
#define HTTP_STATUS_MOVED_TEMPORARILY     302
#define HTTP_STATUS_SEE_OTHER             303 /* from HTTP/1.1 */
#define HTTP_STATUS_NOT_MODIFIED          304
#define HTTP_STATUS_TEMPORARY_REDIRECT    307 /* from HTTP/1.1 */
#define HTTP_STATUS_PERMANENT_REDIRECT    308 /* from HTTP/1.1 */

/* Client error 4xx.  */
#define HTTP_STATUS_BAD_REQUEST           400
#define HTTP_STATUS_UNAUTHORIZED          401
#define HTTP_STATUS_FORBIDDEN             403
#define HTTP_STATUS_NOT_FOUND             404
#define HTTP_STATUS_RANGE_NOT_SATISFIABLE 416

/* Server errors 5xx.  */
#define HTTP_STATUS_INTERNAL              500
#define HTTP_STATUS_NOT_IMPLEMENTED       501
#define HTTP_STATUS_BAD_GATEWAY           502
#define HTTP_STATUS_UNAVAILABLE           503
#define HTTP_STATUS_GATEWAY_TIMEOUT       504

enum rp {
  rel_none, rel_name, rel_value, rel_both
};

struct request {
  const char *method;
  char *arg;

  struct request_header {
    char *name, *value;
    enum rp release_policy;
  } *headers;
  int hcount, hcapacity;
};


/* Create a new, empty request. Set the request's method and its
   arguments.  METHOD should be a literal string (or it should outlive
   the request) because it will not be freed.  ARG will be freed by
   request_free.  */

static struct request *
request_new (const char *method, char *arg)
{
  struct request *req = xnew0 (struct request);
  req->hcapacity = 8;
  req->headers = xnew_array (struct request_header, req->hcapacity);
  req->method = method;
  req->arg = arg;
  return req;
}

/* Return the method string passed with the last call to
   request_set_method.  */

static const char *
request_method (const struct request *req)
{
  return req->method;
}

/* Free one header according to the release policy specified with
   request_set_header.  */

static void
release_header (struct request_header *hdr)
{
  switch (hdr->release_policy)
    {
    case rel_none:
      break;
    case rel_name:
      xfree (hdr->name);
      break;
    case rel_value:
      xfree (hdr->value);
      break;
    case rel_both:
      xfree (hdr->name);
      xfree (hdr->value);
      break;
    }
}

/* Set the request named NAME to VALUE.  Specifically, this means that
   a "NAME: VALUE\r\n" header line will be used in the request.  If a
   header with the same name previously existed in the request, its
   value will be replaced by this one.  A NULL value means do nothing.

   RELEASE_POLICY determines whether NAME and VALUE should be released
   (freed) with request_free.  Allowed values are:

    - rel_none     - don't free NAME or VALUE
    - rel_name     - free NAME when done
    - rel_value    - free VALUE when done
    - rel_both     - free both NAME and VALUE when done

   Setting release policy is useful when arguments come from different
   sources.  For example:

     // Don't free literal strings!
     request_set_header (req, "Pragma", "no-cache", rel_none);

     // Don't free a global variable, we'll need it later.
     request_set_header (req, "Referer", opt.referer, rel_none);

     // Value freshly allocated, free it when done.
     request_set_header (req, "Range",
                         aprintf ("bytes=%s-", number_to_static_string (hs->restval)),
                         rel_value);
   */

static void request_set_header(struct request *req, const char *name, const char *value, enum rp release_policy)
{
	struct request_header *hdr;
	int i;

	if (!value) {
		/* A NULL value is a no-op; if freeing the name is requested,
		   free it now to avoid leaks.
		 */
		if (release_policy == rel_name || release_policy == rel_both)
			xfree (name);
		return;
	}

	for (i = 0; i < req->hcount; i++) {
		hdr = &req->headers[i];
		if (0 == strcasecmp(name, hdr->name)) {
			/* Replace existing header. */
			release_header (hdr);
			hdr->name = (void *)name;
			hdr->value = (void *)value;
			hdr->release_policy = release_policy;
			return;
		}
	}

	/* Install new header. */
	if (req->hcount >= req->hcapacity) {
		req->hcapacity <<= 1;
		req->headers = xrealloc (req->headers, req->hcapacity * sizeof (*hdr));
	}

	hdr = &req->headers[req->hcount++];
	hdr->name = (void *)name;
	hdr->value = (void *)value;
	hdr->release_policy = release_policy;
}

/* Like request_set_header, but sets the whole header line, as
   provided by the user using the `--header' option.  For example,
   request_set_user_header (req, "Foo: bar") works just like
   request_set_header (req, "Foo", "bar").  */

static void
request_set_user_header (struct request *req, const char *header)
{
  char *name;
  const char *p = strchr (header, ':');
  if (!p)
    return;
  BOUNDED_TO_ALLOCA (header, p, name);
  ++p;
  while (isspace (*p))
    ++p;
  request_set_header (req, xstrdup (name), (char *) p, rel_name);
}

/* Remove the header with specified name from REQ.  Returns true if
   the header was actually removed, false otherwise.  */

static bool
request_remove_header (struct request *req, const char *name)
{
  int i;
  for (i = 0; i < req->hcount; i++)
    {
      struct request_header *hdr = &req->headers[i];
      if (0 == strcasecmp (name, hdr->name))
        {
          release_header (hdr);
          /* Move the remaining headers by one. */
          if (i < req->hcount - 1)
            memmove (hdr, hdr + 1, (req->hcount - i - 1) * sizeof (*hdr));
          --req->hcount;
          return true;
        }
    }
  return false;
}

#define APPEND(p, str) do {                     \
	int A_len = strlen(str);                    \
	memcpy(p, str, A_len);                      \
	p += A_len;                                 \
} while (0)

/* Construct the request and write it to FD using fd_write. */
static int request_send(const struct request *req, int fd)
{
	char *request_string, *p;
	int i, size, write_error;

	/* Count the request size. */
	size = 0;

	/* METHOD " " ARG " " "HTTP/1.0" "\r\n" */
	size += strlen(req->method) + 1 + strlen(req->arg) + 1 + 8 + 2;
	for (i = 0; i < req->hcount; i++) {
		struct request_header *hdr = &req->headers[i];
		/* NAME ": " VALUE "\r\n" */
		size += strlen(hdr->name) + 2 + strlen(hdr->value) + 2;
	}

	/* "\r\n\0" */
	size += 3;
	p = request_string = xmalloc(size);

	/* Generate the request. */
	APPEND(p, req->method); *p++ = ' ';
	APPEND(p, req->arg);    *p++ = ' ';
	memcpy (p, "HTTP/1.1\r\n", 10); p += 10;

	for (i = 0; i < req->hcount; i++) {
		struct request_header *hdr = &req->headers[i];
		APPEND(p, hdr->name);
		*p++ = ':', *p++ = ' ';
		APPEND(p, hdr->value);
		*p++ = '\r', *p++ = '\n';
	}

  	*p++ = '\r', *p++ = '\n', *p++ = '\0';
	assert (p - request_string == size);

#undef APPEND
	debug("\n+++request begin+++\n%s---request end---", request_string);

	/* Send the request to the server. */
	write_error = fd_write(fd, request_string, size - 1, -1);
	if (write_error < 0)
		logprintf(LOG_VERBOSE, "Failed writing HTTP request: %s.\n", fd_errstr(fd));
	xfree(request_string);
	return write_error;
}

/* Release the resources used by REQ.
   It is safe to call it with a vaild pointer to a NULL pointer.
   It is not safe to call it with an invalid or NULL pointer.  */

static void
request_free (struct request **req_ref)
{
  int i;
  struct request *req = *req_ref;

  if (!req)
    return;

  xfree (req->arg);
  for (i = 0; i < req->hcount; i++)
    release_header (&req->headers[i]);
  xfree (req->headers);
  xfree (req);
  *req_ref = NULL;
}

/* Send the contents of FILE_NAME to SOCK.  Make sure that exactly
   PROMISED_SIZE bytes are sent over the wire -- if the file is
   longer, read only that much; if the file is shorter, report an error.
   If warc_tmp is set to a file pointer, the post data will
   also be written to that file.  */

static int
body_file_send (int sock, const char *file_name, wgint promised_size, FILE *warc_tmp)
{
  static char chunk[8192];
  wgint written = 0;
  int write_error;
  FILE *fp;

  DEBUGP (("[writing BODY file %s ... ", file_name));

  fp = fopen (file_name, "rb");
  if (!fp)
    return -1;
  while (!feof (fp) && written < promised_size)
    {
      int towrite;
      int length = fread (chunk, 1, sizeof (chunk), fp);
      if (length == 0)
        break;
      towrite = MIN (promised_size - written, length);
      write_error = fd_write (sock, chunk, towrite, -1);
      if (write_error < 0)
        {
          fclose (fp);
          return -1;
        }
      if (warc_tmp != NULL)
        {
          /* Write a copy of the data to the WARC record. */
          int warc_tmp_written = fwrite (chunk, 1, towrite, warc_tmp);
          if (warc_tmp_written != towrite)
            {
              fclose (fp);
              return -2;
            }
        }
      written += towrite;
    }
  fclose (fp);

  /* If we've written less than was promised, report a (probably
     nonsensical) error rather than break the promise.  */
  if (written < promised_size)
    {
      errno = EINVAL;
      return -1;
    }

  assert (written == promised_size);
  DEBUGP (("done]\n"));
  return 0;
}

/* Determine whether [START, PEEKED + PEEKLEN) contains an empty line.
   If so, return the pointer to the position after the line, otherwise
   return NULL.  This is used as callback to fd_read_hunk.  The data
   between START and PEEKED has been read and cannot be "unread"; the
   data after PEEKED has only been peeked.  */
static const char *response_head_terminator (const char *start, const char *peeked, int peeklen)
{
	const char *p, *end;
	/* If at first peek, verify whether HUNK starts with "HTTP".  If
	   not, this is a HTTP/0.9 request and we must bail out without
	   reading anything.  */
	if (start == peeked && 0 != memcmp(start, "HTTP", MIN(peeklen, 4)))
		return start;

	/* Look for "\n[\r]\n", and return the following position if found.
	   Start two chars before the current to cover the possibility that
	   part of the terminator (e.g. "\n\r") arrived in the previous
	   batch.  */
	p = peeked - start < 2 ? start : peeked - 2;
	end = peeked + peeklen;

	/* Check for \n\r\n or \n\n anywhere in [p, end-2). */
	for (; p < end - 2; p++)
		if (*p == '\n') {
			if (p[1] == '\r' && p[2] == '\n')
				return p + 3;
			else if (p[1] == '\n')
				return p + 2;
		}

	/* p==end-2: check for \n\n directly preceding END. */
	if (p[0] == '\n' && p[1] == '\n')
		return p + 2;

	return NULL;
}

/* The maximum size of a single HTTP response we care to read.  Rather
   than being a limit of the reader implementation, this limit
   prevents Wget from slurping all available memory upon encountering
   malicious or buggy server output, thus protecting the user.  Define
   it to 0 to remove the limit.  */

#define HTTP_RESPONSE_MAX_SIZE 65536

/* Read the HTTP request head from FD and return it.  The error
   conditions are the same as with fd_read_hunk.

   To support HTTP/0.9 responses, this function tries to make sure
   that the data begins with "HTTP".  If this is not the case, no data
   is read and an empty request is returned, so that the remaining
   data can be treated as body.  */
static char *read_http_response_head(int fd)
{
  	return fd_read_hunk(fd, response_head_terminator, 512, HTTP_RESPONSE_MAX_SIZE);
}

struct response {
	/* The response data. */
	const char *data;
	/* The array of pointers that indicate where each header starts.
	 For example, given this HTTP response:

	   HTTP/1.0 200 Ok
	   Description: some
	    text
	   Etag: x

	 The headers are located like this:

	 "HTTP/1.0 200 Ok\r\nDescription: some\r\n text\r\nEtag: x\r\n\r\n"
	 ^                   ^                             ^          ^
	 headers[0]          headers[1]                    headers[2] headers[3]

	 I.e. headers[0] points to the beginning of the request,
	 headers[1] points to the end of the first header and the
	 beginning of the second one, etc.
	*/

	const char **headers;
};

/* Create a new response object from the text of the HTTP response,
   available in HEAD.  That text is automatically split into
   constituent header lines for fast retrieval using
   resp_header_*.  */
static struct response *resp_new (char *head)
{
	char *hdr;
  	int count, size;

  	struct response *resp = xnew0(struct response);
  	resp->data = head;

	if (*head == '\0') {
      	/* Empty head means that we're dealing with a headerless
           (HTTP/0.9) response.  In that case, don't set HEADERS at all.  */
		return resp;
    }

	/* Split HEAD into header lines, so that resp_header_* functions
	   don't need to do this over and over again.  */
	size = count = 0;
	hdr = head;
	while (1) {
		DO_REALLOC(resp->headers, size, count + 1, const char *);
		resp->headers[count++] = hdr;

		/* Break upon encountering an empty line. */
		if (!hdr[0] || (hdr[0] == '\r' && hdr[1] == '\n') || hdr[0] == '\n')
			break;

		/* Find the end of HDR, including continuations. */
		for (;;) {
			char *end = strchr (hdr, '\n');

			if (end)
				hdr = end + 1;
			else
				hdr += strlen (hdr);

			if (*hdr != ' ' && *hdr != '\t')
				break;

			// continuation, transform \r and \n into spaces
			*end = ' ';
			if (end > head && end[-1] == '\r')
				end[-1] = ' ';
			}
		}
		DO_REALLOC(resp->headers, size, count + 1, const char *);
		resp->headers[count] = NULL;

		return resp;
}

/* Locate the header named NAME in the request data, starting with
   position START.  This allows the code to loop through the request
   data, filtering for all requests of a given name.  Returns the
   found position, or -1 for failure.  The code that uses this
   function typically looks like this:

     for (pos = 0; (pos = resp_header_locate (...)) != -1; pos++)
       ... do something with header ...

   If you only care about one header, use resp_header_get instead of
   this function.  */
static int resp_header_locate(const struct response *resp, const char *name, int start,
                    const char **begptr, const char **endptr)
{
	int i;
	const char **headers = resp->headers;
	int name_len;

	if (!headers || !headers[1])
		return -1;

	name_len = strlen(name);
	if (start > 0)
		i = start;
	else
		i = 1;

	for (; headers[i + 1]; i++) {
		const char *b = headers[i];
		const char *e = headers[i + 1];
		if (e - b > name_len && b[name_len] == ':' && 0 == strncasecmp (b, name, name_len)) {
			b += name_len + 1; // 1 for ':'
			while (b < e && isspace(*b))
				++b;
			while (b < e && isspace(e[-1]))
				--e;
			*begptr = b;
			*endptr = e;
			return i;
		}
	}
	return -1;
}

/* Find and retrieve the header named NAME in the request data.  If
   found, set *BEGPTR to its starting, and *ENDPTR to its ending
   position, and return true.  Otherwise return false.

   This function is used as a building block for resp_header_copy
   and resp_header_strdup.  */
static bool resp_header_get (const struct response *resp, const char *name,
                 const char **begptr, const char **endptr)
{
	int pos = resp_header_locate(resp, name, 0, begptr, endptr);
	return pos != -1;
}

/* Copy the response header named NAME to buffer BUF, no longer than
   BUFSIZE (BUFSIZE includes the terminating 0).  If the header
   exists, true is returned, false otherwise.  If there should be no
   limit on the size of the header, use resp_header_strdup instead.

   If BUFSIZE is 0, no data is copied, but the boolean indication of
   whether the header is present is still returned. */
static bool resp_header_copy(const struct response *resp, const char *name,
                  char *buf, int bufsize)
{
	const char *b, *e;
	if (!resp_header_get(resp, name, &b, &e))
		return false;
	if (bufsize) {
		int len = MIN(e - b, bufsize - 1);
		memcpy (buf, b, len);
		buf[len] = '\0';
	}

	return true;
}

/* Return the value of header named NAME in RESP, allocated with
   malloc.  If such a header does not exist in RESP, return NULL.  */
static char *resp_header_strdup (const struct response *resp, const char *name)
{
	const char *b, *e;
	if (!resp_header_get (resp, name, &b, &e))
		return NULL;
	return strdupdelim (b, e);
}

/* Parse the HTTP status line, which is of format:

   HTTP-Version SP Status-Code SP Reason-Phrase

   The function returns the status-code, or -1 if the status line
   appears malformed.  The pointer to "reason-phrase" message is
   returned in *MESSAGE.  */
static int resp_status(const struct response *resp, char **message)
{
	int status;
	const char *p, *end;

	if (!resp->headers)	{
		/* For a HTTP/0.9 response, assume status 200. */
		if (message)
			*message = xstrdup("No headers, assuming HTTP/0.9");
		return 200;
	}

	p = resp->headers[0]; // HTTP/1.1 200 OK
	end = resp->headers[1];

	if (!end)
		return -1;

	/* "HTTP" */
	if (end - p < 4 || 0 != strncmp (p, "HTTP", 4))
		return -1;
	p += 4;

	if (p < end && *p == '/') {
		++p;
		while (p < end && isdigit(*p))
			++p;
		if (p < end && *p == '.')
			++p;
		while (p < end && isdigit (*p))
			++p;
	}

	while (p < end && isspace (*p))
		++p;
	if (end - p < 3 || !isdigit (p[0]) || !isdigit (p[1]) || !isdigit (p[2]))
		return -1;

	status = 100 * (p[0] - '0') + 10 * (p[1] - '0') + (p[2] - '0');
	p += 3;

	if (message) {
		while (p < end && isspace (*p))
			++p;
		while (p < end && isspace (end[-1]))
			--end;
		*message = strdupdelim (p, end);
	}

	return status;
}

/* Release the resources used by RESP.
   It is safe to call it with a valid pointer to a NULL pointer.
   It is not safe to call it with a invalid or NULL pointer.  */

static void
resp_free (struct response **resp_ref)
{
  struct response *resp = *resp_ref;

  if (!resp)
    return;

  xfree (resp->headers);
  xfree (resp);

  *resp_ref = NULL;
}

/* Print a single line of response, the characters [b, e).  We tried
   getting away with
      logprintf (LOG_VERBOSE, "%s%.*s\n", prefix, (int) (e - b), b);
   but that failed to escape the non-printable characters and, in fact,
   caused crashes in UTF-8 locales.  */

static void
print_response_line (const char *prefix, const char *b, const char *e)
{
  char *copy;
  BOUNDED_TO_ALLOCA(b, e, copy);
  logprintf (LOG_ALWAYS, "%s%s\n", prefix, copy);
}

/* Print the server response, line by line, omitting the trailing CRLF
   from individual header lines, and prefixed with PREFIX.  */

static void
print_server_response (const struct response *resp, const char *prefix)
{
  int i;
  if (!resp->headers)
    return;
  for (i = 0; resp->headers[i + 1]; i++)
    {
      const char *b = resp->headers[i];
      const char *e = resp->headers[i + 1];
      /* Skip CRLF */
      if (b < e && e[-1] == '\n')
        --e;
      if (b < e && e[-1] == '\r')
        --e;
      print_response_line (prefix, b, e);
    }
}

/* Parse the `Content-Range' header and extract the information it
   contains.  Returns true if successful, false otherwise.  */
static bool parse_content_range(const char *hdr, int *first_byte_ptr, int *last_byte_ptr, int *entity_length_ptr)
{
	int num;

	/* Ancient versions of Netscape proxy server, presumably predating
		rfc2068, sent out `Content-Range' without the "bytes" specifier.  */
	if (0 == strncasecmp (hdr, "bytes", 5)) {
		hdr += 5;
		/* "JavaWebServer/1.1.1" sends "bytes: x-y/z", contrary to the HTTP spec. */
		if (*hdr == ':')
			++hdr;
		while(isspace(*hdr))
			++hdr;
		if (!*hdr)
			return false;
	}

	if (!isdigit(*hdr))
		return false;
	for (num = 0; isdigit(*hdr); hdr++)
		num = 10 * num + (*hdr - '0');

	if (*hdr != '-' || !isdigit(*(hdr + 1)))
		return false;
	*first_byte_ptr = num;
	++hdr;
	for (num = 0; isdigit(*hdr); hdr++)
		num = 10 * num + (*hdr - '0');
	if (*hdr != '/')
		return false;
	*last_byte_ptr = num;
	if (!(isdigit (*(hdr + 1)) || *(hdr + 1) == '*'))
		return false;
	if (*last_byte_ptr < *first_byte_ptr)
		return false;
	++hdr;
	if (*hdr == '*')
		num = -1;
	else
		for (num = 0; isdigit(*hdr); hdr++)
			num = 10 * num + (*hdr - '0');
		*entity_length_ptr = num;
	if ((*entity_length_ptr <= *last_byte_ptr) && *entity_length_ptr != -1)
		return false;
	return true;
}


/* Read the body of the request, but don't store it anywhere and don't
   display a progress gauge.  This is useful for reading the bodies of
   administrative responses to which we will soon issue another
   request.  The response is not useful to the user, but reading it
   allows us to continue using the same connection to the server.

   If reading fails, false is returned, true otherwise.  In debug
   mode, the body is displayed for debugging purposes.  */

static bool
skip_short_body (int fd, wgint contlen, bool chunked)
{
  enum {
    SKIP_SIZE = 512,                /* size of the download buffer */
    SKIP_THRESHOLD = 4096        /* the largest size we read */
  };
  wgint remaining_chunk_size = 0;
  char dlbuf[SKIP_SIZE + 1];
  dlbuf[SKIP_SIZE] = '\0';        /* so DEBUGP can safely print it */

  /* If the body is too large, it makes more sense to simply close the
     connection than to try to read the body.  */
  if (contlen > SKIP_THRESHOLD)
    return false;

  while (contlen > 0 || chunked)
    {
      int ret;
      if (chunked)
        {
          if (remaining_chunk_size == 0)
            {
              char *line = fd_read_line (fd);
              char *endl;
              if (line == NULL)
                break;

              remaining_chunk_size = strtol (line, &endl, 16);
              xfree (line);

              if (remaining_chunk_size < 0)
                return false;

              if (remaining_chunk_size == 0)
                {
                  line = fd_read_line (fd);
                  xfree (line);
                  break;
                }
            }

          contlen = MIN (remaining_chunk_size, SKIP_SIZE);
        }

      DEBUGP (("Skipping %s bytes of body: [", number_to_static_string (contlen)));

      ret = fd_read (fd, dlbuf, MIN (contlen, SKIP_SIZE), -1);
      if (ret <= 0)
        {
          /* Don't normally report the error since this is an
             optimization that should be invisible to the user.  */
          DEBUGP (("] aborting (%s).\n",
                   ret < 0 ? fd_errstr (fd) : "EOF received"));
          return false;
        }
      contlen -= ret;

      if (chunked)
        {
          remaining_chunk_size -= ret;
          if (remaining_chunk_size == 0)
            {
              char *line = fd_read_line (fd);
              if (line == NULL)
                return false;
              else
                xfree (line);
            }
        }

      /* Safe even if %.*s bogusly expects terminating \0 because
         we've zero-terminated dlbuf above.  */
      DEBUGP (("%.*s", ret, dlbuf));
    }

  DEBUGP (("] done.\n"));
  return true;
}

#define NOT_RFC2231 0
#define RFC2231_NOENCODING 1
#define RFC2231_ENCODING 2

/* extract_param extracts the parameter name into NAME.
   However, if the parameter name is in RFC2231 format then
   this function adjusts NAME by stripping of the trailing
   characters that are not part of the name but are present to
   indicate the presence of encoding information in the value
   or a fragment of a long parameter value
*/
static int
modify_param_name (param_token *name)
{
  const char *delim1 = memchr (name->b, '*', name->e - name->b);
  const char *delim2 = memrchr (name->b, '*', name->e - name->b);

  int result;

  if (delim1 == NULL)
    {
      result = NOT_RFC2231;
    }
  else if (delim1 == delim2)
    {
      if ((name->e - 1) == delim1)
        {
          result = RFC2231_ENCODING;
        }
      else
        {
          result = RFC2231_NOENCODING;
        }
      name->e = delim1;
    }
  else
    {
      name->e = delim1;
      result = RFC2231_ENCODING;
    }
  return result;
}

/* extract_param extract the paramater value into VALUE.
   Like modify_param_name this function modifies VALUE by
   stripping off the encoding information from the actual value
*/
static void
modify_param_value (param_token *value, int encoding_type )
{
  if (encoding_type == RFC2231_ENCODING)
    {
      const char *delim = memrchr (value->b, '\'', value->e - value->b);
      if (delim != NULL)
        {
          value->b = (delim+1);
        }
    }
}

/* Extract a parameter from the string (typically an HTTP header) at
   **SOURCE and advance SOURCE to the next parameter.  Return false
   when there are no more parameters to extract.  The name of the
   parameter is returned in NAME, and the value in VALUE.  If the
   parameter has no value, the token's value is zeroed out.

   For example, if *SOURCE points to the string "attachment;
   filename=\"foo bar\"", the first call to this function will return
   the token named "attachment" and no value, and the second call will
   return the token named "filename" and value "foo bar".  The third
   call will return false, indicating no more valid tokens.

   is_url_encoded is an out parameter. If not NULL, a boolean value will be
   stored into it, letting the caller know whether or not the extracted value is
   URL-encoded. The caller can then decode it with url_unescape(), which however
   performs decoding in-place. URL-encoding is used by RFC 2231 to support
   non-US-ASCII characters in HTTP header values.  */

bool
extract_param (const char **source, param_token *name, param_token *value,
               char separator, bool *is_url_encoded)
{
  const char *p = *source;
  int param_type;
  if (is_url_encoded)
    *is_url_encoded = false;   /* initializing the out parameter */

  while (isspace (*p)) ++p;
  if (!*p)
    {
      *source = p;
      return false;             /* no error; nothing more to extract */
    }

  /* Extract name. */
  name->b = p;
  while (*p && !isspace (*p) && *p != '=' && *p != separator) ++p;
  name->e = p;
  if (name->b == name->e)
    return false;               /* empty name: error */
  while (isspace (*p)) ++p;
  if (*p == separator || !*p)           /* no value */
    {
      xzero (*value);
      if (*p == separator) ++p;
      *source = p;
      return true;
    }
  if (*p != '=')
    return false;               /* error */

  /* *p is '=', extract value */
  ++p;
  while (isspace (*p)) ++p;
  if (*p == '"')                /* quoted */
    {
      value->b = ++p;
      while (*p && *p != '"') ++p;
      if (!*p)
        return false;
      value->e = p++;
      /* Currently at closing quotes; find the end of param. */
      while (isspace (*p)) ++p;
      while (*p && *p != separator) ++p;
      if (*p == separator)
        ++p;
      else if (*p)
        /* garbage after closed quotes, e.g. foo="bar"baz */
        return false;
    }
  else                          /* unquoted */
    {
      value->b = p;
      while (*p && *p != separator) ++p;
      value->e = p;
      while (value->e != value->b && isspace (value->e[-1]))
        --value->e;
      if (*p == separator) ++p;
    }
  *source = p;

  param_type = modify_param_name (name);
  if (param_type != NOT_RFC2231)
    {
      if (param_type == RFC2231_ENCODING && is_url_encoded)
        *is_url_encoded = true;
      modify_param_value (value, param_type);
    }
  return true;
}

#undef NOT_RFC2231
#undef RFC2231_NOENCODING
#undef RFC2231_ENCODING

/* Appends the string represented by VALUE to FILENAME */

static void
append_value_to_filename (char **filename, param_token const * const value,
                          bool is_url_encoded)
{
  int original_length = strlen (*filename);
  int new_length = strlen (*filename) + (value->e - value->b);
  *filename = xrealloc (*filename, new_length+1);
  memcpy (*filename + original_length, value->b, (value->e - value->b));
  (*filename)[new_length] = '\0';
  if (is_url_encoded)
    url_unescape (*filename + original_length);
}

/* Parse the contents of the `Content-Disposition' header, extracting
   the information useful to Wget.  Content-Disposition is a header
   borrowed from MIME; when used in HTTP, it typically serves for
   specifying the desired file name of the resource.  For example:

       Content-Disposition: attachment; filename="flora.jpg"

   Wget will skip the tokens it doesn't care about, such as
   "attachment" in the previous example; it will also skip other
   unrecognized params.  If the header is syntactically correct and
   contains a file name, a copy of the file name is stored in
   *filename and true is returned.  Otherwise, the function returns
   false.

   The file name is stripped of directory components and must not be
   empty.

   Historically, this function returned filename prefixed with opt.dir_prefix,
   now that logic is handled by the caller, new code should pay attention,
   changed by crq, Sep 2010.

*/
static bool
parse_content_disposition (const char *hdr, char **filename)
{
  param_token name, value;
  bool is_url_encoded = false;

  char *encodedFilename = NULL;
  char *unencodedFilename = NULL;
  for ( ; extract_param (&hdr, &name, &value, ';', &is_url_encoded);
        is_url_encoded = false)
    {
      int isFilename = BOUNDED_EQUAL_NO_CASE (name.b, name.e, "filename");
      if ( isFilename && value.b != NULL)
        {
          /* Make the file name begin at the last slash or backslash. */
          bool isEncodedFilename;
          char **outFilename;
          const char *last_slash = memrchr (value.b, '/', value.e - value.b);
          const char *last_bs = memrchr (value.b, '\\', value.e - value.b);
          if (last_slash && last_bs)
            value.b = 1 + MAX (last_slash, last_bs);
          else if (last_slash || last_bs)
            value.b = 1 + (last_slash ? last_slash : last_bs);
          if (value.b == value.e)
            continue;

          /* Check if the name is "filename*" as specified in RFC 6266.
           * Since "filename" could be broken up as "filename*N" (RFC 2231),
           * a check is needed to make sure this is not the case */
          isEncodedFilename = *name.e == '*' && !isdigit (*(name.e + 1));
          outFilename = isEncodedFilename ? &encodedFilename
            : &unencodedFilename;
          if (*outFilename)
            append_value_to_filename (outFilename, &value, is_url_encoded);
          else
            {
              *outFilename = strdupdelim (value.b, value.e);
              if (is_url_encoded)
                url_unescape (*outFilename);
            }
        }
    }
  if (encodedFilename)
    {
      xfree (unencodedFilename);
      *filename = encodedFilename;
    }
  else
    {
      xfree (encodedFilename);
      *filename = unencodedFilename;
    }
  if (*filename)
    return true;
  else
    return false;
}

/* Persistent connections.  Currently, we cache the most recently used
   connection as persistent, provided that the HTTP server agrees to
   make it such.  The persistence data is stored in the variables
   below.  Ideally, it should be possible to cache an arbitrary fixed
   number of these connections.  */

/* Whether a persistent connection is active. */
static bool pconn_active;

static struct {
  /* The socket of the connection.  */
  int socket;

  /* Host and port of the currently active persistent connection. */
  char *host;
  int port;

  /* Whether a ssl handshake has occoured on this connection.  */
  bool ssl;

  /* Whether the connection was authorized.  This is only done by
     NTLM, which authorizes *connections* rather than individual
     requests.  (That practice is peculiar for HTTP, but it is a
     useful optimization.)  */
  bool authorized;

#ifdef ENABLE_NTLM
  /* NTLM data of the current connection.  */
  struct ntlmdata ntlm;
#endif
} pconn;

/* Mark the persistent connection as invalid and free the resources it
   uses.  This is used by the CLOSE_* macros after they forcefully
   close a registered persistent connection.  */

static void invalidate_persistent (void)
{
	log_info("Disabling further reuse of socket %d.\n", pconn.socket);
	pconn_active = false;
	fd_close(pconn.socket);
	xfree(pconn.host);
	xzero(pconn);
}

/* Register FD, which should be a TCP/IP connection to HOST:PORT, as
   persistent.  This will enable someone to use the same connection
   later.  In the context of HTTP, this must be called only AFTER the
   response has been received and the server has promised that the
   connection will remain alive.

   If a previous connection was persistent, it is closed. */

static void
register_persistent (const char *host, int port, int fd, bool ssl)
{
  if (pconn_active)
    {
      if (pconn.socket == fd)
        {
          /* The connection FD is already registered. */
          return;
        }
      else
        {
          /* The old persistent connection is still active; close it
             first.  This situation arises whenever a persistent
             connection exists, but we then connect to a different
             host, and try to register a persistent connection to that
             one.  */
          invalidate_persistent ();
        }
    }

  pconn_active = true;
  pconn.socket = fd;
  pconn.host = xstrdup (host);
  pconn.port = port;
  pconn.ssl = ssl;
  pconn.authorized = false;

  DEBUGP (("Registered socket %d for persistent reuse.\n", fd));
}

/* Return true if a persistent connection is available for connecting to HOST:PORT.
 */
static bool persistent_available_p(const char *host, int port, bool ssl, bool *host_lookup_failed)
{
	func_enter();
	log_info("host: %s", host);
	log_info("port: %d", port);
	log_info("ssl: %d", ssl);
	/* First, check whether a persistent connection is active at all.  */
	if (!pconn_active) {
		func_exit();
		return false;
	}

	/* If we want SSL and the last connection wasn't or vice versa,
		don't use it.  Checking for host and port is not enough because
		HTTP and HTTPS can apparently coexist on the same port.  */
	if (ssl != pconn.ssl) {
		func_exit();
		return false;
	}

	/* If we're not connecting to the same port, we're not interested. */
	if (port != pconn.port) {
		func_exit();
		return false;
	}

  	/* If the host is the same, we're in business.  If not, there is
       still hope -- read below.  */
	if (0 != strcasecmp(host, pconn.host)) {
		/* Check if pconn.socket is talking to HOST under another name.
			This happens often when both sites are virtual hosts
			distinguished only by name and served by the same network
			interface, and hence the same web server (possibly set up by
			the ISP and serving many different web sites).  This
			admittedly unconventional optimization does not contradict
			HTTP and works well with popular server software.  */

		bool found;
		ip_address ip;
		struct address_list *al;

		if (ssl) {
			/* Don't try to talk to two different SSL sites over the same
				secure connection!  (Besides, it's not clear that
				name-based virtual hosting is even possible with SSL.)  */
			func_exit();
			return false;
		}

		/* If pconn.socket's peer is one of the IP addresses HOST
			resolves to, pconn.socket is for all intents and purposes
			already talking to HOST.  */
		if (!socket_ip_address(pconn.socket, &ip, ENDPOINT_PEER)) {
			/* Can't get the peer's address -- something must be very
				wrong with the connection.  */
			invalidate_persistent();
			func_exit();
			return false;
		}

		al = lookup_host(host, 0);
		if (!al) {
			*host_lookup_failed = true;
			func_exit();
			return false;
		}

		found = address_list_contains (al, &ip);
		address_list_release (al);

		if (!found) {
			func_exit();
			return false;
		}

		/* The persistent connection's peer address was found among the
			addresses HOST resolved to; therefore, pconn.sock is in fact
			already talking to HOST -- no need to reconnect.  */
	}

	/* Finally, check whether the connection is still open.  This is
		important because most servers implement liberal (short) timeout
		on persistent connections.  Wget can of course always reconnect
		if the connection doesn't work out, but it's nicer to know in
		advance.  This test is a logical followup of the first test, but
		is "expensive" and therefore placed at the end of the list.

		(Current implementation of test_socket_open has a nice side
		effect that it treats sockets with pending data as "closed".
		This is exactly what we want: if a broken server sends message
		body in response to HEAD, or if it sends more than conent-length
		data, we won't reuse the corrupted connection.)  */

	func_exit();
	return true;
}

/* The idea behind these two CLOSE macros is to distinguish between
   two cases: one when the job we've been doing is finished, and we
   want to close the connection and leave, and two when something is
   seriously wrong and we're closing the connection as part of
   cleanup.

   In case of keep_alive, CLOSE_FINISH should leave the connection
   open, while CLOSE_INVALIDATE should still close it.

   Note that the semantics of the flag `keep_alive' is "this
   connection *will* be reused (the server has promised not to close
   the connection once we're done)", while the semantics of
   `pc_active_p && (fd) == pc_last_fd' is "we're *now* using an
   active, registered connection".  */

#define CLOSE_FINISH(fd) do {                   \
  if (!keep_alive)                              \
    {                                           \
      if (pconn_active && (fd) == pconn.socket) \
        invalidate_persistent ();               \
      else                                      \
          fd_close (fd);                        \
      fd = -1;                                  \
    }                                           \
} while (0)

#define CLOSE_INVALIDATE(fd) do {               \
  if (pconn_active && (fd) == pconn.socket)     \
    invalidate_persistent ();                   \
  else                                          \
    fd_close (fd);                              \
  fd = -1;                                      \
} while (0)

typedef enum
{
  ENC_INVALID = -1,             /* invalid encoding */
  ENC_NONE = 0,                 /* no special encoding */
  ENC_GZIP,                     /* gzip compression */
  ENC_DEFLATE,                  /* deflate compression */
  ENC_COMPRESS,                 /* compress compression */
  ENC_BROTLI                    /* brotli compression */
} encoding_t;

struct http_stat {
	wgint len;                    /* received length */
	wgint contlen;                /* expected length */
	wgint restval;                /* the restart value */
	int res;                      /* the result of last read */
	char *rderrmsg;               /* error message from read error */
	char *newloc;                 /* new location (redirection) */
	char *remote_time;            /* remote time-stamp string */
	char *error;                  /* textual HTTP error */
	int statcode;                 /* status code */
	char *message;                /* status message */
	wgint rd_size;                /* amount of data read from socket */
	double dltime;                /* time it took to download the data */
	const char *referer;          /* value of the referer header. */
	char *local_file;             /* local file name. */
	bool existence_checked;       /* true if we already checked for a file's
	                               existence after having begun to download
	                               (needed in gethttp for when connection is
	                               interrupted/restarted. */
	bool timestamp_checked;       /* true if pre-download time-stamping checks
	                             * have already been performed */
	char *orig_file_name;         /* name of file to compare for time-stamping
	                             * (might be != local_file if -K is set) */
	wgint orig_file_size;         /* size of file to compare for time-stamping */
	time_t orig_file_tstamp;      /* time-stamp of file to compare for
	                             * time-stamping */

	encoding_t local_encoding;    /* the encoding of the local file */
	encoding_t remote_encoding;   /* the encoding of the remote file */
};

static void free_hstat (struct http_stat *hs)
{
	xfree(hs->newloc);
	xfree(hs->remote_time);
	xfree(hs->error);
	xfree(hs->rderrmsg);
	xfree(hs->local_file);
	xfree(hs->orig_file_name);
	xfree(hs->message);
}

static void
get_file_flags (const char *filename, int *dt)
{
  logprintf (LOG_VERBOSE, "File %s already there; not retrieving.\n\n", filename);
  /* If the file is there, we suppose it's retrieved OK.  */
  *dt |= RETROKF;

  /* #### Bogusness alert.  */
  /* If its suffix is "html" or "htm" or similar, assume text/html.  */
  if (has_html_suffix_p (filename))
    *dt |= TEXTHTML;
}

/* Download the response body from the socket and writes it to
   an output file.  The headers have already been read from the
   socket. fp is a pointer to the output file.

   url, warc_timestamp_str, warc_request_uuid, warc_ip, type
   and statcode will be saved in the headers of the WARC record.
   The head parameter contains the HTTP headers of the response.

   Returns the error code.   */
static int read_response_body(struct http_stat *hs, int sock, FILE *fp, wgint contlen)
{
	int flags = 0;

	/* Read the response body.  */
	if (contlen != -1)
		/** If content-length is present, read that much; otherwise, read
			until EOF.  The HTTP spec doesn't require the server to
			actually close the connection when it's done sending data. */
		flags |= rb_read_exactly;

	hs->len = hs->restval;
 	hs->rd_size = 0;
	/* Download the response body and write it to fp.*/
	hs->res = fd_read_body(sock, fp, (contlen != -1 ? contlen : 0),
			hs->restval, &hs->rd_size, &hs->len, &hs->dltime, flags);
	if (hs->res >= 0) {
		return RETRFINISHED;
	}

	if (hs->res == -2) {
		/* Error while writing to fd. */
		return FWRITEERR;
	} else {
		/* A read error! */
		hs->rderrmsg = xstrdup(fd_errstr (sock));
		return RETRFINISHED;
	}
}

#define BEGINS_WITH(line, string_constant)                               \
  (!strncasecmp (line, string_constant, sizeof (string_constant) - 1)    \
   && (isspace (line[sizeof (string_constant) - 1])                      \
       || !line[sizeof (string_constant) - 1]))

#define SET_USER_AGENT(req) do {                                         \
	request_set_header (req, "User-Agent",                           \
	                aprintf ("Wget/%s (%s)",                         \
	                "Wget 1.19.5", "Linux"),                        \
	                rel_value);                                      \
} while (0)

/*
   Convert time_t to one of valid HTTP date formats
   ie. rfc1123-date.

   HTTP-date    = rfc1123-date | rfc850-date | asctime-date
   rfc1123-date = wkday "," SP date1 SP time SP "GMT"
   rfc850-date  = weekday "," SP date2 SP time SP "GMT"
   asctime-date = wkday SP date3 SP time SP 4DIGIT
   date1        = 2DIGIT SP month SP 4DIGIT
                  ; day month year (e.g., 02 Jun 1982)
   date2        = 2DIGIT "-" month "-" 2DIGIT
                  ; day-month-year (e.g., 02-Jun-82)
   date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
                  ; month day (e.g., Jun  2)
   time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
                  ; 00:00:00 - 23:59:59
   wkday        = "Mon" | "Tue" | "Wed"
                | "Thu" | "Fri" | "Sat" | "Sun"
   weekday      = "Monday" | "Tuesday" | "Wednesday"
                | "Thursday" | "Friday" | "Saturday" | "Sunday"
   month        = "Jan" | "Feb" | "Mar" | "Apr"
                | "May" | "Jun" | "Jul" | "Aug"
                | "Sep" | "Oct" | "Nov" | "Dec"

   source: RFC2616  */
static uerr_t
time_to_rfc1123 (time_t time, char *buf, size_t bufsize)
{
  static const char *wkday[] = { "Sun", "Mon", "Tue", "Wed",
                                 "Thu", "Fri", "Sat" };
  static const char *month[] = { "Jan", "Feb", "Mar", "Apr",
                                 "May", "Jun", "Jul", "Aug",
                                 "Sep", "Oct", "Nov", "Dec" };
  /* rfc1123 example: Thu, 01 Jan 1998 22:12:57 GMT  */
  static const char *time_format = "%s, %02d %s %04d %02d:%02d:%02d GMT";

  struct tm *gtm = gmtime (&time);
  if (!gtm)
    {
      logprintf (LOG_NOTQUIET,
                 ("gmtime failed. This is probably a bug.\n"));
      return TIMECONV_ERR;
    }

  snprintf (buf, bufsize, time_format, wkday[gtm->tm_wday],
            gtm->tm_mday, month[gtm->tm_mon],
            gtm->tm_year + 1900, gtm->tm_hour,
            gtm->tm_min, gtm->tm_sec);

  return RETROK;
}

static struct request *initialize_request(const struct url *u, struct http_stat *hs, int *dt, struct url *proxy,
                    bool inhibit_keep_alive, bool *basic_auth_finished, wgint *body_data_size, char **user, char **passwd, uerr_t *ret)
{
	bool head_only = !!(*dt & HEAD_ONLY);
	struct request *req;
	func_enter();
	printf("head_only: %d\n", head_only);

	/* Prepare the request to send. */
	{
		char *meth_arg;
		const char *meth = "GET";
		if (head_only)
			meth = "HEAD";
		else if (opt.method)
			meth = opt.method;

		/* Use the full path, i.e. one that includes the leading slash and
		the query string.  E.g. if u->path is "foo/bar" and u->query is
		"param=value", full_path will be "/foo/bar?param=value".  */
		meth_arg = url_full_path(u);
		printf("meth: %s\n", meth);
		printf("meth_arg: %s\n", meth_arg);
		req = request_new(meth, meth_arg);
	}

	request_set_header(req, "Referer", (char *) hs->referer, rel_none);
	if (*dt & SEND_NOCACHE) {
		/* Cache-Control MUST be obeyed by all HTTP/1.1 caching mechanisms... */
		// Cache-Control: no-cache
		request_set_header(req, "Cache-Control", "no-cache", rel_none);

		/* ... but some HTTP/1.0 caches doesn't implement Cache-Control.  */
		// Pragma: no-cache
		request_set_header(req, "Pragma", "no-cache", rel_none);
	}

	if (*dt & IF_MODIFIED_SINCE) {
		char strtime[32];
		uerr_t err = time_to_rfc1123(hs->orig_file_tstamp, strtime, countof (strtime));
		if (err != RETROK) {
			logputs (LOG_VERBOSE, "Cannot convert timestamp to http format. "
						"Falling back to time 0 as last modification time.\n");
			strcpy (strtime, "Thu, 01 Jan 1970 00:00:00 GMT");
		}
		request_set_header(req, "If-Modified-Since", xstrdup (strtime), rel_value);
	}

	printf("restval: %ld\n", hs->restval);
	if (hs->restval)
    	request_set_header(req, "Range", aprintf("bytes=%s-",
                                 number_to_static_string (hs->restval)), rel_value);

	SET_USER_AGENT(req);
	request_set_header(req, "Accept", "*/*", rel_none);
	request_set_header(req, "Accept-Encoding", "identity", rel_none);

	/* Find the username/password with priority */
    *user = NULL;
	*passwd = NULL;

	/* Generate the Host header, HOST:PORT.  Take into account that:
	 - Broken server-side software often doesn't recognize the PORT
	   argument, so we must generate "Host: www.server.com" instead of
	   "Host: www.server.com:80" (and likewise for https port).
	 - IPv6 addresses contain ":", so "Host: 3ffe:8100:200:2::2:1234"
	   becomes ambiguous and needs to be rewritten as "Host: [3ffe:8100:200:2::2]:1234".  */
    /* Formats arranged for hfmt[add_port][add_squares].  */
    static const char *hfmt[][2] = {
      	{ "%s", "[%s]" }, { "%s:%d", "[%s]:%d" }
    };
    int add_port = u->port != scheme_default_port(u->scheme);
    int add_squares = strchr(u->host, ':') != NULL;
    request_set_header(req, "Host", aprintf(hfmt[add_port][add_squares], u->host, u->port), rel_value);

	if (inhibit_keep_alive)
		request_set_header(req, "Connection", "Close", rel_none);
	else {
		request_set_header (req, "Connection", "Keep-Alive", rel_none);
		if (proxy)
			request_set_header (req, "Proxy-Connection", "Keep-Alive", rel_none);
	}

	func_exit();
	return req;
}

static void
initialize_proxy_configuration (const struct url *u, struct request *req,
                                struct url *proxy, char **proxyauth)
{
  char *proxy_user, *proxy_passwd;
  /* For normal username and password, URL components override
     command-line/wgetrc parameters.  With proxy
     authentication, it's the reverse, because proxy URLs are
     normally the "permanent" ones, so command-line args
     should take precedence.  */
  if (opt.proxy_user && opt.proxy_passwd)
    {
      proxy_user = opt.proxy_user;
      proxy_passwd = opt.proxy_passwd;
    }
  else
    {
      proxy_user = proxy->user;
      proxy_passwd = proxy->passwd;
    }
  /* #### This does not appear right.  Can't the proxy request,
     say, `Digest' authentication?  */
  if (proxy_user && proxy_passwd)
    *proxyauth = basic_authentication_encode (proxy_user, proxy_passwd);

  /* Proxy authorization over SSL is handled below. */
#ifdef HAVE_SSL
  if (u->scheme != SCHEME_HTTPS)
#endif
    request_set_header (req, "Proxy-Authorization", *proxyauth, rel_value);
}

static uerr_t establish_connection(const struct url *u, const struct url **conn_ref,
                      struct http_stat *hs, struct url *proxy,
                      char **proxyauth,
                      struct request **req_ref, bool *using_ssl,
                      bool inhibit_keep_alive,
                      int *sock_ref)
{
	bool host_lookup_failed = false;
	int sock = *sock_ref;
	struct request *req = *req_ref;
	const struct url *conn = *conn_ref;
	struct response *resp;
	int write_error;
	int statcode;

	if (!inhibit_keep_alive) {
		/* Look for a persistent connection to target host, unless a
		    proxy is used.  The exception is when SSL is in use, in which
		    case the proxy is nothing but a passthrough to the target
		    host, registered as a connection to the latter.  */
		const struct url *relevant = conn;
#ifdef HAVE_SSL
		if (u->scheme == SCHEME_HTTPS)
        	relevant = u;
#endif

		if (persistent_available_p(relevant->host, relevant->port,
#ifdef HAVE_SSL
				relevant->scheme == SCHEME_HTTPS,
#else
				0,
#endif
				&host_lookup_failed)) {
			socket_family(pconn.socket, ENDPOINT_PEER);
			sock = pconn.socket;
			*using_ssl = pconn.ssl;
			logprintf (LOG_VERBOSE, "Reusing existing connection to %s:%d.\n", pconn.host, pconn.port);
			DEBUGP(("Reusing fd %d.\n", sock));
			if (pconn.authorized)
				/* If the connection is already authorized, the "Basic"
					authorization added by code above is unnecessary and
					only hurts us.  */
				request_remove_header (req, "Authorization");
		} else if (host_lookup_failed) {
			logprintf(LOG_NOTQUIET, "%s: unable to resolve host address %s\n", exec_name, relevant->host);
          	return HOSTERR;
        } else if (sock != -1) {
			sock = -1;
        }
    }

	if (sock < 0) {
		sock = connect_to_host(conn->host, conn->port);

	if (sock == E_HOST)
		return HOSTERR;
	else if (sock < 0)
		return (retryable_socket_connect_error(errno) ? CONERROR : CONIMPOSSIBLE);

#ifdef HAVE_SSL
	if (proxy && u->scheme == SCHEME_HTTPS) {
		char *head;
		char *message;
		/* When requesting SSL URLs through proxies, use the
			CONNECT method to request passthrough.  */
		struct request *connreq = request_new("CONNECT", aprintf ("%s:%d", u->host, u->port));
		SET_USER_AGENT(connreq);
		if (proxyauth) {
			request_set_header(connreq, "Proxy-Authorization", *proxyauth, rel_value);
			/* Now that PROXYAUTH is part of the CONNECT request,
				zero it out so we don't send proxy authorization with
				the regular request below.  */
			*proxyauth = NULL;
		}

		printf("||||||||||request_send||||||||||\n");
		request_set_header(connreq, "Host", aprintf("%s:%d", u->host, u->port), rel_value);
		write_error = request_send(connreq, sock);
		request_free (&connreq);
		if (write_error < 0) {
			CLOSE_INVALIDATE (sock);
			return WRITEFAILED;
		}

		head = read_http_response_head(sock);
		if (!head) {
			logprintf(LOG_VERBOSE, "Failed reading proxy response: %s\n", fd_errstr (sock));
			CLOSE_INVALIDATE (sock);
			return HERR;
		}

		message = NULL;
		if (!*head) {
			xfree (head);
			goto failed_tunnel;
		}

		logprintf(LOG_VERBOSE, "proxy responded with: [%s]\n", head);
		resp = resp_new(head);
		statcode = resp_status(resp, &message);
		if (statcode < 0) {
			char *tms = datetime_str(time (NULL));
			logprintf(LOG_VERBOSE, "%d\n", statcode);
			logprintf(LOG_NOTQUIET, "%s ERROR %d: %s.\n", tms, statcode, "Malformed status line");
			xfree (head);
			return HERR;
		}

		xfree(hs->message);
		hs->message = xstrdup(message);
		resp_free (&resp);
		xfree (head);
		if (statcode != 200) {
	failed_tunnel:
			logprintf (LOG_NOTQUIET, "Proxy tunneling failed: %s", message ? message : "?");
			xfree (message);
			return CONSSLERR;
		}
		xfree (message);

		/* SOCK is now *really* connected to u->host, so update CONN
			to reflect this.  That way register_persistent will
			register SOCK as being connected to u->host:u->port.  */
		conn = u;
	}

	if (conn->scheme == SCHEME_HTTPS) {
		if (!ssl_connect_wget(sock, u->host, NULL)) {
			CLOSE_INVALIDATE (sock);
			return CONSSLERR;
		} else if (!ssl_check_certificate (sock, u->host)) {
			CLOSE_INVALIDATE (sock);
			return VERIFCERTERR;
		}
		*using_ssl = true;
	}
#endif /* HAVE_SSL */
	}

	*conn_ref = conn;
	*req_ref = req;
	*sock_ref = sock;
	return RETROK;
}

static uerr_t
set_file_timestamp (struct http_stat *hs)
{
  bool local_dot_orig_file_exists = false;
  char *local_filename = NULL;
  struct stat st;

  if (!local_dot_orig_file_exists)
    /* Couldn't stat() <file>.orig, so try to stat() <file>. */
    if (stat (hs->local_file, &st) == 0)
      local_filename = hs->local_file;

  if (local_filename != NULL)
    /* There was a local file, so we'll check later to see if the version
        the server has is the same version we already have, allowing us to
        skip a download. */
    {
      hs->orig_file_name = xstrdup (local_filename);
      hs->orig_file_size = st.st_size;
      hs->orig_file_tstamp = st.st_mtime;
      hs->timestamp_checked = true;
    }

  return RETROK;
}

static uerr_t
check_file_output (const struct url *u, struct http_stat *hs,
                   struct response *resp, char *hdrval, size_t hdrsize)
{
  /* Determine the local filename if needed. Notice that if -O is used
   * hstat.local_file is set by http_loop to the argument of -O. */
  if (!hs->local_file)
    {
      char *local_file = NULL;

      /* Honor Content-Disposition whether possible. */
      if (!opt.content_disposition
          || !resp_header_copy (resp, "Content-Disposition",
                                hdrval, hdrsize)
          || !parse_content_disposition (hdrval, &local_file))
        {
          /* The Content-Disposition header is missing or broken.
           * Choose unique file name according to given URL. */
          hs->local_file = url_file_name (u);
        }
      else
        {
          DEBUGP (("Parsed filename from Content-Disposition: %s\n",
                  local_file));
          hs->local_file = url_file_name (u);
        }

      xfree (local_file);
    }

  /* TODO: perform this check only once. */
  if (!hs->existence_checked && file_exists_p (hs->local_file, NULL))
    {
      if (!ALLOW_CLOBBER)
        {
          char *unique = unique_name (hs->local_file, true);
          if (unique != hs->local_file)
            xfree (hs->local_file);
          hs->local_file = unique;
        }
    }
  hs->existence_checked = true;

  /* Support timestamping */
  if (opt.timestamping && !hs->timestamp_checked)
    {
      uerr_t timestamp_err = set_file_timestamp (hs);
      if (timestamp_err != RETROK)
        return timestamp_err;
    }
  return RETROK;
}

static uerr_t
check_auth (const struct url *u, char *user, char *passwd, struct response *resp,
            struct request *req, bool *retry,
            bool *basic_auth_finished_ref, bool *auth_finished_ref)
{
  uerr_t auth_err = RETROK;
  bool basic_auth_finished = *basic_auth_finished_ref;
  bool auth_finished = *auth_finished_ref;
  *retry = false;
  if (!auth_finished && (user && passwd))
    {
      /* IIS sends multiple copies of WWW-Authenticate, one with
         the value "negotiate", and other(s) with data.  Loop over
         all the occurrences and pick the one we recognize.  */
      int wapos;
      char *buf;
      const char *www_authenticate = NULL;
      const char *wabeg, *waend;
      const char *digest = NULL, *basic = NULL, *ntlm = NULL;
      for (wapos = 0; !ntlm
             && (wapos = resp_header_locate (resp, "WWW-Authenticate", wapos,
                                             &wabeg, &waend)) != -1;
           ++wapos)
        {
          param_token name, value;

          BOUNDED_TO_ALLOCA (wabeg, waend, buf);
          www_authenticate = buf;

          for (;!ntlm;)
            {
              /* extract the auth-scheme */
              while (isspace (*www_authenticate)) www_authenticate++;
              name.e = name.b = www_authenticate;
              while (*name.e && !isspace (*name.e)) name.e++;

              if (name.b == name.e)
                break;

              DEBUGP (("Auth scheme found '%.*s'\n", (int) (name.e - name.b), name.b));

              if (known_authentication_scheme_p (name.b, name.e))
                {
                  if (!digest && BEGINS_WITH (name.b, "Digest"))
                    digest = name.b;
                  else if (!basic && BEGINS_WITH (name.b, "Basic"))
                    basic = name.b;
                }

              /* now advance over the auth-params */
              www_authenticate = name.e;
              DEBUGP (("Auth param list '%s'\n", www_authenticate));
              while (extract_param (&www_authenticate, &name, &value, ',', NULL) && name.b && value.b)
                {
                  DEBUGP (("Auth param %.*s=%.*s\n",
                           (int) (name.e - name.b), name.b, (int) (value.e - value.b), value.b));
                }
            }
        }

      if (!basic && !digest && !ntlm)
        {
          /* If the authentication header is missing or
             unrecognized, there's no sense in retrying.  */
          logputs (LOG_NOTQUIET, ("Unknown authentication scheme.\n"));
        }
      else if (!basic_auth_finished
               || !basic)
        {
          char *pth = url_full_path (u);
          const char *value;
          uerr_t *auth_stat;
          auth_stat = xmalloc (sizeof (uerr_t));
          *auth_stat = RETROK;

          if (ntlm)
            www_authenticate = ntlm;
          else if (digest)
            www_authenticate = digest;
          else
            www_authenticate = basic;

          logprintf (LOG_NOTQUIET, ("Authentication selected: %s\n"), www_authenticate);

          value =  create_authorization_line (www_authenticate,
                                              user, passwd,
                                              request_method (req),
                                              pth,
                                              &auth_finished,
                                              auth_stat);

          auth_err = *auth_stat;
          if (auth_err == RETROK)
            {
              request_set_header (req, "Authorization", value, rel_value);

              if (!u->user && BEGINS_WITH (www_authenticate, "Basic"))
                {
                }

              xfree (pth);
              xfree (auth_stat);
              *retry = true;
              goto cleanup;
            }
          else
            {
              /* Creating the Authorization header went wrong */
            }
        }
      else
        {
          /* We already did Basic auth, and it failed. Gotta
           * give up. */
        }
    }

 cleanup:
  *basic_auth_finished_ref = basic_auth_finished;
  *auth_finished_ref = auth_finished;
  return auth_err;
}

static uerr_t open_output_stream (struct http_stat *hs, int count, FILE **fp)
{
	/* Open the local file.  */
	if (1) {
		if (file_exists_p(hs->local_file, NULL)) {
			if (unlink (hs->local_file) < 0) {
				log_error("%s unlink error: %s\n", hs->local_file, strerror(errno));
				return UNLINKERR;
			}
		}

		*fp = fopen(hs->local_file, "wb");
	} else {
		*fp = fopen_excl(hs->local_file, true);
		if (!*fp && errno == EEXIST) {
			/* We cannot just invent a new name and use it (which is
			   what functions like unique_create typically do)
			   because we told the user we'd use this name.
			   Instead, return and retry the download.  */
			log_error("%s has sprung into existence.\n", hs->local_file);
			return FOPEN_EXCL_ERR;
		}
	}

	if (!*fp) {
		log_error("%s: %s\n", hs->local_file, strerror (errno));
		return FOPENERR;
	}

	/* Print fetch message, if opt.verbose.  */
	log_info("Saving to: %s\n", HYPHENP(hs->local_file) ? "STDOUT" : hs->local_file);

	return RETROK;
}

/* Set proper type flags based on type string.  */
static void set_content_type (int *dt, const char *type)
{
	/* If content-type is not given, assume text/html.  This is because
		of the multitude of broken CGI's that "forget" to generate the
		content-type.  */
	if (!type ||
			0 == strcasecmp (type, TEXTHTML_S) ||
			0 == strcasecmp (type, TEXTXHTML_S))
		*dt |= TEXTHTML;
	else
		*dt &= ~TEXTHTML;

	if (type &&	0 == strcasecmp (type, TEXTCSS_S))
		*dt |= TEXTCSS;
	else
		*dt &= ~TEXTCSS;
}

/* Retrieve a document through HTTP protocol.  It recognizes status
   code, and correctly handles redirections.  It closes the network
   socket.  If it receives an error from the functions below it, it
   will print it if there is enough information to do so (almost
   always), returning the error to the caller (i.e. http_loop).

   Various HTTP parameters are stored to hs.

   If PROXY is non-NULL, the connection will be made to the proxy
   server, and u->url will be requested.  */
static uerr_t gethttp(const struct url *u, struct url *original_url, struct http_stat *hs, int *dt, int count)
{
	struct request *req = NULL;

	char *type = NULL;
	char *user, *passwd;
	char *proxyauth;
	int statcode;
	int write_error;
	wgint contlen, contrange;
	const struct url *conn;
	FILE *fp;
	int err;
	uerr_t retval;

	int sock = -1;

	/* Set to 1 when the authorization has already been sent and should
	 	not be tried again. */
	bool auth_finished = false;

	/* Set to 1 when just globally-set Basic authorization has been sent;
	* should prevent further Basic negotiations, but not other
	* mechanisms. */
	bool basic_auth_finished = false;

	/* Whether our connection to the remote host is through SSL.  */
	bool using_ssl = false;

	/* Whether a HEAD request will be issued (as opposed to GET or POST). */
	bool head_only = !!(*dt & HEAD_ONLY);

	/* Whether conditional get request will be issued.  */
	bool cond_get = !!(*dt & IF_MODIFIED_SINCE);

	char *head = NULL;
	struct response *resp = NULL;
	char hdrval[512];
	char *message = NULL;

	/* Whether this connection will be kept alive after the HTTP request is done. */
	bool keep_alive;

	/* Is the server using the chunked transfer encoding?  */
	bool chunked_transfer_encoding = false;

	/* Whether keep-alive should be inhibited.  */
	bool inhibit_keep_alive = !opt.http_keep_alive || opt.ignore_length;

	/* Headers sent when using POST. */
	wgint body_data_size = 0;

	func_enter();
#ifdef HAVE_SSL
  	if (u->scheme == SCHEME_HTTPS) {
		/* Initialize the SSL context.
		 After this has once been done, it becomes a no-op.  */
		if (!ssl_init ()){
		  	scheme_disable (SCHEME_HTTPS);
		  	logprintf (LOG_NOTQUIET, "Disabling SSL due to encountered errors.\n");
		  	retval = SSLINITFAILED;
		  	goto cleanup;
		}
    }
#endif /* HAVE_SSL */

	/* Initialize certain elements of struct http_stat. */
	hs->len = 0;
	hs->contlen = -1;
	hs->res = -1;
	hs->rderrmsg = NULL;
	hs->newloc = NULL;
	xfree (hs->remote_time);
	hs->error = NULL;
	hs->message = NULL;
	hs->local_encoding = ENC_NONE;
	hs->remote_encoding = ENC_NONE;

  	conn = u;

	/* init HTTP request */ 
    uerr_t ret;
    req = initialize_request(u, hs, dt, NULL, inhibit_keep_alive,
			&basic_auth_finished, &body_data_size, &user, &passwd, &ret);
	if (req == NULL) {
		retval = ret;
		goto cleanup;
	}

retry_with_auth:
	/* We need to come back here when the initial attempt to retrieve
	   without authorization header fails.  (Expected to happen at least
	   for the Digest authorization scheme.)
	 */
	proxyauth = NULL;
	keep_alive = true;
	/* Establish the connection.  */
	if (inhibit_keep_alive)
		keep_alive = false;

    uerr_t conn_err = establish_connection(u, &conn, hs, NULL, &proxyauth, &req, &using_ssl, inhibit_keep_alive, &sock);
	if (conn_err != RETROK) {
		retval = conn_err;
		goto cleanup;
	}

	/* Send the request to server.  */
	write_error = request_send(req, sock);
	if (write_error < 0) {
		CLOSE_INVALIDATE(sock);

		retval = WRITEFAILED;
		goto cleanup;
	}

	log_info("%s request sent, awaiting response... \n", "HTTP");
	contlen = -1;
	contrange = 0;
	*dt &= ~RETROKF;

	/* Repeat while we receive a 10x response code.  */
    bool _repeat;
    do {
		head = read_http_response_head(sock);
		if (!head) {
			if (errno == 0) {
				logputs(LOG_NOTQUIET, "No data received.\n");
				CLOSE_INVALIDATE(sock);
				retval = HEOF;
			} else {
				logprintf(LOG_NOTQUIET, "Read error (%s) in headers.\n", fd_errstr (sock));
				CLOSE_INVALIDATE(sock);
				retval = HERR;
			}
			goto cleanup;
		}

		logprintf(LOG_NOTQUIET, "\n+++response begin+++\n%s---response end---\n", head);
		resp = resp_new(head);
        /* Check for status line.  */
        xfree(message);
        statcode = resp_status(resp, &message);
		debug("statcode: %d", statcode);
		if (statcode < 0) {
			char *tms = datetime_str (time (NULL));
			logprintf (LOG_VERBOSE, "%d\n", statcode);
			logprintf (LOG_NOTQUIET, "%s ERROR %d: %s.\n", tms, statcode, "Malformed status line");
			CLOSE_INVALIDATE (sock);
			retval = HERR;
			goto cleanup;
		}

		if (H_10X (statcode)) {
			xfree (head);
			resp_free (&resp);
			_repeat = true;
			DEBUGP (("Ignoring response\n"));
		} else {
			_repeat = false;
		}
	}while (_repeat);

	xfree (hs->message);
	hs->message = xstrdup (message);
	if (!opt.server_response)
		logprintf (LOG_VERBOSE, "%2d %s\n", statcode, message ? message : "");
	else {
		logprintf (LOG_VERBOSE, "\n");
		print_server_response (resp, "  ");
	}

	if (!opt.ignore_length && resp_header_copy(resp, "Content-Length", hdrval, sizeof (hdrval))) {
		wgint parsed;
		errno = 0;
		parsed = str_to_wgint(hdrval, NULL, 10);
		if (parsed == WGINT_MAX && errno == ERANGE) {
		/* Out of range.
			#### If Content-Length is out of range, it most likely
			means that the file is larger than 2G and that we're
			compiled without LFS.  In that case we should probably
			refuse to even attempt to download the file.  */
			contlen = -1;
		} else if (parsed < 0) {
			/* Negative Content-Length; nonsensical, so we can't
				assume any information about the content to receive. */
			contlen = -1;
		} else
			contlen = parsed;
	}

	/* Check for keep-alive related responses. */
	if (!inhibit_keep_alive) {
		if (resp_header_copy(resp, "Connection", hdrval, sizeof (hdrval))) {
			if (0 == strcasecmp (hdrval, "Close"))
				keep_alive = false;
		}
	}

	chunked_transfer_encoding = false;
	if (resp_header_copy (resp, "Transfer-Encoding", hdrval, sizeof (hdrval)) && 0 == strcasecmp (hdrval, "chunked"))
    	chunked_transfer_encoding = true;

	if (keep_alive)
    	/* The server has promised that it will not close the connection
       	   when we're done.  This means that we can register it.  */
    	register_persistent (conn->host, conn->port, sock, using_ssl);

	if (statcode == HTTP_STATUS_UNAUTHORIZED) {
		/* Authorization is required.  */
		uerr_t auth_err = RETROK;
		bool retry;
		/* Since WARC is disabled, we are not interested in the response body.  */
		if (keep_alive && !head_only && skip_short_body(sock, contlen, chunked_transfer_encoding))
			CLOSE_FINISH(sock);
		else
			CLOSE_INVALIDATE(sock);

		pconn.authorized = false;
		auth_err = check_auth(u, user, passwd, resp, req, &retry, &basic_auth_finished, &auth_finished);
		if (auth_err == RETROK && retry) {
			xfree(hs->message);
			resp_free(&resp);
			xfree(message);
			xfree(head);
			goto retry_with_auth;
		}
		if (auth_err == RETROK)
			retval = AUTHFAILED;
		else
			retval = auth_err;
		goto cleanup;
	}

    ret = check_file_output(u, hs, resp, hdrval, sizeof(hdrval));
    if (ret != RETROK) {
        retval = ret;
        goto cleanup;
    }

	hs->statcode = statcode;
	if (statcode == -1)
		hs->error = xstrdup("Malformed status line");
	else if (!*message)
		hs->error = xstrdup("(no description)");
	else
		hs->error = xstrdup(message);

	type = resp_header_strdup(resp, "Content-Type");
	if (type) {
		char *tmp = strchr(type, ';');
		if (tmp) {
			while (tmp > type && isspace(tmp[-1]))
				--tmp;
			*tmp = '\0';
		}
	}
  
	hs->newloc = resp_header_strdup(resp, "Location");
	hs->remote_time = resp_header_strdup (resp, "Last-Modified");
	if (!hs->remote_time) // now look for the Wayback Machine's timestamp
		hs->remote_time = resp_header_strdup (resp, "X-Archive-Orig-last-modified");

	// "Content-Range:  bytes  0-800/801"
	if (resp_header_copy (resp, "Content-Range", hdrval, sizeof (hdrval))) {
		wgint first_byte_pos, last_byte_pos, entity_length;
		if (parse_content_range(hdrval, &first_byte_pos, &last_byte_pos, &entity_length)) {
			contrange = first_byte_pos;
			contlen = last_byte_pos - first_byte_pos + 1;
		}
	}

	if (resp_header_copy(resp, "Content-Encoding", hdrval, sizeof (hdrval))) {
		hs->local_encoding = ENC_INVALID;

		switch (hdrval[0]) {
		case 'b': case 'B':
			if (0 == strcasecmp(hdrval, "br"))
			hs->local_encoding = ENC_BROTLI;
			break;
		case 'c': case 'C':
			if (0 == strcasecmp(hdrval, "compress"))
				hs->local_encoding = ENC_COMPRESS;
			break;
		case 'd': case 'D':
			if (0 == strcasecmp(hdrval, "deflate"))
				hs->local_encoding = ENC_DEFLATE;
			break;
		case 'g': case 'G':
			if (0 == strcasecmp(hdrval, "gzip"))
				hs->local_encoding = ENC_GZIP;
			break;
		case 'i': case 'I':
			if (0 == strcasecmp(hdrval, "identity"))
				hs->local_encoding = ENC_NONE;
			break;
		case 'x': case 'X':
			if (0 == strcasecmp(hdrval, "x-compress"))
				hs->local_encoding = ENC_COMPRESS;
			else if (0 == strcasecmp(hdrval, "x-gzip"))
				hs->local_encoding = ENC_GZIP;
			break;
		case '\0':
			hs->local_encoding = ENC_NONE;
		}

		if (hs->local_encoding == ENC_INVALID) {
			log_warn("Unrecognized Content-Encoding: %s\n", hdrval);
			hs->local_encoding = ENC_NONE;
		}
	}

	/* 20x responses are counted among successful by default.  */
	/* 2xx:   成功—表示请求已经被成功接收、理解、接受。*/
	if (H_20X (statcode))
		*dt |= RETROKF;

	if (statcode == HTTP_STATUS_NO_CONTENT) {
		/* 204 response has no body (RFC 2616, 4.3) */
		/* In case the caller cares to look...  */
		hs->len = 0;
		hs->res = 0;
		hs->restval = 0;

		CLOSE_FINISH(sock);
		retval = RETRFINISHED;
		goto cleanup;
	}

	/* Return if redirected.  */
	if (H_REDIRECTED (statcode) || statcode == HTTP_STATUS_MULTIPLE_CHOICES) {
		/* RFC2068 says that in case of the 300 (multiple choices)
			response, the server can output a preferred URL through
			`Location' header; otherwise, the request should be treated
			like GET.  So, if the location is set, it will be a
			redirection; otherwise, just proceed normally.  */
		if (statcode == HTTP_STATUS_MULTIPLE_CHOICES && !hs->newloc)
			*dt |= RETROKF;
		else {
			logprintf (LOG_VERBOSE, "Location: %s%s\n",	hs->newloc ? escnonprint_uri (hs->newloc) : ("unspecified"), hs->newloc ? (" [following]") : "");

			/* In case the caller cares to look...  */
			hs->len = 0;
			hs->res = 0;
			hs->restval = 0;

			/* Since WARC is disabled, we are not interested in the response body.  */
			if (keep_alive && !head_only && skip_short_body (sock, contlen, chunked_transfer_encoding))
				CLOSE_FINISH (sock);
			else
				CLOSE_INVALIDATE (sock);


			/* From RFC2616: The status codes 303 and 307 have
				been added for servers that wish to make unambiguously
				clear which kind of reaction is expected of the client.

				A 307 should be redirected using the same method,
				in other words, a POST should be preserved and not
				converted to a GET in that case.

				With strict adherence to RFC2616, POST requests are not
				converted to a GET request on 301 Permanent Redirect
				or 302 Temporary Redirect.

				A switch may be provided later based on the HTTPbis draft
				that allows clients to convert POST requests to GET
				requests on 301 and 302 response codes. */
			switch (statcode) {
			case HTTP_STATUS_TEMPORARY_REDIRECT:
			case HTTP_STATUS_PERMANENT_REDIRECT:
				retval = NEWLOCATION_KEEP_POST;
				goto cleanup;
			case HTTP_STATUS_MOVED_PERMANENTLY:
				if (opt.method && strcasecmp (opt.method, "post") != 0) {
					retval = NEWLOCATION_KEEP_POST;
					goto cleanup;
				}
				break;
			case HTTP_STATUS_MOVED_TEMPORARILY:
				if (opt.method && strcasecmp (opt.method, "post") != 0) {
					retval = NEWLOCATION_KEEP_POST;
					goto cleanup;
				}
				break;
			}

			retval = NEWLOCATION;
			goto cleanup;
		}
	}

	if (cond_get) {
		if (statcode == HTTP_STATUS_NOT_MODIFIED) {
			logprintf (LOG_VERBOSE,	"File %s not modified on server. Omitting download.\n\n", hs->local_file);
			*dt |= RETROKF;
			CLOSE_FINISH (sock);
			retval = RETRUNNEEDED;
			goto cleanup;
		}
	}

	set_content_type(dt, type);
	if (cond_get) {
		/* Handle the case when server ignores If-Modified-Since header.  */
		if (statcode == HTTP_STATUS_OK && hs->remote_time) {
			time_t tmr = http_atotm (hs->remote_time);

			/* Check if the local file is up-to-date based on Last-Modified header
				and content length.  */
			if (tmr != (time_t) - 1 && tmr <= hs->orig_file_tstamp && (contlen == -1 || contlen == hs->orig_file_size)) {
				logprintf (LOG_VERBOSE,	"Server ignored If-Modified-Since header for file %s.\n"
						"You might want to add --no-if-modified-since option.\n\n", hs->local_file);
				*dt |= RETROKF;
				CLOSE_INVALIDATE (sock);
				retval = RETRUNNEEDED;
				goto cleanup;
			}
		}
	}

	if (statcode == HTTP_STATUS_RANGE_NOT_SATISFIABLE && hs->restval < (contlen + contrange)) {
		/* The file was not completely downloaded,
			yet the server claims the range is invalid.	Bail out.  */
		CLOSE_INVALIDATE (sock);
		retval = RANGEERR;
		goto cleanup;
	}

	if (statcode == HTTP_STATUS_RANGE_NOT_SATISFIABLE || (!opt.timestamping && hs->restval > 0 && statcode == HTTP_STATUS_OK
			&& contrange == 0 && contlen >= 0 && hs->restval >= contlen)) {
		/* If `-c' is in use and the file has been fully downloaded (or
			the remote file has shrunk), Wget effectively requests bytes
			after the end of file and the server response with 416
			(or 200 with a <= Content-Length.  */
		logputs (LOG_VERBOSE, ("The file is already fully retrieved; nothing to do.\n\n"));
		/* In case the caller inspects. */
		hs->len = contlen;
		hs->res = 0;
		/* Mark as successfully retrieved. */
		*dt |= RETROKF;

		/* Try to maintain the keep-alive connection. It is often cheaper to
		* consume some bytes which have already been sent than to negotiate
		* a new connection. However, if the body is too large, or we don't
		* care about keep-alive, then simply terminate the connection */
		if (keep_alive && skip_short_body (sock, contlen, chunked_transfer_encoding))
			CLOSE_FINISH (sock);
		else
			CLOSE_INVALIDATE (sock);
		retval = RETRUNNEEDED;
		goto cleanup;
	}


	if ((contrange != 0 && contrange != hs->restval) || (H_PARTIAL (statcode) && !contrange && hs->restval)) {
		/* The Range request was somehow misunderstood by the server. Bail out.  */
		CLOSE_INVALIDATE (sock);
		retval = RANGEERR;
		goto cleanup;
    }

	if (contlen == -1)
		hs->contlen = -1;
	/* If the response is gzipped, the uncompressed size is unknown. */
	else if (hs->remote_encoding == ENC_GZIP)
		hs->contlen = -1;
	else
		hs->contlen = contlen + contrange;

	log_debug("opt.verbose: %d", opt.verbose);
	if (opt.verbose) {
		if (*dt & RETROKF) {
			/* No need to print this output if the body won't be
			   downloaded at all, or if the original server response is
			   printed.  */
			logputs(LOG_VERBOSE, "Length: ");
			if (contlen != -1) {
				logputs(LOG_VERBOSE, number_to_static_string(contlen + contrange));
			if (contlen + contrange >= 1024)
				logprintf(LOG_VERBOSE, " (%s)", human_readable(contlen + contrange, 10, 1));
			if (contrange) {
				if (contlen >= 1024)
					logprintf(LOG_VERBOSE, (", %s (%s) remaining"), number_to_static_string (contlen), human_readable (contlen, 10, 1));
				else
					logprintf (LOG_VERBOSE, (", %s remaining"), number_to_static_string (contlen));
			}
		} else
			logputs (LOG_VERBOSE, opt.ignore_length ? ("ignored") : ("unspecified"));
			if (type)
				logprintf (LOG_VERBOSE, " [%s]\n", type);
			else
				logputs (LOG_VERBOSE, "\n");
		}
	}


	/* Return if we have no intention of further downloading.  */
	if ((!(*dt & RETROKF) && !opt.content_on_error) || head_only) {
		/* In case the caller cares to look...  */
		hs->len = 0;
		hs->res = 0;
		hs->restval = 0;

		/* Since WARC is disabled, we are not interested in the response body.  */
		if (head_only)
			/* Pre-1.10 Wget used CLOSE_INVALIDATE here.  Now we trust the
				servers not to send body in response to a HEAD request, and
				those that do will likely be caught by test_socket_open.
				If not, they can be worked around using
				`--no-http-keep-alive'.  */
			CLOSE_FINISH (sock);
		else if (keep_alive	&& skip_short_body (sock, contlen, chunked_transfer_encoding))
			/* Successfully skipped the body; also keep using the socket. */
			CLOSE_FINISH (sock);
		else
			CLOSE_INVALIDATE (sock);

		if (statcode == HTTP_STATUS_GATEWAY_TIMEOUT)
			retval = GATEWAYTIMEOUT;
		else
			retval = RETRFINISHED;

		goto cleanup;
	}

	err = open_output_stream(hs, count, &fp);
	if (err != RETROK) {
		CLOSE_INVALIDATE (sock);
		retval = err;
		goto cleanup;
	}

	err = read_response_body(hs, sock, fp, contlen);
	if (hs->res >= 0)
		CLOSE_FINISH (sock);
	else
		CLOSE_INVALIDATE (sock);

	retval = err;

cleanup:
	xfree(head);
	xfree(type);
	xfree(message);
	resp_free(&resp);
	request_free(&req);

	func_exit();
	return retval;
}

/* The genuine HTTP loop!  This is the part where the retrieval is
   retried, and retried, and retried, and...  */
uerr_t http_loop(const struct url *u, struct url *original_url)
{
	int count, dt;
	bool got_head = false;         /* used for time-stamping and filename detection */
	bool time_came_from_head = false;
	bool got_name = false;
	char *tms;
	const char *tmrate;
	uerr_t err, ret = TRYLIMEXC;
	time_t tmr = -1;               /* remote time-stamp */
	struct http_stat hstat;        /* HTTP status */
	struct stat st;
	bool send_head_first = true;
	bool force_full_retrieve = false;

	/* Setup hstat struct. */
	xzero(hstat);
	hstat.referer = NULL;

	if (!opt.content_disposition) {
		hstat.local_file = url_file_name(u);
		log_info("local_file： %s", hstat.local_file);
		got_name = hstat.local_file != NULL;
	}

	/* Reset the counter. */
	count = 0;

	/* Reset the document type. */
	dt = 0;

	send_head_first = false;

	/* Send preliminary HEAD request if --content-disposition and -c are used
	   together.  */
	if (opt.content_disposition && opt.always_rest)
		send_head_first = true;

	if (opt.timestamping) {
		/* Use conditional get request if requested
		 * and if timestamp is known at this moment.  */
		if (opt.if_modified_since && !send_head_first && got_name && file_exists_p(hstat.local_file, NULL)) {
			dt |= IF_MODIFIED_SINCE;
			uerr_t timestamp_err = set_file_timestamp(&hstat);
			if (timestamp_err != RETROK)
				return timestamp_err;
		}

		/* Send preliminary HEAD request if -N is given and we have existing
		 * destination file or content disposition is enabled.  */
		else if (opt.content_disposition || file_exists_p (hstat.local_file, NULL))
			send_head_first = true;
	}

	/* THE loop */
	do {
		/* Increment the pass counter.  */
		++count;

		/* Get the current time string.  */
		tms = datetime_str(time(NULL));

		/* Print fetch message, if opt.verbose.  */
		if (opt.verbose) {
			char *hurl = original_url->url;
			if (count > 1) {
				char tmp[256];
				sprintf(tmp, ("(try:%2d)"), count);
				log_warn("--%s--  %s  %s\n", tms, tmp, hurl);
			} else {
				log_info("--%s--  %s\n", tms, hurl);
			}
		}

		if (send_head_first && !got_head)
			dt |= HEAD_ONLY;
		else
			dt &= ~HEAD_ONLY;

		/* Decide whether or not to restart.  */
		if (force_full_retrieve)
			hstat.restval = hstat.len;
		else if (opt.start_pos >= 0)
			hstat.restval = opt.start_pos;
		else if (opt.always_rest && got_name && stat(hstat.local_file, &st) == 0 && S_ISREG(st.st_mode))
			/* When -c is used, continue from on-disk size.  (Can't use
			   hstat.len even if count>1 because we don't want a failed
		       first attempt to clobber existing data.)  */
			hstat.restval = st.st_size;
		else if (count > 1)
			/* otherwise, continue where the previous try left off */
			hstat.restval = hstat.len;
		else
			hstat.restval = 0;

		/* Decide whether to send the no-cache directive.  We send it in two cases:
		   a) we're using a proxy, and we're past our first retrieval.
		   Some proxies are notorious臭名昭著的 for caching incomplete data, so
		   we require a fresh get.
		   b) caching is explicitly inhibited. */
		dt |= SEND_NOCACHE;

		/* Try fetching the document, or at least its head.  */
		err = gethttp(u, original_url, &hstat, &dt, count);

		/* Time?  */
		tms = datetime_str(time(NULL));

		switch (err) {
		case HERR: 		case HEOF: 				case CONSOCKERR:
		case CONERROR: 	case READERR: 			case WRITEFAILED:
		case RANGEERR: 	case FOPEN_EXCL_ERR: 	case GATEWAYTIMEOUT:
			/* Non-fatal errors continue executing the loop, which will
			   bring them to "while" statement at the end, to judge
			   whether the number of tries was exceeded.  */
			log_warn((count == opt.ntry) ? ("Giving up.\n\n") : ("Retrying.\n\n"));
			xfree(hstat.message);
			xfree(hstat.error);
			continue;
		case FWRITEERR: case FOPENERR:
			/* Another fatal error.  */
			log_warn("Cannot write to %s (%s).\n", hstat.local_file, strerror (errno));
		case HOSTERR: 		   case CONIMPOSSIBLE: case PROXERR: case SSLINITFAILED:
		case CONTNOTSUPPORTED: case VERIFCERTERR:  case FILEBADFILE:
		case UNKNOWNATTR:
			/* Fatal errors just return from the function.  */
			ret = err;
			log_error("UNKNOWNATTR.\n");
			goto exit;
		case ATTRMISSING:
			/* A missing attribute in a Header is a fatal Protocol error. */
			log_error("Required attribute missing from Header received.\n");
			ret = err;
			goto exit;
		case AUTHFAILED:
			log_error("Username/Password Authentication Failed.\n");
			ret = err;
			goto exit;
		case CONSSLERR:
			/* Another fatal error.  */
			log_error("Unable to establish SSL connection.\n");
			ret = err;
			goto exit;
		case UNLINKERR:
			/* Another fatal error.  */
			log_error("Cannot unlink %s (%s).\n", hstat.local_file, strerror (errno));
			ret = err;
			goto exit;
		case NEWLOCATION: case NEWLOCATION_KEEP_POST:
			/* Return the new location to the caller.  */
			log_error("ERROR: Redirection (%d) without location.\n", hstat.statcode);
			ret = WRONGCODE;
			goto exit;
		case RETRUNNEEDED:
			/* The file was already fully retrieved. */
			log_error("RETRUNNEEDED.\n");
			ret = RETROK;
			goto exit;
		case RETRFINISHED:
			/* Deal with you later.  */
			break;
		default:
			/* All possibilities should have been exhausted.  */
			log_error("All possibilities should have been exhausted.\n");
			abort();
		}

		if (!(dt & RETROKF)) {
			log_warn("dt & RETROKF");

			/* Fall back to GET if HEAD fails with a 500 or 501 error code. */
			/* 500 Internal Server Error; 501 Not Implemented */
			if (dt & HEAD_ONLY && (hstat.statcode == 500 || hstat.statcode == 501)) {
				got_head = true;
				continue;
			} else {
				log_error("%s ERROR %d: %s.\n", tms, hstat.statcode, hstat.error);
			}

			ret = WRONGCODE;
			goto exit;
		}

		/* Did we get the time-stamp? */
		if (!got_head) {
			got_head = true;    /* no more time-stamping */

			if (opt.timestamping && !hstat.remote_time) {
				log_warn("Last-modified header missing -- time-stamps turned off.\n");
			} else if (hstat.remote_time) {
				/* Convert the date-string into struct tm.  */
				tmr = http_atotm(hstat.remote_time);
				if (tmr == (time_t) (-1))
					log_warn("Last-modified header invalid -- time-stamp ignored.\n");
				if (dt & HEAD_ONLY)
					time_came_from_head = true;
			}

			if (send_head_first) {
				/* The time-stamping section.  */
				if (opt.timestamping) {
					if (hstat.orig_file_name) {
						if (hstat.remote_time && tmr != (time_t) (-1)) {
							/* Now time-stamping can be used validly.
							Time-stamping means that if the sizes of
							the local and remote file match, and local
							file is newer than the remote file, it will
							not be retrieved.  Otherwise, the normal
							download procedure is resumed.  */
							if (hstat.orig_file_tstamp >= tmr) {
								if (hstat.contlen == -1 || hstat.orig_file_size == hstat.contlen) {
									logprintf (LOG_VERBOSE, "Server file no newer than local file %s -- not retrieving.\n\n",
									hstat.orig_file_name);
									ret = RETROK;
									goto exit;
								} else {
									logprintf (LOG_VERBOSE, "The sizes do not match (local %s) -- retrieving.\n",
									number_to_static_string (hstat.orig_file_size));
								}
							} else {
								force_full_retrieve = true;
								logputs (LOG_VERBOSE, "Remote file is newer, retrieving.\n");
							}

							logputs (LOG_VERBOSE, "\n");
						}
					}

					/* free_hstat (&hstat); */
					hstat.timestamp_checked = true;
				}

				got_name = true;
				dt &= ~HEAD_ONLY;
				count = 0;          /* the retrieve count for HEAD is reset */
				xfree (hstat.message);
				xfree (hstat.error);
				continue;
			} /* send_head_first */
		} /* !got_head */

		if (opt.useservertimestamps && (tmr != (time_t) (-1))	&& ((hstat.len == hstat.contlen) ||
				((hstat.res == 0) && (hstat.contlen == -1)))) {
			const char *fl = NULL;
			if (fl) {
				time_t newtmr = -1;
				/* Reparse time header, in case it's changed. */
				if (time_came_from_head && hstat.remote_time && hstat.remote_time[0]) {
					newtmr = http_atotm (hstat.remote_time);
					if (newtmr != (time_t)-1)
					tmr = newtmr;
				}
			}
		}

		/* End of time-stamping section. */
		tmrate = retr_rate(hstat.rd_size, hstat.dltime);
		total_download_time += hstat.dltime;

		if (hstat.len == hstat.contlen) {
			if (dt & RETROKF || opt.content_on_error) {
				bool write_to_stdout = false;

				log_info(write_to_stdout ? "%s (%s) - written to stdout %s[%s/%s]\n" : "%s (%s) - %s saved [%s/%s]\n",
							tms, tmrate, write_to_stdout ? "" : hstat.local_file,
							number_to_static_string(hstat.len),
							number_to_static_string(hstat.contlen));
				log_info("%s URL:%s [%s/%s] -> \"%s\" [%d]\n", tms, u->url,
							number_to_static_string(hstat.len),
							number_to_static_string(hstat.contlen),
							hstat.local_file, count);
			}

			total_downloaded_bytes += hstat.rd_size;
			/* Remember that we downloaded the file for later ".orig" code. */
			if (dt & ADDED_HTML_EXTENSION)
				downloaded_file(FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED, hstat.local_file);
			else
				downloaded_file(FILE_DOWNLOADED_NORMALLY, hstat.local_file);

			ret = RETROK;
			goto exit;
		} else if (hstat.res == 0) { /* No read error */
			if (hstat.contlen == -1) { /* We don't know how much we were supposed
										  to get, so assume we succeeded. */
				if (dt & RETROKF || opt.content_on_error) {
					bool write_to_stdout = false;
					log_info(write_to_stdout ? "%s (%s) - written to stdout %s[%s]\n"	: "%s (%s) - %s saved [%s]\n",
								tms, tmrate, write_to_stdout ? "" : hstat.local_file, number_to_static_string (hstat.len));
					log_info("%s URL:%s [%s] -> \"%s\" [%d]\n", tms, u->url, number_to_static_string (hstat.len), hstat.local_file, count);
				}

				total_downloaded_bytes += hstat.rd_size;
				/* Remember that we downloaded the file for later ".orig" code. */
				if (dt & ADDED_HTML_EXTENSION)
					downloaded_file(FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED, hstat.local_file);
				else
					downloaded_file(FILE_DOWNLOADED_NORMALLY, hstat.local_file);

				ret = RETROK;
				goto exit;
			} else if (hstat.len < hstat.contlen) { /* meaning we lost the connection too soon */
				log_info("%s (%s) - Connection closed at byte %s. ", tms, tmrate, number_to_static_string(hstat.len));
				log_info((count == opt.ntry) ? ("Giving up.\n") : ("Retrying.\n"));
				continue;
			} else if (hstat.len != hstat.restval) {
				/* Getting here would mean reading more data than
				   requested with content-length, which we never do.  */
				abort();
			} else {
				/* Getting here probably means that the content-length was
				 * _less_ than the original, local size. We should probably
				 * truncate or re-read, or something. FIXME */
				ret = RETROK;
				goto exit;
			}
		} else { /* from now on hstat.res can only be -1 */
			if (hstat.contlen == -1) {
				log_info("%s (%s) - Read error at byte %s (%s).",
							tms, tmrate, number_to_static_string(hstat.len), hstat.rderrmsg);
				log_info((count == opt.ntry) ? ("Giving up.\n") : ("Retrying.\n"));
				continue;
			} else { /* hstat.res == -1 and contlen is given */
				log_info("%s (%s) - Read error at byte %s/%s (%s). ",
						tms, tmrate, number_to_static_string(hstat.len),
						number_to_static_string (hstat.contlen), hstat.rderrmsg);
				log_info((count == opt.ntry) ? ("Giving up.\n") : ("Retrying.\n"));
				continue;
			}
		}
		/* not reached */
	} while (!opt.ntry || (count < opt.ntry));

exit:
	free_hstat(&hstat);
	return ret;
}

/* Check whether the result of strptime() indicates success.
   strptime() returns the pointer to how far it got to in the string.
   The processing has been successful if the string is at `GMT' or
   `+X', or at the end of the string.

   In extended regexp parlance, the function returns 1 if P matches
   "^ *(GMT|[+-][0-9]|$)", 0 otherwise.  P being NULL (which strptime
   can return) is considered a failure and 0 is returned.  */
static bool check_end (const char *p)
{
  if (!p)
    return false;
  while (isspace (*p))
    ++p;
  if (!*p
      || (p[0] == 'G' && p[1] == 'M' && p[2] == 'T')
      || ((p[0] == '+' || p[0] == '-') && isdigit (p[1])))
    return true;
  else
    return false;
}

/* Convert the textual specification of time in TIME_STRING to the
   number of seconds since the Epoch.

   TIME_STRING can be in any of the three formats RFC2616 allows the
   HTTP servers to emit -- RFC1123-date, RFC850-date or asctime-date,
   as well as the time format used in the Set-Cookie header.
   Timezones are ignored, and should be GMT.

   Return the computed time_t representation, or -1 if the conversion
   fails.

   This function uses strptime with various string formats for parsing
   TIME_STRING.  This results in a parser that is not as lenient in
   interpreting TIME_STRING as I would like it to be.  Being based on
   strptime, it always allows shortened months, one-digit days, etc.,
   but due to the multitude of formats in which time can be
   represented, an ideal HTTP time parser would be even more
   forgiving.  It should completely ignore things like week days and
   concentrate only on the various forms of representing years,
   months, days, hours, minutes, and seconds.  For example, it would
   be nice if it accepted ISO 8601 out of the box.

   I've investigated free and PD code for this purpose, but none was
   usable.  getdate was big and unwieldy, and had potential copyright
   issues, or so I was informed.  Dr. Marcus Hennecke's atotm(),
   distributed with phttpd, is excellent, but we cannot use it because
   it is not assigned to the FSF.  So I stuck it with strptime.  */
time_t http_atotm (const char *time_string)
{
  /* NOTE: Solaris strptime man page claims that %n and %t match white
     space, but that's not universally available.  Instead, we simply
     use ` ' to mean "skip all WS", which works under all strptime
     implementations I've tested.  */

  static const char *time_formats[] = {
    "%a, %d %b %Y %T",          /* rfc1123: Thu, 29 Jan 1998 22:12:57 */
    "%A, %d-%b-%y %T",          /* rfc850:  Thursday, 29-Jan-98 22:12:57 */
    "%a %b %d %T %Y",           /* asctime: Thu Jan 29 22:12:57 1998 */
    "%a, %d-%b-%Y %T"           /* cookies: Thu, 29-Jan-1998 22:12:57
                                   (used in Set-Cookie, defined in the
                                   Netscape cookie specification.) */
  };
  const char *oldlocale;
  char savedlocale[256];
  size_t i;
  time_t ret = (time_t) -1;

  /* Solaris strptime fails to recognize English month names in
     non-English locales, which we work around by temporarily setting
     locale to C before invoking strptime.  */
  oldlocale = setlocale (LC_TIME, NULL);
  if (oldlocale)
    {
      size_t l = strlen (oldlocale) + 1;
      if (l >= sizeof savedlocale)
        savedlocale[0] = '\0';
      else
        memcpy (savedlocale, oldlocale, l);
    }
  else savedlocale[0] = '\0';

  setlocale (LC_TIME, "C");

  for (i = 0; i < countof (time_formats); i++)
    {
      struct tm t;

      /* Some versions of strptime use the existing contents of struct
         tm to recalculate the date according to format.  Zero it out
         to prevent stack garbage from influencing strptime.  */
      xzero (t);

      if (check_end (strptime (time_string, time_formats[i], &t)))
        {
          ret = timegm (&t);
          break;
        }
    }

  /* Restore the previous locale. */
  if (savedlocale[0])
    setlocale (LC_TIME, savedlocale);

  return ret;
}

/* Authorization support: We support three authorization schemes:

   * `Basic' scheme, consisting of base64-ing USER:PASSWORD string;

   * `Digest' scheme, added by Junio Hamano <junio@twinsun.com>,
   consisting of answering to the server's challenge with the proper
   MD5 digests.

   * `NTLM' ("NT Lan Manager") scheme, based on code written by Daniel
   Stenberg for libcurl.  Like digest, NTLM is based on a
   challenge-response mechanism, but unlike digest, it is non-standard
   (authenticates TCP connections rather than requests), undocumented
   and Microsoft-specific.  */

/* Create the authentication header contents for the `Basic' scheme.
   This is done by encoding the string "USER:PASS" to base64 and
   prepending the string "Basic " in front of it.  */

static char *
basic_authentication_encode (const char *user, const char *passwd)
{
  char *t1, *t2;
  int len1 = strlen (user) + 1 + strlen (passwd);

  t1 = (char *)alloca (len1 + 1);
  sprintf (t1, "%s:%s", user, passwd);

  t2 = (char *)alloca (BASE64_LENGTH (len1) + 1);
  wget_base64_encode (t1, len1, t2);

  return concat_strings ("Basic ", t2, (char *) 0);
}

#define SKIP_WS(x) do {                         \
  while (isspace (*(x)))                        \
    ++(x);                                      \
} while (0)

/* Computing the size of a string literal must take into account that
   value returned by sizeof includes the terminating \0.  */
#define STRSIZE(literal) (sizeof (literal) - 1)

/* Whether chars in [b, e) begin with the literal string provided as
   first argument and are followed by whitespace or terminating \0.
   The comparison is case-insensitive.  */
#define STARTS(literal, b, e)                           \
  ((e > b) \
   && ((size_t) ((e) - (b))) >= STRSIZE (literal)   \
   && 0 == strncasecmp (b, literal, STRSIZE (literal))  \
   && ((size_t) ((e) - (b)) == STRSIZE (literal)          \
       || isspace (b[STRSIZE (literal)])))

static bool
known_authentication_scheme_p (const char *hdrbeg, const char *hdrend)
{
  return STARTS ("Basic", hdrbeg, hdrend)
#ifdef ENABLE_DIGEST
    || STARTS ("Digest", hdrbeg, hdrend)
#endif
#ifdef ENABLE_NTLM
    || STARTS ("NTLM", hdrbeg, hdrend)
#endif
    ;
}

#undef STARTS

/* Create the HTTP authorization request header.  When the
   `WWW-Authenticate' response header is seen, according to the
   authorization scheme specified in that header (`Basic' and `Digest'
   are supported by the current implementation), produce an
   appropriate HTTP authorization request header.  */
static char *
create_authorization_line (const char *au, const char *user,
                           const char *passwd, const char *method,
                           const char *path, bool *finished, uerr_t *auth_err)
{
  /* We are called only with known schemes, so we can dispatch on the
     first letter. */
  switch (toupper (*au))
    {
    case 'B':                   /* Basic */
      *finished = true;
      return basic_authentication_encode (user, passwd);
    default:
      /* We shouldn't get here -- this function should be only called
         with values approved by known_authentication_scheme_p.  */
      abort ();
    }
}

void
http_cleanup (void)
{
  xfree (pconn.host);
}

void ensure_extension (struct http_stat *hs, const char *ext, int *dt)
{
  char *last_period_in_local_filename = strrchr (hs->local_file, '.');
  char shortext[8];
  int len;
  shortext[0] = '\0';
  len = strlen (ext);
  if (len == 5)
    {
      memcpy (shortext, ext, len - 1);
      shortext[len - 1] = '\0';
    }

  if (last_period_in_local_filename == NULL
      || !(0 == strcasecmp (last_period_in_local_filename, shortext)
           || 0 == strcasecmp (last_period_in_local_filename, ext)))
    {
      int local_filename_len = strlen (hs->local_file);
      /* Resize the local file, allowing for ".html" preceded by
         optional ".NUMBER".  */
      hs->local_file = xrealloc (hs->local_file,
                                 local_filename_len + 24 + len);
      strcpy (hs->local_file + local_filename_len, ext);
      /* If clobbering is not allowed and the file, as named,
         exists, tack on ".NUMBER.html" instead. */
      if (!ALLOW_CLOBBER && file_exists_p (hs->local_file, NULL))
        {
          int ext_num = 1;
          do
            sprintf (hs->local_file + local_filename_len,
                     ".%d%s", ext_num++, ext);
          while (file_exists_p (hs->local_file, NULL));
        }
      *dt |= ADDED_HTML_EXTENSION;
    }
}

/*
 * vim: et sts=2 sw=2 cino+={s
 */
