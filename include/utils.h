#ifndef _UTILITY_ES_H
#define _UTILITY_ES_H
#include <sys/types.h>
#include <time.h>

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) :(b))
#endif

#define NUM_ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))


int is_recoverable (int error);
size_t sys_strlen(const char *str);
int sys_strcmp(const char *s1, const char *s2);
int sys_strncmp (const char *s1, const char *s2, size_t n);
int sys_strcasecmp (const char *s1, const char *s2);

time_t get_time();
char *get_ctime(const time_t *t);

#endif

