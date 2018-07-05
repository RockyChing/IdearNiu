/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
#include <dirent.h>

#include "log.h"
#include "CommandListener.h"
#include "NetlinkManager.h"
#include "FwmarkServer.h"


#define LOG_TAG "Netd"

static void blockSigpipe();

const char* const PID_FILE_PATH = "/data/misc/net/netd_pid";
const int PID_FILE_FLAGS = O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW | O_CLOEXEC;
const mode_t PID_FILE_MODE = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;  // mode 0644, rw-r--r--

int main()
{
    // CommandListener *cl; // by rocky

    NetlinkManager *nm;
    FwmarkServer* fwmarkServer;

    ALOGI("Netd 1.0 starting");
    blockSigpipe();

    if (!(nm = NetlinkManager::Instance())) {
        ALOGE("Unable to create NetlinkManager");
        exit(1);
    };

    //cl = new CommandListener();
    //nm->setBroadcaster((SocketListener *) cl);
    if (nm->start()) {
        ALOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    } else {
		ALOGD("Start NetlinkManager");
	}
#if 0 // by rocky
    // Set local DNS mode, to prevent bionic from proxying
    // back to this service, recursively.
    setenv("ANDROID_DNS_MODE", "local", 1);

    fwmarkServer = new FwmarkServer(CommandListener::sNetCtrl);
    if (fwmarkServer->startListener()) {
        ALOGE("Unable to start FwmarkServer (%s)", strerror(errno));
        exit(1);
    }
#endif
    /*
     * Now that we're up, we can respond to commands
     */
    //if (cl->startListener()) {
    //    ALOGE("Unable to start CommandListener (%s)", strerror(errno));
    //    exit(1);
    //}

    while(1) {
        sleep(30); // 30 sec
    }

    ALOGI("Netd exiting");
    exit(0);
}

static void blockSigpipe()
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
        ALOGW("WARNING: SIGPIPE not blocked\n");
}

