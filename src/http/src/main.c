#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <spawn.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "utils.h"
#include "init.h"
#include "retr.h"
#include "recur.h"
#include "host.h"
#include "url.h"
#include "convert.h"
#include "http.h"               /* for save_cookies */
#include "ptimer.h"
#include <getopt.h>


#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR '/'
#endif

void *ares;

struct options opt;

/* defined in version.c */
extern char *system_getrc;
#define MAX_CHARS_PER_LINE      72
#define TABULATION              4

const char *exec_name;

/* Number of successfully downloaded URLs */
int numurls = 0;

#ifdef HAVE_SSL
#define IF_SSL(x) x
#else
#define IF_SSL(x) NULL
#endif

#define BOOLEAN_NEG_MARKER 1024

struct ptimer *timer;

int main (int argc, char **argv)
{
	char **url, **t, *p;
	int i;
	int nurl;
	int argstring_length;

	timer = ptimer_new ();
	ptimer_measure (timer);

	total_downloaded_bytes = 0;

	/* Construct the name of the executable, without the directory part.  */
	exec_name = argv[0];

	/* Construct the arguments string. */
	for (argstring_length = 1, i = 1; i < argc; i++)
		argstring_length += strlen (argv[i]) + 3 + 1;
	p = malloc (argstring_length);

	if (p == NULL) {
		fprintf(stderr, "Memory allocation problem\n");
		exit(0);
	}

	for (i = 1; i < argc; i++) {
		int arglen;

		*p++ = '"';
		arglen = strlen (argv[i]);
		memcpy (p, argv[i], arglen);
		p += arglen;
		*p++ = '"';
		*p++ = ' ';
	}

	*p = '\0';
	/* Load the hard-coded defaults.  */
	defaults ();

	opterr = 0;
	optind = 0;

	nurl = argc - optind;
	debug("p: %s\n", p);
	/* Initialize logging ASAP.  */
	log_init (NULL, false);

	/* All user options have now been processed, so it's now safe to do
	interoption dependency checks. */

	if (!opt.no_dirstruct)
		opt.dirstruct = 1;      /* normally handled by cmd_spec_recursive() */

	debug("opt.verbose: %d\n", opt.verbose);
	debug("opt.show_progress: %d\n", opt.show_progress);
    opt.verbose = 1;
    opt.show_progress = -1;

	if (!nurl) {
		/* No URL specified.  */
#if 1
		fprintf(stderr, "%s: missing URL\n", exec_name);
		fprintf(stderr, "\n");
		/* #### Something nicer should be printed here -- similar to the
		pre-1.5 `--help' page.  */
		fprintf(stderr, "Try `%s --help' for more options.\n", exec_name);
#endif
		exit (0);
	}

	/* Initialize progress.  Have to do this after the options are
		processed so we know where the log file is.  */
	if (opt.show_progress)
		set_progress_implementation (opt.progress_type);

	/* Fill in the arguments.  */
	url = alloca_array (char *, nurl + 1);
	if (url == NULL) {
		fprintf (stderr, ("Memory allocation problem\n"));
		exit (0);
	}

	for (i = 0; i < nurl; i++, optind++) {
		char *rewritten = rewrite_shorthand_url (argv[optind]);
		if (rewritten)
		url[i] = rewritten;
		else
		url[i] = xstrdup (argv[optind]);
	}

	url[i] = NULL;
	debug("url[0]: %s", url[0]);

	/* Retrieve the URLs from argument list.  */
	for (t = url; *t; t++) {
		char *filename = NULL, *redirected_URL = NULL;
		int dt, url_err;
		struct url *url_parsed;

		url_parsed = url_parse(*t, &url_err, true);
		if (!url_parsed) {
			char *error = url_error(*t, url_err);
			logprintf (LOG_NOTQUIET, "%s: %s.\n",*t, error);
			xfree (error);
		} else {
			retrieve_url(url_parsed, *t, &filename, &redirected_URL, NULL, &dt, 0, true);
		}

		if (opt.delete_after && filename != NULL && file_exists_p (filename, NULL)) {
			DEBUGP (("Removing file due to --delete-after in main():\n"));
			logprintf (LOG_VERBOSE, ("Removing %s.\n"), filename);
			if (unlink (filename))
				logprintf (LOG_NOTQUIET, "unlink: %s\n", strerror(errno));
		}

		xfree(redirected_URL);
		xfree(filename);
		url_free(url_parsed);
	}

	cleanup();
	exit(0);
}

