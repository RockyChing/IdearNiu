/*
 * Copyright 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <stdbool.h>
#include <stdlib.h>

#include <log_util.h>
#include "dhcp.h"
#include "ifc.h"
#include "dhcpmsg.h"

#define DHCPC_SCRIPT "./src/dhcpc/script/default.script"

extern int spawn_and_wait(char **argv);

/* Call a script with a par file and env vars */
static void udhcp_run_script(struct dhcp_msg *packet, const char *interface, const char *name)
{
	char envp[2][64];
	char *argv[3];

	memset(envp, 0, sizeof(envp));
	sprintf(envp[0], "interface=%s", interface);
	putenv(envp[0]);

	/* call script */
	debug("Executing %s %s", DHCPC_SCRIPT, name);
	argv[0] = (char*) DHCPC_SCRIPT;
	argv[1] = (char*) name;
	argv[2] = NULL;
	spawn_and_wait(argv);
}

int dhcp_main(int argc, char *argv[])
{
	if (argc != 2) {
		error("usage: %s INTERFACE", argv[0]);
		return -1;
	} else {
		udhcp_run_script(NULL, argv[1], DHCPC_DECONFG);
		return -1;
	}

	char *interface = argv[1];
	if (ifc_init()) {
		error("dhcptool %s: ifc_init failed", interface);
		return -1;
	}

	udhcp_run_script(NULL, interface, DHCPC_DECONFG);
	int rc = do_dhcp(interface);
	if (rc) {
		error("dhcptool %s: do_dhcp failed", interface);
		return -1;
	}

	ifc_close();

	return rc ? -1 : 0;
}

