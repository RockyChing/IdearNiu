/*
 * This is used in embedded MCU-oriented software,
 * we just build it here
 */
#ifndef __DRIVER_MODEL_H
#define __DRIVER_MODEL_H
#include <stdio.h>

struct driver_ops {
	int (*read) (void *buf, int count);
	int (*write) (const void *buf, int count);
	int (*ioctl) (unsigned int cmd, unsigned long arg);
};

struct driver {
	int fd; /* index of driver array */
	const char *driver_name; /* name of the driver */
	struct driver_ops *ops; /* interface to the driver */
};


int dm_open(const char *name);
int dm_read(int fd, char *buf, int count);
int dm_write(int fd, const char *buf, int count);
int dm_ioctl(int fd, unsigned int cmd, unsigned long arg);

int driver_register(const char *name, struct driver_ops *ops);
void driver_deregister(const char *driver_name);

#endif
