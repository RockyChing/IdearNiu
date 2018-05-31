#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <log_util.h>
#include <type_def.h>

/**
 * file_create - create a file and return the file descriptor
 */
FILE *file_create(const char *path)
{
	/**
	 * Open for appending (writing at end of file). The file is created
	 * if it does not exist. The stream is positioned at the end of the file.
	 */
	FILE *fp = fopen(path, "ab");
	if (!fp) {
		error("%s(): fopen return null", __FUNCTION__);
	}

	return fp;
}

/**
 * file_open - open the file and return the file descriptor
 */
FILE *file_open(const char *path)
{
	/**
	 * Open text file for reading. The stream is positioned at the
	 * beginning of the file.
	 */
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		error("%s(): fopen return null", __FUNCTION__);
	}

	return fp;
}

/**
 * file_size - calc the size of specified file
 */
long file_size(FILE *fp)
{
	long size;
	if (!fp) {
		error("%s(): null parameter", __FUNCTION__);
		return -1;
	}

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	if (size == -1) {
		error("%s(): %s", __FUNCTION__, strerror(errno));
	}

	return size;
}

size_t file_read(FILE *fp, uint8_t *buff, size_t count)
{
	size_t ret = fread(buff, 1, count, fp);
	if (ret < count) {
		/**
		 * feof() tests the end-of-file indicator for the stream
         * pointed to by @fp, returning nonzero if it is set
         *
         * ferror() tests the error indicator for the stream pointed
         * to by @fp, returning nonzero if it is set
         */
		if (feof(fp)) {
			debug("reach to the end of firmware file");
		} else if (ferror(fp)) {
			error("fread error of firmware file");
			return -1;
		}
	}

	return ret;
}

size_t file_write(FILE *fp, uint8_t *buff, size_t count)
{
	size_t ret = fwrite(buff, 1, count, fp);
	if (ret < count) {
		if (ferror(fp)) {
			error("fwrite error of firmware file");
			return -1;
		}
	}

	return ret;
}

/**
 * firmware_flush - forces a write of all user-space buffered
 *			data for the given output or update @fp
 */
void file_flush(FILE *fp)
{
	if (fflush(fp)) {
		error("fflush error: %s", strerror(errno));
	}
}

void file_close(FILE *fp)
{
	fclose(fp);
}

void file_empty(int fd)
{
	if (ftruncate(fd, 0) < 0) {
		error("ftruncate error: %s", strerror(errno));
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		error("lseek error: %s", strerror(errno));
	}
}

#if 0
void file_test()
{
	FILE *fp = file_open("./lsusb");
	FILE *wfp = file_create("./test");
	// same size to 'ls' cmd
	long file_size = file_size(fp);
	debug("file_size: %d", file_size);
	int cnt = file_size / 255;
	int i;
	uint8_t buff[255];
	size_t read_size;
	fseek(fp, 0L, SEEK_SET);
	for (i = 0; i <= cnt; i ++) {
		read_size = file_read(fp, buff, 255);
		file_write(wfp, buff, read_size);
		if (read_size < 255)
			break;
	}

	file_flush(wfp);

	file_close(fp);
	file_close(wfp);
}
#endif

