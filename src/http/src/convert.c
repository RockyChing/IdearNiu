/* Conversion of links to local files.
   Copyright (C) 2003-2011, 2014-2015, 2018 Free Software Foundation,
   Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

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
#include "hash.h"
#include "ptimer.h"
#include "res.h"

static struct hash_table *dl_file_url_map;
struct hash_table *dl_url_file_map;

/* Set of HTML/CSS files downloaded in this Wget run, used for link
   conversion after Wget is done.  */
struct hash_table *downloaded_html_set;
struct hash_table *downloaded_css_set;

static void write_backup_file (const char *, downloaded_file_t);
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

/* Used by write_backup_file to remember which files have been
   written. */
static struct hash_table *converted_files;

static void
write_backup_file (const char *file, downloaded_file_t downloaded_file_return)
{
  /* Rather than just writing over the original .html file with the
     converted version, save the former to *.orig.  Note we only do
     this for files we've _successfully_ downloaded, so we don't
     clobber .orig files sitting around from previous invocations.
     On VMS, use "_orig" instead of ".orig".  See "wget.h". */

  /* Construct the backup filename as the original name plus ".orig". */
  size_t         filename_len = strlen (file);
  char*          filename_plus_orig_suffix;

  /* TODO: hack this to work with css files */
  if (downloaded_file_return == FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED)
    {
      /* Just write "orig" over "html".  We need to do it this way
         because when we're checking to see if we've downloaded the
         file before (to see if we can skip downloading it), we don't
         know if it's a text/html file.  Therefore we don't know yet
         at that stage that -E is going to cause us to tack on
         ".html", so we need to compare vs. the original URL plus
         ".orig", not the original URL plus ".html.orig". */
      filename_plus_orig_suffix = alloca (filename_len + 1);
      strcpy (filename_plus_orig_suffix, file);
      strcpy ((filename_plus_orig_suffix + filename_len) - 4, "orig");
    }
  else /* downloaded_file_return == FILE_DOWNLOADED_NORMALLY */
    {
      /* Append ".orig" to the name. */
      filename_plus_orig_suffix = alloca (filename_len + sizeof (ORIG_SFX));
      strcpy (filename_plus_orig_suffix, file);
      strcpy (filename_plus_orig_suffix + filename_len, ORIG_SFX);
    }

  if (!converted_files)
    converted_files = make_string_hash_table (0);

  /* We can get called twice on the same URL thanks to the
     each time in such a case, it'll end up containing the first-pass
     conversion, not the original file.  So, see if we've already been
     called on this file. */
  if (!string_set_contains (converted_files, file))
    {
      /* Rename <file> to <file>.orig before former gets written over. */
      if (rename (file, filename_plus_orig_suffix) != 0)
        logprintf (LOG_NOTQUIET, ("Cannot back up %s as %s: %s\n"),
                   file, filename_plus_orig_suffix, strerror (errno));

      /* Remember that we've already written a .orig backup for this file.
         Note that we never free this memory since we need it till the
         program does before terminating.  BTW, I'm not sure if it would be
         safe to just set 'converted_file_ptr->string' to 'file' below,
         rather than making a copy of the string...  Another note is that I
         thought I could just add a field to the urlpos structure saying
         that we'd written a .orig file for this URL, but that didn't work,
         so I had to make this separate list.
         -- Dan Harkless <wget@harkless.org>

         This [adding a field to the urlpos structure] didn't work
         the end of the retrieval with a freshly built new urlpos
         list.
         -- Hrvoje Niksic <hniksic@xemacs.org>
      */
      string_set_add (converted_files, file);
    }
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

/* Book-keeping code for dl_file_url_map, dl_url_file_map,
   downloaded_html_list, and downloaded_html_set.  Other code calls
   these functions to let us know that a file has been downloaded.  */

#define ENSURE_TABLES_EXIST do {                        \
  if (!dl_file_url_map)                                 \
    dl_file_url_map = make_string_hash_table (0);       \
  if (!dl_url_file_map)                                 \
    dl_url_file_map = make_string_hash_table (0);       \
} while (0)

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
      hash_table_remove (dl_url_file_map, mapping_url);
      xfree (mapping_url);
      xfree (mapping_file);
    }

  /* Continue mapping. */
  return 0;
}

/* Remove all associations from various URLs to FILE from dl_url_file_map. */

static void
dissociate_urls_from_file (const char *file)
{
  /* Can't use hash_table_iter_* because the table mutates while mapping.  */
  hash_table_for_each (dl_url_file_map, dissociate_urls_from_file_mapper,
                       (char *) file);
}

/* Register that URL has been successfully downloaded to FILE.  This
   is used by the link conversion code to convert references to URLs
   to references to local files.  It is also being used to check if a
   URL has already been downloaded.  */

void
register_download (const char *url, const char *file)
{
  char *old_file, *old_url;

  ENSURE_TABLES_EXIST;

  /* With some forms of retrieval, it is possible, although not likely
     or particularly desirable.  If both are downloaded, the second
     download will override the first one.  When that happens,
     dissociate the old file name from the URL.  */

  if (hash_table_get_pair (dl_file_url_map, file, &old_file, &old_url))
    {
      if (0 == strcmp (url, old_url))
        /* We have somehow managed to download the same URL twice.
           Nothing to do.  */
        return;

      if (match_except_index (url, old_url)
          && !hash_table_contains (dl_url_file_map, url))
        /* The two URLs differ only in the "index.html" ending.  For
           example, one is "http://www.server.com/", and the other is
           "http://www.server.com/index.html".  Don't remove the old
           one, just add the new one as a non-canonical entry.  */
        goto url_only;

      hash_table_remove (dl_file_url_map, file);
      xfree (old_file);
      xfree (old_url);

      /* Remove all the URLs that point to this file.  Yes, there can
         be more than one such URL, because we store redirections as
         multiple entries in dl_url_file_map.  For example, if URL1
         redirects to URL2 which gets downloaded to FILE, we map both
         URL1 and URL2 to FILE in dl_url_file_map.  (dl_file_url_map
         only points to URL2.)  When another URL gets loaded to FILE,
         we want both URL1 and URL2 dissociated from it.

         This is a relatively expensive operation because it performs
         a linear search of the whole hash table, but it should be
         called very rarely, only when two URLs resolve to the same
         file name, *and* the "<file>.1" extensions are turned off.
         In other words, almost never.  */
      dissociate_urls_from_file (file);
    }

  hash_table_put (dl_file_url_map, xstrdup (file), xstrdup (url));

 url_only:
  /* A URL->FILE mapping is not possible without a FILE->URL mapping.
     If the latter were present, it should have been removed by the
     above `if'.  So we could write:

         assert (!hash_table_contains (dl_url_file_map, url));

     The above is correct when running in recursive mode where the
     same URL always resolves to the same file.  But if you do
     something like:

         wget URL URL

     then the first URL will resolve to "FILE", and the other to
     "FILE.1".  In that case, FILE.1 will not be found in
     dl_file_url_map, but URL will still point to FILE in
     dl_url_file_map.  */
  if (hash_table_get_pair (dl_url_file_map, url, &old_url, &old_file))
    {
      hash_table_remove (dl_url_file_map, url);
      xfree (old_url);
      xfree (old_file);
    }

  hash_table_put (dl_url_file_map, xstrdup (url), xstrdup (file));
}

/* Register that FROM has been redirected to "TO".  This assumes that TO
   is successfully downloaded and already registered using
   register_download() above.  */

void
register_redirection (const char *from, const char *to)
{
  char *file;

  ENSURE_TABLES_EXIST;

  file = hash_table_get (dl_url_file_map, to);
  assert (file != NULL);
  if (!hash_table_contains (dl_url_file_map, from))
    hash_table_put (dl_url_file_map, xstrdup (from), xstrdup (file));
}

/* Register that the file has been deleted. */

void
register_delete_file (const char *file)
{
  char *old_url, *old_file;

  ENSURE_TABLES_EXIST;

  if (!hash_table_get_pair (dl_file_url_map, file, &old_file, &old_url))
    return;

  hash_table_remove (dl_file_url_map, file);
  xfree (old_file);
  xfree (old_url);
  dissociate_urls_from_file (file);
}

static void downloaded_files_free (void);

/* Cleanup the data structures associated with this file.  */

void
convert_cleanup (void)
{
  if (dl_file_url_map)
    {
      free_keys_and_values (dl_file_url_map);
      hash_table_destroy (dl_file_url_map);
      dl_file_url_map = NULL;
    }
  if (dl_url_file_map)
    {
      free_keys_and_values (dl_url_file_map);
      hash_table_destroy (dl_url_file_map);
      dl_url_file_map = NULL;
    }
  if (downloaded_html_set)
    string_set_free (downloaded_html_set);
  downloaded_files_free ();
  if (converted_files)
    string_set_free (converted_files);
}

/* Book-keeping code for downloaded files that enables extension
   hacks.  */

/* This table should really be merged with dl_file_url_map and
   downloaded_html_files.  This was originally a list, but I changed
   it to a hash table because it was actually taking a lot of time to
   find things in it.  */

static struct hash_table *downloaded_files_hash;

/* We're storing "modes" of type downloaded_file_t in the hash table.
   However, our hash tables only accept pointers for keys and values.
   So when we need a pointer, we use the address of a
   downloaded_file_t variable of static storage.  */

static downloaded_file_t *
downloaded_mode_to_ptr (downloaded_file_t mode)
{
  static downloaded_file_t
    v1 = FILE_NOT_ALREADY_DOWNLOADED,
    v2 = FILE_DOWNLOADED_NORMALLY,
    v3 = FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED,
    v4 = CHECK_FOR_FILE;

  switch (mode)
    {
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

downloaded_file_t
downloaded_file (downloaded_file_t mode, const char *file)
{
  downloaded_file_t *ptr;

  if (mode == CHECK_FOR_FILE)
    {
      if (!downloaded_files_hash)
        return FILE_NOT_ALREADY_DOWNLOADED;
      ptr = hash_table_get (downloaded_files_hash, file);
      if (!ptr)
        return FILE_NOT_ALREADY_DOWNLOADED;
      return *ptr;
    }

  if (!downloaded_files_hash)
    downloaded_files_hash = make_string_hash_table (0);

  ptr = hash_table_get (downloaded_files_hash, file);
  if (ptr)
    return *ptr;

  ptr = downloaded_mode_to_ptr (mode);
  hash_table_put (downloaded_files_hash, xstrdup (file), ptr);

  return FILE_NOT_ALREADY_DOWNLOADED;
}

static void
downloaded_files_free (void)
{
  if (downloaded_files_hash)
    {
      hash_table_iterator iter;
      for (hash_table_iterate (downloaded_files_hash, &iter);
           hash_table_iter_next (&iter);
           )
        xfree (iter.key);
      hash_table_destroy (downloaded_files_hash);
      downloaded_files_hash = NULL;
    }
}

