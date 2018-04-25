/*
 * This is used in embedded MCU-oriented software,
 * we just build it here
 */
#include <stdio.h>
#include <string.h>


#include <driver_model.h>
#include <driver_port.h>

struct driver drivers[MAX_DRIVER] = { { } };

int dm_open(const char *name)
{
	int i;
	if (!name)
		return -1;
	for (i = 0; i < MAX_DRIVER; i ++) {
		if (drivers[i].driver_name && !strcmp(drivers[i].driver_name, name))
			return i;
	}

	return -1;
}

int dm_read(int fd, char *buf, int count)
{
	int ret;
	if (fd < 0 || fd >= MAX_DRIVER || !buf || count <=0)
		return -1;
	if (drivers[fd].ops && drivers[fd].ops->read)
		ret = drivers[fd].ops->read((void *)buf, count);
	else
		ret =  -1;
	return ret;
}

int dm_write(int fd, const char *buf, int count)
{
	int ret;
	if (fd < 0 || fd >= MAX_DRIVER || !buf || count <=0)
		return -1;
	if (drivers[fd].ops && drivers[fd].ops->write)
		ret = drivers[fd].ops->write(buf, count);
	else
		ret =  -1;
	return ret;
}

int dm_ioctl(int fd, unsigned int cmd, unsigned long arg)
{
	int ret;
	if (fd < 0 || fd >= MAX_DRIVER)
		return -1;
	if (drivers[fd].ops && drivers[fd].ops->ioctl)
		ret = drivers[fd].ops->ioctl(cmd, arg);
	else
		ret =  -1;
	return ret;
}

int driver_register(const char *name, struct driver_ops *ops)
{
	int i;
	if (!name || !ops)
		return -1;
	for (i = 0; i < MAX_DRIVER; i ++) {
		if (!drivers[i].driver_name)
			break;
	}

	if (i >= MAX_DRIVER) {
		printf("System load driver error: too many drivers!\n");
		return -1;
	}

	drivers[i].fd = i;
	drivers[i].driver_name = name;
	drivers[i].ops = ops;
	printf("i = %d\n", i);
	return 0;
}

void driver_deregister(const char *driver_name)
{
	int i;
	if (!driver_name)
		return;
	for (i = 0; i < MAX_DRIVER; i ++) {
		if (drivers[i].driver_name && !strcmp(drivers[i].driver_name, driver_name))
			break;
	}

	if (i >= MAX_DRIVER) {
		printf("System unload drivers error: not found!\n");
		return;
	}

	drivers[i].fd = -1;
	drivers[i].driver_name = NULL;
	drivers[i].ops = NULL;
}

