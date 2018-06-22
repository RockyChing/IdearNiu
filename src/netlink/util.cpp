#include <stdio.h>

int property_get(const char *name, char *value)
{
	printf("[NETD] property_get()");
}

int property_get(const char *key, char *value, const char *default_value)
{
	printf("[NETD] property_get() 2");
}

