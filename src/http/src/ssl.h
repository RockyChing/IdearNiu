#ifndef GEN_SSLFUNC_H
#define GEN_SSLFUNC_H

bool ssl_init (void);
bool ssl_connect_wget (int, const char *, int *);
bool ssl_check_certificate (int, const char *);

#endif /* GEN_SSLFUNC_H */
