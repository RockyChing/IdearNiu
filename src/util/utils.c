#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include <utils.h>
#include <log_util.h>

#define error_null() sys_debug(1, "ERROR: %s() called with NULL!", __FUNCTION__)

int is_recoverable (int error)
{
	if ((error == EAGAIN) || (error == EINPROGRESS))
		return 1;

	return 0;
}

size_t sys_strlen(const char *str)
{
	if (!str) {
		error_null();
		return 0;
	}
	return strlen(str);
}

int sys_strcmp(const char *s1, const char *s2)
{
	if (!s1 || !s2) {
		error_null();
		return 0;
	}
	return strcmp(s1, s2);
}

int sys_strncmp (const char *s1, const char *s2, size_t n)
{
	if (!s1 || !s2) {
		error_null();
		return 0;
	}
	return strncmp(s1, s2, n);
}

int sys_strcasecmp (const char *s1, const char *s2)
{
	if (!s1 || !s2)	{
		error_null();
		return 0;
	}
	return strcasecmp (s1, s2);
}

time_t get_time()
{
    return time(NULL);
}

char *get_ctime(const time_t *t)
{
    char *str_time = ctime(t);
    return str_time ? str_time : "(null)";
}

/*
 * Print an error message and exit
 */
static void panic(char *str, ...)
{
	char buf[BUFSIZE] = {'\0'};
	va_list ap;

	va_start(ap, str);
	vsnprintf(buf, BUFSIZE, str, ap);
	fprintf(stderr, "panic: %s\n", buf);
	va_end (ap);
	exit(4);
}

/*
 * Panic on failing malloc
 */
void *xmalloc(int size)
{
	void *ret;
	if (!size)
		size ++;
	ret = malloc(size);
	if (ret == (void *)0)
		panic("Couldn't allocate memory!");
	return ret;
}

/*
 * Panic on failing realloc
 */
void *xrealloc(void *ptr, int size)
{
	void *ret;

	ret = realloc(ptr, size);
	if(ret == (void *)0)
		panic("Couldn't re-allocate memory!");
	return ret;
}

