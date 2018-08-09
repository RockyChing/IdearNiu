#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#define UDHCPC_PID "/var/run/udhcpc.pid"

static pid_t dhcp_getpid()
{
	FILE *fp = fopen(UDHCPC_PID, "r");
	if (!fp) {
		return -1;
	}

	char pidstr[8] = { 0 };
	if (fgets(pidstr, sizeof(pidstr), fp) != NULL) {
		fclose(fp);
		return (pid_t) atoi(pidstr);
	}

	fclose(fp);
	return -1;
}

/**
 * udhcpc -h show:
 * Signals:
      USR1    Renew lease
      USR2    Release lease
 * This not portable!
 */
static int dhcp_signal(int sig)
{
	int ret = -1;
	pid_t dhcpc_pid = dhcp_getpid();
	if (dhcpc_pid > 0) {
		ret = kill(dhcpc_pid, sig);
	}

	return ret;
}

int dhcp_start(const char *interface)
{
	interface = interface;
    return 0;
}

int dhcp_stop(const char *interface)
{
    interface = interface;
    return 0;
}

int dhcp_release_lease(const char *interface)
{
    return dhcp_signal(SIGUSR2);
}

int dhcp_start_renew(const char *interface)
{
    return dhcp_signal(SIGUSR1);
}

