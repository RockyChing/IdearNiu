/*
 * Copyright (C) 2011 The Android Open Source Project
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

#define LOG_TAG "ResolverController"
#define DBG 0

#include "log.h"

#ifndef NULL
#define NULL (void *)0
#endif

#include <net/if.h>

//       declarations for _resolv_set_nameservers_for_net and
//       _resolv_flush_cache_for_net

#include "ResolverController.h"

int ResolverController::setDnsServers(unsigned netId, const char* domains,
        const char** servers, int numservers) {
    if (DBG) {
        ALOGD("setDnsServers netId = %u\n", netId);
    }
#if 0
	_resolv_set_nameservers_for_net(netId, servers, numservers, domains);
#else
	ALOGW("setDnsServers not impl");
#endif
    return 0;
}

int ResolverController::clearDnsServers(unsigned netId) {
#if 0
    _resolv_set_nameservers_for_net(netId, NULL, 0, "");
#else
		ALOGW("clearDnsServers not impl");
#endif

    if (DBG) {
        ALOGD("clearDnsServers netId = %u\n", netId);
    }
    return 0;
}

int ResolverController::flushDnsCache(unsigned netId) {
    if (DBG) {
        ALOGD("flushDnsCache netId = %u\n", netId);
    }
#if 0
    _resolv_flush_cache_for_net(netId);
#else
			ALOGW("flushDnsCache not impl");
#endif

    return 0;
}
