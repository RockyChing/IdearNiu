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
void defaults(void)
{
  /* Most of the default values are 0 (and 0.0, NULL, and false).
     Just reset everything, and fill in the non-zero values.  Note
     that initializing pointers to NULL this way is technically
     illegal, but porting Wget to a machine where NULL is not all-zero
     bit pattern will be the least of the implementors' worries.  */
  xzero (opt);
  opt.verbose = -1;
  opt.ntry = 20;
  opt.http_keep_alive = true;
  opt.if_modified_since = true;

  opt.read_timeout = 900;

  opt.dot_bytes = 1024;
  opt.dot_spacing = 10;
  opt.dots_in_line = 50;

#ifdef HAVE_SSL
  opt.check_cert = CHECK_CERT_ON;
#endif

  /* The default for file name restriction defaults to the OS type. */
  opt.restrict_files_os = restrict_unix;
  opt.restrict_files_ctrl = true;
  opt.restrict_files_nonascii = false;
  opt.restrict_files_case = restrict_no_case_restriction;


  opt.waitretry = 10;


  opt.useservertimestamps = true;

  /* Use a negative value to mark the absence of --start-pos option */
  opt.start_pos = -1;
  opt.show_progress = -1;
  opt.noscroll = false;
}

extern struct ptimer *timer;

/* Free the memory allocated by global variables.  */
void
cleanup (void)
{
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
  xfree (opt.dir_prefix);
  xfree (opt.default_page);
  free_vec (opt.accepts);
  free_vec (opt.rejects);
  free_vec ((char **)opt.excludes);
  free_vec ((char **)opt.includes);
  free_vec (opt.domains);
  free_vec (opt.follow_tags);
  free_vec (opt.ignore_tags);
  xfree (opt.progress_type);
  xfree (opt.ftp_user);
  xfree (opt.ftp_passwd);
  xfree (opt.https_proxy);
  free_vec (opt.no_proxy);
  xfree (opt.proxy_user);
  xfree (opt.proxy_passwd);
  xfree (opt.useragent);
  xfree (opt.referer);
  xfree (opt.http_user);
  xfree (opt.http_passwd);
  xfree (opt.dot_style);
# ifdef HAVE_SSL
  xfree (opt.cert_file);
  xfree (opt.private_key);
  xfree (opt.ca_directory);
  xfree (opt.ca_cert);
  xfree (opt.crl_file);
  xfree (opt.pinnedpubkey);
# endif
  xfree (opt.cookies_input);
  xfree (opt.cookies_output);
  xfree (opt.base_href);
  xfree (opt.method);
  xfree (opt.post_file_name);
  xfree (opt.post_data);
  xfree (opt.body_data);
  xfree (opt.body_file);
  xfree (opt.use_askpass);

  xfree (opt.hsts_file);

  xfree (opt.wgetrcfile);
  xfree (exec_name);
  ptimer_destroy (timer); timer = NULL;
  quotearg_free ();

#endif /* DEBUG_MALLOC */
}

