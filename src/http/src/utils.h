#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <wget.h>

/* Constant is using when we don`t know attempted size exactly */
#define UNKNOWN_ATTEMPTED_SIZE -3

#ifndef MAX_PINNED_PUBKEY_SIZE
#define MAX_PINNED_PUBKEY_SIZE 1048576 /* 1MB */
#endif

/* Macros that interface to malloc, but know about type sizes, and
   cast the result to the appropriate type.  The casts are not
   necessary in standard C, but Wget performs them anyway for the sake
   of pre-standard environments and possibly C++.  */

#define xnew(type) (xmalloc (sizeof (type)))
#define xnew0(type) (xcalloc (1, sizeof (type)))
#define xnew_array(type, len) (xmalloc ((len) * sizeof (type)))
#define xnew0_array(type, len) (xcalloc ((len), sizeof (type)))

#define alloca_array(type, size) ((type *) alloca ((size) * sizeof (type)))

#define xfree(p) do { free ((void *) (p)); p = NULL; } while (0)

struct hash_table;

struct file_memory {
  char *content;
  long length;
  int mmap_p;
};

#define HYPHENP(x) (*(x) == '-' && !*((x) + 1))

char *time_str (time_t);
char *datetime_str (time_t);

char *xstrdup_lower (const char *);

char *strdupdelim (const char *, const char *);
char **sepstring (const char *);
bool subdir_p (const char *, const char *);

char *aprintf (const char *, ...) GCC_FORMAT_ATTR (1, 2);
char *concat_strings (const char *, ...);

typedef struct file_stat_s {
  int access_err;               /* Error in accecssing file : Not present vs permission */
  ino_t st_ino;                 /* st_ino from stats() on the file before open() */
  dev_t st_dev;                 /* st_dev from stats() on the file before open() */
} file_stats_t;

int remove_link (const char *);
bool file_exists_p (const char *, file_stats_t *);
bool file_non_directory_p (const char *);
wgint file_size (const char *);
char *unique_name (const char *, bool);
FILE *unique_create (const char *, bool, char **);
FILE *fopen_excl (const char *, int);
FILE *fopen_stat (const char *, const char *, file_stats_t *);
int   open_stat  (const char *, int, mode_t, file_stats_t *);
char *file_merge (const char *, const char *);

int fnmatch_nocase (const char *, const char *, int);
bool acceptable (const char *);
bool accept_url (const char *);
bool accdir (const char *s);
char *suffix (const char *s);
bool match_tail (const char *, const char *, bool);
bool has_wildcards_p (const char *);

bool has_html_suffix_p (const char *);

struct file_memory *wget_read_file (const char *);
void wget_read_file_free (struct file_memory *);

void free_vec (char **);
char **merge_vecs (char **, char **);
char **vec_append (char **, const char *);
const char *with_thousand_seps (wgint);

/* human_readable must be able to accept wgint and SUM_SIZE_INT
   arguments.  On machines where wgint is 32-bit, declare it to accept
   double.  */
#if SIZEOF_WGINT >= 8
# define HR_NUMTYPE wgint
#else
# define HR_NUMTYPE double
#endif
char *human_readable (HR_NUMTYPE, const int, const int);


int numdigit (wgint);
char *number_to_string (char *, wgint);
char *number_to_static_string (wgint);
wgint convert_to_bits (wgint);

int determine_screen_width (void);
int random_number (int);
double random_float (void);

void xsleep (double);

/* How many bytes it will take to store LEN bytes in base64.  */
#define BASE64_LENGTH(len) (4 * (((len) + 2) / 3))

size_t wget_base64_encode (const void *, size_t, char *);
ssize_t wget_base64_decode (const char *, void *, size_t);

void stable_sort (void *, size_t, size_t, int (*) (const void *, const void *));

const char *print_decimal (double);


#ifndef HAVE_STRLCPY
size_t strlcpy (char *dst, const char *src, size_t size);
#endif

void wg_hex_to_string (char *str_buffer, const char *hex_buffer, size_t hex_len);

extern unsigned char char_prop[];

#ifdef HAVE_SSL
/* Check pinned public key. */
bool wg_pin_peer_pubkey (const char *pinnedpubkey, const char *pubkey, size_t pubkeylen);
#endif

#endif /* UTILS_H */

#ifndef _XUTILS_H
#define _XUTILS_H
#define xstrdup (strdup)
#define xcalloc (calloc)


void *xmalloc(int size);
void *xrealloc(void *ptr, int size);

void *xmemdup (void const *p, size_t s);

char *xstrndup (const char *string, size_t n);
char *xstrdup (char const *string);

#endif

