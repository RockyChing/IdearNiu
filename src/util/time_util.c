#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <sys/msg.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>

#define d printf

#if 0
struct tm { /* a broken-down time */
	int tm_sec; /* seconds after the minute: [0 - 60] */
	int tm_min; /* minutes after the hour: [0 - 59] */
	int tm_hour; /* hours after midnight: [0 - 23] */
	int tm_mday; /* day of the month: [1 - 31] */
	int tm_mon; /* months since January: [0 - 11] */
	int tm_year; /* years since 1900 */
	int tm_wday; /* days since Sunday: [0 - 6] */
	int tm_yday; /* days since January 1: [0 - 365] */
	int tm_isdst; /* daylight saving time flag: <0, 0, >0 */
};

#endif
static void dump_tm(const struct tm *t)
{
	d("dump_tm --------\n");
	d("tm_sec: %d\n", t->tm_sec);
	d("tm_min: %d\n", t->tm_min);
	d("tm_hour: %d\n", t->tm_hour);
	d("tm_mday: %d\n", t->tm_mday);
	d("tm_mon: %d\n", t->tm_mon);
	d("tm_year: %d\n", t->tm_year);
	d("tm_wday: %d\n", t->tm_wday);
	d("tm_yday: %d\n", t->tm_yday);
	d("tm_isdst: %d\n\n", t->tm_isdst);
}

static void dump_tv(const struct timeval *tv)
{
	d("dump_tv --------\n");
	d("tv_sec: %d\n", tv->tv_sec);
	d("tv_usec: %d\n\n", tv->tv_usec);
}

static void dump_time()
{
	time_t time_utc = time(NULL);
	d("time_utc: %d\n", time_utc);

	// int gettimeofday(struct timeval *tv, struct timezone *tz);
	struct timeval tv;
	gettimeofday(&tv, NULL);
	dump_tv(&tv);

	// struct tm *gmtime_r(const time_t *timep, struct tm *result);
	struct tm tm_gmt;
	gmtime_r(&time_utc, &tm_gmt);
	dump_tm(&tm_gmt);

	time_t time_mk_utc = mktime(&tm_gmt);
	d("time_mk_utc: %d\n\n", time_mk_utc);

	// struct tm *localtime_r(const time_t *timep, struct tm *result);
	struct tm tm_loc;
	localtime_r(&time_utc, &tm_loc);
	dump_tm(&tm_loc);

	time_t time_mk_loc = mktime(&tm_loc);
	d("time_mk_loc: %d\n\n", time_mk_loc);
}

int time_main(int argc, char *argv[])
{
	dump_time();
#if 0
	time_utc: 1536829458
	dump_tv --------
	tv_sec: 1536829458
	tv_usec: 64148

	dump_tm --------
	tm_sec: 18
	tm_min: 4
	tm_hour: 9
	tm_mday: 13
	tm_mon: 8
	tm_year: 118
	tm_wday: 4
	tm_yday: 255
	tm_isdst: 0

	time_mk_utc: 1536800658

	dump_tm --------
	tm_sec: 18
	tm_min: 4
	tm_hour: 17
	tm_mday: 13
	tm_mon: 8
	tm_year: 118
	tm_wday: 4
	tm_yday: 255
	tm_isdst: 0

	time_mk_loc: 1536829458
#endif
	return 0;
}

