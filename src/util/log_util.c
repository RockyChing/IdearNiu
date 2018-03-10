#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <log_util.h>


/*
 * format: src/util/log_util.c:25: assert_test_entry(): Assertion `val' failed.
 */
void assert_fail(const char *assertion, const char *file,
		unsigned int line, const char *function)
{
	const char *format = "%s:%u: %s(): Assertion `%s' failed.\n";
	fprintf(stderr, format, file, line, function, assertion);
	abort();
}

void sys_debug (int level, char *fmt, ...)
{
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

