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
#include <getopt.h>

struct options opt;
const char *exec_name;

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

	struct url *url_parsed;

	url_parsed = url_parse(url, false);
	if (!url_parsed) {
		logprintf(LOG_NOTQUIET, "URL error!\n");
	} else {
		dump_struct_url(url_parsed);
		retrieve_url(url_parsed);
	}

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

	/* Load the hard-coded defaults.  */
	defaults();

	/* Initialize logging ASAP.  */
	log_init(NULL, false);

    opt.verbose = 1;

	http_dload(argv[1]);

	cleanup();
	exit(0);
}


