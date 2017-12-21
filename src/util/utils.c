#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include <utils.h>
#include <log_util.h>

#define error_null() sys_debug(1, "ERROR: %s() called with NULL!", __FUNCTION__)

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

int is_recoverable (int error)
{
	if ((error == EAGAIN) || (error == EINPROGRESS))
		return 1;

	return 0;
}

size_t xstrlen(const char *str)
{
	if (!str) {
		error_null();
		return 0;
	}
	return strlen(str);
}

int xstrcmp(const char *s1, const char *s2)
{
	if (!s1 || !s2) {
		error_null();
		return 0;
	}
	return strcmp(s1, s2);
}

int xstrncmp (const char *s1, const char *s2, size_t n)
{
	if (!s1 || !s2) {
		error_null();
		return 0;
	}
	return strncmp(s1, s2, n);
}

int xstrcasecmp (const char *s1, const char *s2)
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
 * Split a string @sentence with the specified separator @sep, such as:
 * "$GPRMC,32.8,118.5" --> $GPRMC 32.8 118.5
 */
void xsplit(struct token *tok, const char *sentence, int sep)
{
	const char *str = sentence;
	char *substr = tok->str[0];
	assert_param(tok);
	assert_param(sentence);
	assert_param(sep > 0);

	while (*str) {
		if (*str == sep) {
			tok->str_cnt += 1;
			substr = tok->str[tok->str_cnt];
		} else {
			*substr++ = *str;
		}
		str++;
	}
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

