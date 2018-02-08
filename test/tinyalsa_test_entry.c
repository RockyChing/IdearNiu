#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <type_def.h>
#include <log_util.h>

extern int tinymix_main(int argc, char **argv);
extern int tinycap_main(int argc, char **argv);
extern int tinyplay_main(int argc, char **argv);
extern int tinypcminfo_main(int argc, char **argv);

static void usage(const char *exep)
{
	printf("Usage:\n");
	printf("      %s tinymix/tinyplay/tinycap/tinypcminfo\n", exep);
}

int tinyalsa_test_entry(int argc, char *argv[])
{
	func_enter();
	int i;
	const char *exep = argv[0];
	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	argc -= 1;
	argv += 1;

	if (!strcmp(argv[0], "tinymix")) {
		tinymix_main(argc, argv);
	} else if (!strcmp(argv[0], "tinycap")) {
		tinycap_main(argc, argv);
	} else if (!strcmp(argv[0], "tinyplay")) {
		tinyplay_main(argc, argv);
	} else if (!strcmp(argv[0], "tinypcminfo")) {
		tinypcminfo_main(argc, argv);
	} else {
		usage(exep);
	}

	func_exit();
	return 0;
}

