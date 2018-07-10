#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define LOG_TAG "NetlinkMain"
#include "log.h"

#include "NetlinkManager.h"

static void blockSigpipe()
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0) {
        ALOGW("WARNING: SIGPIPE not blocked\n");
	}
}

int main()
{
    //CommandListener *cl; // by rocky

    NetlinkManager *nm;

    ALOGI("NetlinkManager starting");
    blockSigpipe();

    if (!(nm = NetlinkManager::Instance())) {
        ALOGE("Unable to create NetlinkManager");
        exit(1);
    };

    //nm->setBroadcaster((SocketListener *) cl);
    if (nm->start()) {
        ALOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    } else {
		ALOGD("NetlinkManager started");
	}

    while(1) {
        sleep(30); // 30 sec
    }

    ALOGI("NetlinkManager exiting");
    exit(0);
}