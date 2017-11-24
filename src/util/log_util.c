#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <log_util.h>

void sys_debug (int level, char *fmt, ...)
{
#if 0
	char buf[BUFSIZE];
	va_list ap;
	char *logtime = NULL;
	mythread_t *mt = thread_check_created ();
#ifdef OPTIMIZE
	return;
#endif
	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);

	if (!mt)
		fprintf (stderr, "WARNING: No mt while outputting [%s]", buf);
  
	logtime = get_log_time();

	if (!mt) 
		return;

#ifdef DEBUG_FULL
	fprintf (stderr, "\r[%s] [%ld:%s] %s\n", logtime, mt->id, nullcheck_string (mt->name), buf);
#endif

	if (strstr (buf, "%s") != NULL) {
		fprintf (stderr, "WARNING, xa_debug() called with '%%s' formatted string [%s]!", buf);
		return;
	}


	if (info.logfiledebuglevel >= level)
		if (info.logfile != -1) {
			fd_write (info.logfile, "[%s] [%ld:%s] %s\n", logtime, mt->id, nullcheck_string (mt->name), buf);
		}

	if (info.consoledebuglevel >= level) {
			printf("\r[%s] [%ld:%s] %s\n", logtime, mt->id, nullcheck_string (mt->name), buf);
			fflush(stdout);
	}
	
	if (!(running == SERVER_RUNNING)) {
		if (info.consoledebuglevel >= level)
			fprintf (stderr, "[%s] %s\n", logtime, buf);
	}
	
	if (logtime)
		free(logtime);
	va_end (ap);
#endif
	char buf[BUFSIZE];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);
	if (strstr (buf, "%s") != NULL) {
		fprintf (stderr, "WARNING, xa_debug() called with '%%s' formatted string [%s]!", buf);
		return;
	}

	fprintf (stderr, "%s\n", buf);
	va_end (ap);
}

