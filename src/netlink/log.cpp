#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dirent.h> 
#include <unistd.h>
#include <time.h>

#include "log.h"

struct log_tm {
	int tm_usec;	/* us */
	int tm_sec;	/* s – [0,59] */
    int tm_min;	/* minute - [0,59] */
    int tm_hour;	/* hour - [0,23] */
};

static int log_level = DEFAULT_LOG_LEVEL;
static const char *log_level_string[8] = { "ERROR", "WARNING", "INFO", "DEBUG" };

static void log_time(struct log_tm *ltm)
{
	struct timeval tv;
	struct tm *tm;
	gettimeofday(&tv, NULL);
	tm = localtime((time_t *) &tv.tv_sec);
	ltm->tm_hour = tm->tm_hour;
	ltm->tm_min = tm->tm_min;
	ltm->tm_sec = tm->tm_sec;
	ltm->tm_usec = tv.tv_usec;
}

void sys_debug(int level, const char *tag, int line_num, const char *fmt, ...)
{
	if (level > log_level || level < 0)
		return;

	char buf[512] = { 0 };
	struct log_tm ltm;
	va_list ap;
	va_start(ap, fmt);

	log_time(&ltm);
	vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	if (strstr (buf, "%s") != NULL) {
		fprintf (stderr, "WARNING, xa_debug() called with '%%s' formatted string [%s]!", buf);
		goto exit;
	}

	fprintf(stderr, "[%s(%d)_%s %02d:%02d:%02d.%d] %s\n", tag, line_num, log_level_string[level],
			ltm.tm_hour, ltm.tm_min, ltm.tm_sec, ltm.tm_usec, buf);
	fflush(stderr);
exit:
	va_end (ap);
	//unlock(log_mutex);
}


