#include <stdio.h>

int property_get(const char *name, char *value)
{
	printf("[NETD] property_get()");
	return 0;
}

int property_get(const char *key, char *value, const char *default_value)
{
	printf("[NETD] property_get() 2");
	return 0;
}

int property_set(const char *name, const char *value)
{
	printf("[NETD] property_set()");
	return 0;
}

