#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include <utils.h>
#include <log_util.h>

#define error_null() sys_debug(1, "ERROR: %s() called with NULL!", __FUNCTION__)

int is_recoverable (int error)
{
	if ((error == EAGAIN) || (error == EINTR) || (error == EINPROGRESS))
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

