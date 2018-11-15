#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
/* not all systems provide PATH_MAX in limits.h */
#ifndef PATH_MAX
# include <sys/param.h>
# ifndef PATH_MAX
#  define PATH_MAX MAXPATHLEN
# endif
#endif

#include <regex.h>

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif
#include <assert.h>

#include "utils.h"
#include "init.h"
#include "host.h"
#include "recur.h"              /* for INFINITE_RECURSION */
#include "convert.h"            /* for convert_cleanup */
#include "res.h"                /* for res_cleanup */
#include "http.h"               /* for http_cleanup */
#include "retr.h"               /* for output_stream */
#include "ptimer.h"             /* for ptimer_destroy */

/* Reset the variables to default values.  */
void
defaults (void)
{
  char *tmp;

  /* Most of the default values are 0 (and 0.0, NULL, and false).
     Just reset everything, and fill in the non-zero values.  Note
     that initializing pointers to NULL this way is technically
     illegal, but porting Wget to a machine where NULL is not all-zero
     bit pattern will be the least of the implementors' worries.  */
  xzero (opt);
  opt.verbose = -1;
  opt.ntry = 20;
  opt.reclevel = 5;
  opt.add_hostdir = true;
  opt.netrc = true;
  opt.ftp_glob = true;
  opt.htmlify = true;
  opt.http_keep_alive = true;
  opt.use_proxy = true;
  opt.convert_file_only = false;
  tmp = getenv ("no_proxy");
  if (tmp)
    opt.no_proxy = sepstring (tmp);
  opt.prefer_family = prefer_none;
  opt.allow_cache = true;
  opt.if_modified_since = true;

  opt.read_timeout = 900;
  opt.use_robots = true;

  opt.remove_listing = true;

  opt.dot_bytes = 1024;
  opt.dot_spacing = 10;
  opt.dots_in_line = 50;

  opt.dns_cache = true;
  opt.ftp_pasv = true;
  /* 2014-09-07  Darshit Shah  <darnir@gmail.com>
   * opt.retr_symlinks is set to true by default. Creating symbolic links on the
   * local filesystem pose a security threat by malicious FTP Servers that
   * server a specially crafted .listing file akin to this:
   *
   * lrwxrwxrwx   1 root     root           33 Dec 25  2012 JoCxl6d8rFU -> /
   * drwxrwxr-x  15 1024     106          4096 Aug 28 02:02 JoCxl6d8rFU
   *
   * A .listing file in this fashion makes Wget susceptiple to a symlink attack
   * wherein the attacker is able to create arbitrary files, directories and
   * symbolic links on the target system and even set permissions.
   *
   * Hence, by default Wget attempts to retrieve the pointed-to files and does
   * not create the symbolic links locally.
   */
  opt.retr_symlinks = true;

#ifdef HAVE_SSL
  opt.check_cert = CHECK_CERT_ON;
  opt.ftps_resume_ssl = true;
  opt.ftps_fallback_to_ftp = false;
  opt.ftps_implicit = false;
  opt.ftps_clear_data_connection = false;
#endif

  /* The default for file name restriction defaults to the OS type. */
  opt.restrict_files_os = restrict_unix;
  opt.restrict_files_ctrl = true;
  opt.restrict_files_nonascii = false;
  opt.restrict_files_case = restrict_no_case_restriction;

  opt.max_redirect = 20;

  opt.waitretry = 10;
  opt.enable_iri = false;

  opt.locale = NULL;
  opt.encoding_remote = NULL;

  opt.useservertimestamps = true;
  opt.show_all_dns_entries = false;

  opt.warc_maxsize = 0; /* 1024 * 1024 * 1024; */
  opt.warc_compression_enabled = false;
  opt.warc_digests_enabled = true;
  opt.warc_cdx_enabled = false;
  opt.warc_cdx_dedup_filename = NULL;
  opt.warc_tempdir = NULL;
  opt.warc_keep_log = true;

  /* Use a negative value to mark the absence of --start-pos option */
  opt.start_pos = -1;
  opt.show_progress = -1;
  opt.noscroll = false;

  opt.enable_xattr = false;
}

extern struct ptimer *timer;
extern int cleaned_up;

/* Free the memory allocated by global variables.  */
void
cleanup (void)
{
  /* Free external resources, close files, etc. */

  if (cleaned_up++)
    return; /* cleanup() must not be called twice */

  log_close ();

  if (output_stream && output_stream != stderr)
    {
      FILE *fp = output_stream;
      output_stream = NULL;
      if (fclose (fp) == EOF){}
    }

  /* No need to check for error because Wget flushes its output (and
     checks for errors) after any data arrives.  */

  /* We're exiting anyway so there's no real need to call free()
     hundreds of times.  Skipping the frees will make Wget exit
     faster.
   *
     However, when detecting leaks, it's crucial to free() everything
     because then you can find the real leaks, i.e. the allocated
     memory which grows with the size of the program.  */

#if defined DEBUG_MALLOC
  convert_cleanup ();
  res_cleanup ();
  http_cleanup ();
  cleanup_html_url ();
  spider_cleanup ();
  host_cleanup ();
  log_cleanup ();

  xfree (opt.choose_config);
  xfree (opt.lfilename);
  xfree (opt.dir_prefix);
  xfree (opt.input_filename);
  xfree (opt.default_page);
  free_vec (opt.accepts);
  free_vec (opt.rejects);
  free_vec ((char **)opt.excludes);
  free_vec ((char **)opt.includes);
  free_vec (opt.domains);
  free_vec (opt.exclude_domains);
  free_vec (opt.follow_tags);
  free_vec (opt.ignore_tags);
  xfree (opt.progress_type);
  xfree (opt.warc_tempdir);
  xfree (opt.warc_cdx_dedup_filename);
  xfree (opt.ftp_user);
  xfree (opt.ftp_passwd);
  xfree (opt.ftp_proxy);
  xfree (opt.https_proxy);
  xfree (opt.http_proxy);
  free_vec (opt.no_proxy);
  xfree (opt.proxy_user);
  xfree (opt.proxy_passwd);
  xfree (opt.useragent);
  xfree (opt.referer);
  xfree (opt.http_user);
  xfree (opt.http_passwd);
  xfree (opt.dot_style);
  free_vec (opt.warc_user_headers);
# ifdef HAVE_SSL
  xfree (opt.cert_file);
  xfree (opt.private_key);
  xfree (opt.ca_directory);
  xfree (opt.ca_cert);
  xfree (opt.crl_file);
  xfree (opt.pinnedpubkey);
# endif
  xfree (opt.bind_address);
  xfree (opt.cookies_input);
  xfree (opt.cookies_output);
  xfree (opt.user);
  xfree (opt.passwd);
  xfree (opt.base_href);
  xfree (opt.method);
  xfree (opt.post_file_name);
  xfree (opt.post_data);
  xfree (opt.body_data);
  xfree (opt.body_file);
  xfree (opt.rejected_log);
  xfree (opt.use_askpass);
  xfree (opt.retry_on_http_error);

  xfree (opt.encoding_remote);
  xfree (opt.locale);
  xfree (opt.hsts_file);

  xfree (opt.wgetrcfile);
  xfree (exec_name);
  xfree (program_argstring);
  ptimer_destroy (timer); timer = NULL;
  quotearg_free ();

#endif /* DEBUG_MALLOC */
}

