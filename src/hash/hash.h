#ifndef HASH_H
#define HASH_H

struct hash_table;

struct hash_table *hash_table_new(int, unsigned long (*) (const void *),
                                   int (*) (const void *, const void *));
void hash_table_destroy(struct hash_table *);

void *hash_table_get(const struct hash_table *, const void *);
int hash_table_get_pair(const struct hash_table *, const void *, void *, void *);
int hash_table_contains(const struct hash_table *, const void *);

void hash_table_put(struct hash_table *, const void *, const void *);
int hash_table_remove(struct hash_table *, const void *);
void hash_table_clear(struct hash_table *);

void hash_table_for_each(struct hash_table *, int (*) (void *, void *, void *), void *);

typedef struct {
	void *key, *value;    /* public members */
	void *pos, *end;      /* private members */
} hash_table_iterator;
void hash_table_iterate(struct hash_table *, hash_table_iterator *);
int hash_table_iter_next(hash_table_iterator *);

int hash_table_count(const struct hash_table *);

struct hash_table *make_string_hash_table(int);
struct hash_table *make_nocase_string_hash_table(int);

unsigned long hash_pointer(const void *);

#endif /* HASH_H */

