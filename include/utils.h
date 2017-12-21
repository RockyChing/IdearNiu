#ifndef _UTILITY_ES_H
#define _UTILITY_ES_H
#include <sys/types.h>
#include <time.h>

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) :(b))
#endif

#define NUM_ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))

#define TOK_MAX_SZ	(32)
#define TOK_MAX_CNT (32)
struct token {
	char str[TOK_MAX_CNT][TOK_MAX_SZ];
	int str_cnt;
};

int is_recoverable (int error);
size_t xstrlen(const char *str);
int xstrcmp(const char *s1, const char *s2);
int xstrncmp (const char *s1, const char *s2, size_t n);
int xstrcasecmp (const char *s1, const char *s2);

time_t get_time();
char *get_ctime(const time_t *t);
void xsplit(struct token *tok, const char *sentence, int sep);
void *xmalloc(int size);
void *xrealloc(void *ptr, int size);

#endif

