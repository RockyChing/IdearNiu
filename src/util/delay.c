#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

void msleep(unsigned int msecs)
{
	struct timeval sleep_tv;
	sleep_tv.tv_sec = 0;
	sleep_tv.tv_usec = msecs * 1000;
	select(0, NULL, NULL, NULL, &sleep_tv);
}

