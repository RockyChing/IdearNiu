#ifndef _NETD_UTIL_H
#define _NETD_UTIL_H
#include <stdio.h>

#ifndef UINT_MAX
#define UINT_MAX -1
#endif

#define PROP_VALUE_MAX  92
#define PROPERTY_VALUE_MAX  PROP_VALUE_MAX
#define UID_MAX	UINT_MAX 

int property_get(const char *name, char *value);
int property_get(const char *key, char *value, const char *default_value);


#endif

