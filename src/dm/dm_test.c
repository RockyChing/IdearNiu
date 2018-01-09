#include <stdio.h>
#include <string.h>

#include <driver_model.h>
#include <log_util.h>

#define DRIVER_NAME "test driver"


int dm_test_read(void *buf, int count)
{
	int ret = -1;
	func_enter();

	strncpy(buf, "dm_test_read", count);
	ret = count;

	func_exit();
	return ret;
}


int dm_test_write(const void *buf, int count)
{
	int ret = -1;
	char buff[1500];
	func_enter();

	strncpy(buff, buf, count);
	buff[count] = '\0';
	ret = count;

	printf("dm_test_write: %s\n", buff);
	func_exit();
	return ret;
}

int dm_test_ioctl(unsigned int cmd, unsigned long arg)
{
	func_enter();

	printf("dm_test_ioctl enter\n");

	func_exit();
	return 0;
}

struct driver_ops test_driver_ops = {
	.read = dm_test_read,
	.write = dm_test_write,
	.ioctl = dm_test_ioctl,
};

void driver_model_test_entry()
{
	func_enter();
	char buff[1500] = {'\0'};
	int fd = -1;
	int ret;
	ret = driver_register(DRIVER_NAME, &test_driver_ops);
	printf("register %s %s!\n", DRIVER_NAME, ret == 0 ? "OK" : "failed");

	fd = dm_open(DRIVER_NAME);
	printf("%s fd = %d\n", DRIVER_NAME, fd);

	ret = dm_read(fd, buff, 5);
	printf("read from %s: %s[%d]\n", DRIVER_NAME, buff, ret);

	ret = dm_write(fd, "write test", 8);
	printf("write return %d bytes\n", ret);

	ret = dm_ioctl(fd, 1, 123);
	printf("ioctl return %d\n", ret);

	driver_deregister(DRIVER_NAME);
	fd = dm_open(DRIVER_NAME);
	printf("open after `driver_deregister` called test: %s fd = %d\n", DRIVER_NAME, fd);
	ret = dm_write(fd, "write test", 8);
	printf("dm_write after `driver_deregister` called test: %d\n", ret);

	func_exit();
}

