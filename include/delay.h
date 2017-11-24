#ifndef __DELAY_H
#define __DELAY_H

/**
 * Linux C has usleep() and sleep()
 */

inline void msleep(unsigned int msecs)
{
	struct timeval sleep_tv;
	sleep_tv.tv_sec = 0;
	sleep_tv.tv_usec = msecs * 1000;
	select (1, NULL, NULL, NULL, &sleep_tv);
}

#endif
