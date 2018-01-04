#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <linux/fb.h>
#include "font_8x16.h"

#define SCREEN_WIDTH	(1280)
#define SCREEN_HEIGHT	(800)
#define ASCII_WIDTH		(8) /* font 8x16 */
#define ASCII_HEIGHT	(16) /* font 8x16 */

// 0x00ff0000 <--> B G R A
#define WHITE (0xffffffff)
#define RED   (0x0000ff00)
#define GREEN (0x00ff0000)
#define BLUE  (0xff000000)
#define BLACK (0x00000000)

#define FBDEV "/dev/fb0"

int fd_fb;							// LCD设备驱动的文件句柄
struct fb_var_screeninfo var;		// 定义LCD的可变参数
struct fb_fix_screeninfo fix; 		// 定义LCD的固定参数
int screen_size;					// 表示整个屏幕所占显存的大小
int line_width;						// 表示屏幕每一行所占显存的大小
int pixel_width;					// 表示每个像素点所占显存的大小
char *fbmem;						// 表示显存的起始地址

int fd_hzk16;						// HZK16汉字库的文件句柄
struct stat hzk16_stat;				// 描述HZK16这个文件的状态信息
char *hzk16mem;						// HZK16这个汉字库映射到内存的起始地址

static int fb_init()
{
	int ret = -1;
	fd_fb = open(FBDEV, O_RDWR);
	if (fd_fb < 0) {
		return -1;
	}

	ret = ioctl(fd_fb, FBIOGET_VSCREENINFO, &var);
	if(ret == -1) {
		printf("can't ioctl for /dev/fb0!\n");
		return -1;
	}

	ret = ioctl(fd_fb, FBIOGET_FSCREENINFO, &fix);
	if(ret == -1) {
		printf("can't ioctl for /dev/fb0!\n");
		return -1;
	}

	/* 获取液晶显存，每一行显存，每一个像素显存的大小 */
	pixel_width = var.bits_per_pixel / 8;
	screen_size = var.xres * var.yres * pixel_width;
	line_width  = var.xres * pixel_width;
	printf("LCD res: %d x %d\n", var.xres, var.yres);
	printf("    pixel_width: %d\n", pixel_width);
	printf("    screen_size: %d\n", screen_size);
	printf("    line_width: %d\n", line_width);

	/* 将液晶显存映射到用户空间 */
	fbmem = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);
	if(fbmem == (char *)-1)	{
		printf("mmap for /dev/fb0 error!\n");
		return -1;
	}

	return 0;
}

static int font_init()
{
	int ret = -1;
	fd_hzk16 = open("/usr/sbin/HZK16", O_RDONLY);
	if (fd_hzk16 < 0) return -1;

	ret = fstat(fd_hzk16, &hzk16_stat);
	if (ret < 0) return -1;

	hzk16mem = mmap(NULL, hzk16_stat.st_size, PROT_READ, MAP_SHARED, fd_hzk16, 0);
	if(hzk16mem == (char *)-1) {
		printf("mmap for HZK16 error!\n");
		return -1;
	}

	return 0;
}

static void clear_screen(int color)
{
	memset(fbmem, color, screen_size);
}

static void deinit()
{
	munmap(fbmem, screen_size);
	close(fd_fb);

	munmap(hzk16mem, hzk16_stat.st_size);
	close(fd_hzk16);
}

static void disp_pixel(int x, int y, int color)
{
	__u8 *pen8 = fbmem + y * line_width + x * pixel_width;
	//__u16 *pen16 = (__u16 *) pen8;
	__u32 *pen32 = (__u32 *) pen8;

	switch (var.bits_per_pixel) {
	case 32:
		*pen32 = color;
		break;
	default:
		printf("Unsupported bits_per_pixel!\n");
		printf("bits_per_pixel must be 32 bits!\n");
		break;
	}
}

static void disp_char(int x, int y, char c)
{
	/* 获取字符在字符数组中的起始位置 */
	unsigned char *buffer = (unsigned char *)&fontdata_8x16[c * 16];
	unsigned char data;
	int i, j;

	/* 循环操作将整个字符写入到显存指定位置中，达到在指定位置显示字符 */
	for(i = 0; i < 16; i++) {
		data = buffer[i];
		for(j = 0; j < 8; j++) {
			if(data & 0x80) {
				disp_pixel(x + j, y + i, WHITE);  /*白色*/
			} else {
				disp_pixel(x + j, y + i, BLACK);  /*黑色*/
			}
			data = data << 1;
		}
	}
}

static void disp_string(int x, int y, const char *str)
{
	assert(str);
	//assert(x < (SCREEN_WIDTH - 8));
	//assert(y < (SCREEN_HEIGHT - 16));
	while (*str) {
		disp_char(x, y, *str);
		str ++;
		x += 8;
		if (x > (SCREEN_WIDTH - ASCII_WIDTH)) {
			x = 0;
			y += ASCII_HEIGHT;
		}

		if (y > (SCREEN_HEIGHT - ASCII_HEIGHT)) {
			y = 0;
		}
	}
}

static void disp_single_hzk16(int x, int y, char *str)
{
	/* 确定汉字在字符中的位置 */
	int area = str[0] - 0xa0 - 1 ;
	int where = str[1] - 0xa0 - 1;
	int offset = (area * 94 + where) * 32;
	__u8 *buffer = (__u8 *) (hzk16mem + offset);
	__u16 data;
	int i, j;

	//printf("offset: %d\n", offset);
	//for(i = 0; i < 32; i ++){
    //    printf("%02X ", buffer[i]);
    //}

	/* 循环的将汉字的点阵写入到屏的显存当中 */
	for (i = 0; i < 16; i ++) {
		data = (buffer[i * 2] << 8) | buffer[2 * i + 1];
		for (j = 0; j < 16; j ++) {
			if (data & 0x8000)
				disp_pixel(x + j, y + i, WHITE); /*白色*/
			else
				disp_pixel(x + j, y + i, BLACK);  /*黑色*/
			data <<= 1;
		}
	}
}

static void disp_hzk16(int x, int y, char *str)
{
	assert(str);
	//assert(x < (SCREEN_WIDTH - 16));
	//assert(y < (SCREEN_HEIGHT - 16));
	while (*str) {
		disp_single_hzk16(x, y, str);
		str += 2;
		x += 16;
		if (x > (SCREEN_WIDTH - 16)) {
			x = 0;
			y += 16;
		}

		if (y > (SCREEN_HEIGHT - 16)) {
			y = 0;
		}
	}
}

static void disp_mix(int x, int y, char *str)
{
	assert(str);
	while (*str) {
		if (*str & 0x80) { // chinese
			disp_single_hzk16(x, y, str);
			str += 2;
			x += 16;
			if (x > (SCREEN_WIDTH - 16)) {
				x = 0;
				y += 16;
			}
		} else {
			disp_char(x, y, *str);
			str += 1;
			x += 8;
			if (x > (SCREEN_WIDTH - ASCII_WIDTH)) {
				x = 0;
				y += ASCII_HEIGHT;
			}
		}

		if (y > (SCREEN_HEIGHT - ASCII_HEIGHT)) {
			y = 0;
		}
	}
}

#define BMP_PATH "/usr/sbin/ram.bmp"
#define BMP_WIDTH (128)
#define BMP_HEIGHT (128)
#define BMP_INDEX (BMP_HEIGHT-1)
#define BMP_SIZE (BMP_WIDTH*BMP_HEIGHT*3+54) // bmp header 54 bytes
static void disp_bmp(int x, int y, const char *path)
{
	int sx, sy, location;

	const int bytes_per_pixel = pixel_width;
	const int screensize = screen_size;
	char *map_mem = fbmem;
	const int start_location = x * bytes_per_pixel + y * line_width;

	FILE *file_fd = fopen(path, "r");
	char buffer[BMP_SIZE];
	int readCnt = fread(buffer, sizeof(buffer), 1, file_fd);
	fclose(file_fd);

	for(sx = 0; sx < BMP_WIDTH; sx ++) {
		for(sy = 0; sy < BMP_HEIGHT; sy++) {
			location = start_location + sx * bytes_per_pixel + sy * line_width;

			// 矫正BMP显示和LCD显示的上下反 BMP_INDEX-y
			*(map_mem + location + 0) = buffer[54+(BMP_WIDTH*(BMP_INDEX-sy)+sx)*3+0];// 蓝色的色深
			*(map_mem + location + 1) = buffer[54+(BMP_WIDTH*(BMP_INDEX-sy)+sx)*3+1];// 绿色的色深
			*(map_mem + location + 2) = buffer[54+(BMP_WIDTH*(BMP_INDEX-sy)+sx)*3+2];// 红色的色深
			//*(map_mem + location + 3) = 0;// 是否透明
		}
	}
}

void fbtest_entry()
{
	int ret;
	ret = fb_init();
	assert(ret == 0);

	ret = font_init();
	assert(ret == 0);

	clear_screen(WHITE);
	sleep(1);

	disp_char(100, 50, '9');
	disp_string(100, 100, "abcdefghijklmnopqrstuvwxyz1234567890");
	disp_string(1000, 200, "abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ");

	sleep(1);
	disp_single_hzk16(100, 300, "��");	// 显示“我”字，在字库中的偏移，136928，用于测试
	disp_hzk16(100, 400, "��·��ƫ��ѩ������ʿɽ"); // 拦路雨偏似雪花，富士山
	disp_mix(100, 500, "Eason: ��·��ƫ��ѩ������ʿɽ"); // Eason:拦路雨偏似雪花，富士山

	disp_bmp(1000, 600, BMP_PATH);

	deinit();
	return 0;
}

