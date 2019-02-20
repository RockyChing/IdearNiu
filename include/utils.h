#ifndef _UTILITY_ES_H
#define _UTILITY_ES_H
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <type_def.h>

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) :(b))
#endif

#define NUM_ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define SET_NONBLOCKING(fd) {           \
        int flags = fcntl(fd, F_GETFL); \
        flags |= O_NONBLOCK;            \
        fcntl(fd, F_SETFL, flags);      \
	}

#define SET_BLOCKING(fd) {              \
        int flags = fcntl(fd, F_GETFL); \
        flags &= ~O_NONBLOCK;           \
        fcntl(fd, F_SETFL, flags);      \
	}


#define TOK_MAX_SZ	(32)
#define TOK_MAX_CNT (32)

typedef int (*cmd_callback) (char *buff, size_t len);

/**
 * If @ptr is NULL, no operation is performed.
 */
#define xfree(ptr) do { free(ptr); \
		ptr = NULL; \
	} while (0)

struct token {
	char str[TOK_MAX_CNT][TOK_MAX_SZ];
	int  str_cnt;
};

void swap(long *pa, long *pb);
int is_recoverable (int error);
size_t xstrlen(const char *str);
int xstrcmp(const char *s1, const char *s2);
int xstrncmp (const char *s1, const char *s2, size_t n);
int xstrcasecmp (const char *s1, const char *s2);
int run_command(const char *cmd, cmd_callback cmd_cb);

time_t get_time();
char *get_ctime(const time_t *t);
void xsplit(struct token *tok, const char *sentence, int sep);
void *xmalloc(int size);
void *xrealloc(void *ptr, int size);
void *zmalloc(int size);
void *zrealloc(void *ptr, int size);

void sleep_us(uint32_t us);
void sleep_ms(uint32_t us);
void sleep_s(uint32_t us);

#endif

