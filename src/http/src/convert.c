#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "convert.h"
#include "url.h"
#include "recur.h"
#include "utils.h"
#include "res.h"

static const char *replace_plain (const char*, int, FILE*, const char *);
static const char *replace_attr (const char *, int, FILE *, const char *);
static const char *replace_attr_refresh_hack (const char *, int, FILE *,
                                              const char *, int);
static char *construct_relative (const char *, const char *);
static char *convert_basename (const char *, const struct urlpos *);

/* Construct and return a link that points from BASEFILE to LINKFILE.
   Both files should be local file names, BASEFILE of the referrering
   file, and LINKFILE of the referred file.

   Examples:

   cr("foo", "bar")         -> "bar"
   cr("A/foo", "A/bar")     -> "bar"
   cr("A/foo", "A/B/bar")   -> "B/bar"
   cr("A/X/foo", "A/Y/bar") -> "../Y/bar"
   cr("X/", "Y/bar")        -> "../Y/bar" (trailing slash does matter in BASE)

   Both files should be absolute or relative, otherwise strange
   results might ensue.  The function makes no special efforts to
   handle "." and ".." in links, so make sure they're not there
   (e.g. using path_simplify).  */

static char *
construct_relative (const char *basefile, const char *linkfile)
{
  char *link;
  int basedirs;
  const char *b, *l;
  int i, start;

  /* First, skip the initial directory components common to both
     files.  */
  start = 0;
  for (b = basefile, l = linkfile; *b == *l && *b != '\0'; ++b, ++l)
    {
      if (*b == '/')
        start = (b - basefile) + 1;
    }
  basefile += start;
  linkfile += start;

  /* With common directories out of the way, the situation we have is
     as follows:
         b - b1/b2/[...]/bfile
         l - l1/l2/[...]/lfile

     The link we're constructing needs to be:
       lnk - ../../l1/l2/[...]/lfile

     Where the number of ".."'s equals the number of bN directory
     components in B.  */

  /* Count the directory components in B. */
  basedirs = 0;
  for (b = basefile; *b; b++)
    {
      if (*b == '/')
        ++basedirs;
    }

  if (!basedirs && (b = strpbrk (linkfile, "/:")) && *b == ':')
    {
      link = xmalloc (2 + strlen (linkfile) + 1);
      memcpy (link, "./", 2);
      strcpy (link + 2, linkfile);
    }
  else
    {
      /* Construct LINK as explained above. */
      link = xmalloc (3 * basedirs + strlen (linkfile) + 1);
      for (i = 0; i < basedirs; i++)
        memcpy (link + 3 * i, "../", 3);
      strcpy (link + 3 * i, linkfile);
    }

  return link;
}

/* Construct and return a "transparent proxy" URL
   reflecting changes made by --adjust-extension to the file component
   (i.e., "basename") of the original URL, but leaving the "dirname"
   of the URL (protocol://hostname... portion) untouched.

   Think: populating a squid cache via a recursive wget scrape, where
   changing URLs to work locally with "file://..." is NOT desirable.

   Example:

   if
                     p = "//foo.com/bar.cgi?xyz"
   and
      link->local_name = "docroot/foo.com/bar.cgi?xyz.css"
   then

      new_construct_func(p, link);
   will return
      "//foo.com/bar.cgi?xyz.css"

   Essentially, we do s/$(basename orig_url)/$(basename link->local_name)/
*/
static char *
convert_basename (const char *p, const struct urlpos *link)
{
  int len = link->size;
  char *url = NULL;
  char *org_basename = NULL, *local_basename = NULL;
  char *result = NULL;

  if (*p == '"' || *p == '\'')
    {
      len -= 2;
      p++;
    }

  url = xstrndup (p, len);

  org_basename = strrchr (url, '/');
  if (org_basename)
    org_basename++;
  else
    org_basename = url;

  local_basename = strrchr (link->local_name, '/');
  if (local_basename)
    local_basename++;
  else
    local_basename = url;

  /*
   * If the basenames differ, graft the adjusted basename (local_basename)
   * onto the original URL.
   */
  if (strcmp (org_basename, local_basename) == 0)
    result = url;
  else
    {
      result = uri_merge (url, local_basename);
      xfree (url);
    }

  return result;
}

static bool find_fragment (const char *, int, const char **, const char **);

/* Replace a string with NEW_TEXT.  Ignore quoting. */
static const char *
replace_plain (const char *p, int size, FILE *fp, const char *new_text)
{
  fputs (new_text, fp);
  p += size;
  return p;
}

/* Replace an attribute's original text with NEW_TEXT. */

static const char *
replace_attr (const char *p, int size, FILE *fp, const char *new_text)
{
  bool quote_flag = false;
  char quote_char = '\"';       /* use "..." for quoting, unless the
                                   original value is quoted, in which
                                   case reuse its quoting char. */
  const char *frag_beg, *frag_end;

  /* Structure of our string is:
       "...old-contents..."
       <---    size    --->  (with quotes)
     OR:
       ...old-contents...
       <---    size   -->    (no quotes)   */

  if (*p == '\"' || *p == '\'')
    {
      quote_char = *p;
      quote_flag = true;
      ++p;
      size -= 2;                /* disregard opening and closing quotes */
    }
  putc (quote_char, fp);
  fputs (new_text, fp);

  /* Look for fragment identifier, if any. */
  if (find_fragment (p, size, &frag_beg, &frag_end))
    fwrite (frag_beg, 1, frag_end - frag_beg, fp);
  p += size;
  if (quote_flag)
    ++p;
  putc (quote_char, fp);

  return p;
}

/* The same as REPLACE_ATTR, but used when replacing
   <meta http-equiv=refresh content="new_text"> because we need to
   append "timeout_value; URL=" before the next_text.  */

static const char *
replace_attr_refresh_hack (const char *p, int size, FILE *fp,
                           const char *new_text, int timeout)
{
  /* "0; URL=..." */
  char *new_with_timeout = (char *)alloca (numdigit (timeout)
                                           + 6 /* "; URL=" */
                                           + strlen (new_text)
                                           + 1);
  sprintf (new_with_timeout, "%d; URL=%s", timeout, new_text);

  return replace_attr (p, size, fp, new_with_timeout);
}

/* Find the first occurrence of '#' in [BEG, BEG+SIZE) that is not
   preceded by '&'.  If the character is not found, return zero.  If
   the character is found, return true and set BP and EP to point to
   the beginning and end of the region.

   This is used for finding the fragment indentifiers in URLs.  */

static bool
find_fragment (const char *beg, int size, const char **bp, const char **ep)
{
  const char *end = beg + size;
  bool saw_amp = false;
  for (; beg < end; beg++)
    {
      switch (*beg)
        {
        case '&':
          saw_amp = true;
          break;
        case '#':
          if (!saw_amp)
            {
              *bp = beg;
              *ep = end;
              return true;
            }
          /* fallthrough */
        default:
          saw_amp = false;
        }
    }
  return false;
}

/* Return true if S1 and S2 are the same, except for "/index.html".
   The three cases in which it returns one are (substitute any
   substring for "foo"):

   m("foo/index.html", "foo/")  ==> 1
   m("foo/", "foo/index.html")  ==> 1
   m("foo", "foo/index.html")   ==> 1
   m("foo", "foo/"              ==> 1
   m("foo", "foo")              ==> 1  */

static bool
match_except_index (const char *s1, const char *s2)
{
  int i;
  const char *lng;

  /* Skip common substring. */
  for (i = 0; *s1 && *s2 && *s1 == *s2; s1++, s2++, i++)
    ;
  if (i == 0)
    /* Strings differ at the very beginning -- bail out.  We need to
       check this explicitly to avoid `lng - 1' reading outside the
       array.  */
    return false;

  if (!*s1 && !*s2)
    /* Both strings hit EOF -- strings are equal. */
    return true;
  else if (*s1 && *s2)
    /* Strings are randomly different, e.g. "/foo/bar" and "/foo/qux". */
    return false;
  else if (*s1)
    /* S1 is the longer one. */
    lng = s1;
  else
    /* S2 is the longer one. */
    lng = s2;

  /* foo            */            /* foo/           */
  /* foo/index.html */  /* or */  /* foo/index.html */
  /*    ^           */            /*     ^          */

  if (*lng != '/')
    /* The right-hand case. */
    --lng;

  if (*lng == '/' && *(lng + 1) == '\0')
    /* foo  */
    /* foo/ */
    return true;

  return 0 == strcmp (lng, "/index.html");
}

static int
dissociate_urls_from_file_mapper (void *key, void *value, void *arg)
{
  char *mapping_url = (char *)key;
  char *mapping_file = (char *)value;
  char *file = (char *)arg;

  if (0 == strcmp (mapping_file, file))
    {
      xfree (mapping_url);
      xfree (mapping_file);
    }

  /* Continue mapping. */
  return 0;
}

/* Book-keeping code for downloaded files that enables extension
   hacks.  */

/* This table should really be merged with dl_file_url_map and
   downloaded_html_files.  This was originally a list, but I changed
   it to a hash table because it was actually taking a lot of time to
   find things in it.  */

/* We're storing "modes" of type downloaded_file_t in the hash table.
   However, our hash tables only accept pointers for keys and values.
   So when we need a pointer, we use the address of a
   downloaded_file_t variable of static storage.  */

static downloaded_file_t *downloaded_mode_to_ptr (downloaded_file_t mode)
{
	static downloaded_file_t
	v1 = FILE_NOT_ALREADY_DOWNLOADED,
	v2 = FILE_DOWNLOADED_NORMALLY,
	v3 = FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED,
	v4 = CHECK_FOR_FILE;

	switch (mode) {
	case FILE_NOT_ALREADY_DOWNLOADED:
		return &v1;
	case FILE_DOWNLOADED_NORMALLY:
		return &v2;
	case FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED:
		return &v3;
	case CHECK_FOR_FILE:
		return &v4;
	}
	return NULL;
}

/* Remembers which files have been downloaded.  In the standard case,
   should be called with mode == FILE_DOWNLOADED_NORMALLY for each
   file we actually download successfully (i.e. not for ones we have
   failures on or that we skip due to -N).

   When we've downloaded a file and tacked on a ".html" extension due
   to -E, call this function with
   FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED rather than
   FILE_DOWNLOADED_NORMALLY.

   If you just want to check if a file has been previously added
   without adding it, call with mode == CHECK_FOR_FILE.  Please be
   sure to call this function with local filenames, not remote
   URLs.  */

downloaded_file_t downloaded_file (downloaded_file_t mode, const char *file)
{
	return FILE_DOWNLOADED_NORMALLY;
}

