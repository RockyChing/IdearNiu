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

struct options opt;
const char *exec_name;

/* Number of successfully downloaded URLs */
int numurls = 0;

struct ptimer *timer;


static void http_dload(const char *durl)
{
	char *url = NULL;
	char *rewritten = rewrite_shorthand_url(durl);
	if (rewritten) {
		url = rewritten;
	} else {
		url = xstrdup(durl);
	}

	printf("url[0]: %s", url);

	char *filename = NULL, *redirected_URL = NULL;
	int dt, url_err;
	struct url *url_parsed;

	url_parsed = url_parse(url, &url_err, false);
	if (!url_parsed) {
		logprintf(LOG_NOTQUIET, "URL error!\n");
	} else {
		retrieve_url(url_parsed, &filename, &redirected_URL, NULL, &dt, 0, true);
	}

	xfree(redirected_URL);
	xfree(filename);
	xfree(url);
	url_free(url_parsed);
}

int main(int argc, char **argv)
{
	total_downloaded_bytes = 0;
	exec_name = argv[0];
	if (argc != 2) {
		printf("Usage: exec_name url\n");
		exit(-1);
	}

	timer = ptimer_new();
	ptimer_measure(timer);

	/* Load the hard-coded defaults.  */
	defaults();

	/* Initialize logging ASAP.  */
	log_init(NULL, false);

	/* All user options have now been processed, so it's now safe to do
	interoption dependency checks. */

	debug("opt.verbose: %d\n", opt.verbose);
	debug("opt.show_progress: %d\n", opt.show_progress);
    opt.verbose = 1;
    opt.show_progress = -1;

	/* Initialize progress.  Have to do this after the options are
		processed so we know where the log file is.  */
	if (opt.show_progress)
		set_progress_implementation (opt.progress_type);

	http_dload(argv[1]);

	cleanup();
	exit(0);
}


