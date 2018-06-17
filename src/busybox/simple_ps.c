/**
 * Only show process name and pid
 */
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <log_util.h>

#if 0
On Linux, the dirent structure is defined as follows:
struct dirent {
   ino_t          d_ino;       /* inode number */
   off_t          d_off;       /* not an offset; see NOTES */
   unsigned short d_reclen;    /* length of this record */
   unsigned char  d_type;      /* type of file; not supported
                                  by all filesystem types */
   char           d_name[256]; /* filename */
};
#endif

int ps_main()
{
	DIR *dir;
	FILE *fp;
	struct dirent *ptr;
	char buff[64];

	dir = opendir("/proc");
	if (dir != NULL) {
		while ((ptr = readdir(dir)) != NULL) {
			if (DT_DIR != ptr->d_type || strchr(ptr->d_name, '.'))
				continue;
			snprintf(buff, sizeof(buff), "/proc/%s/status", ptr->d_name);
			debug("opening path: %s", buff);
			fp = fopen(buff, "r");
			memset(buff, 0, sizeof(buff));
			if (fp != NULL) {
				if (fgets(buff, sizeof(buff)-1, fp) != NULL) {
					debug("status: %s", buff);
				}
				fclose(fp);
			}
		}

		closedir(dir);
	} else {
		error("opendir return NULL");
	}

	return 0;
}
