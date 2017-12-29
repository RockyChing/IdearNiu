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

// B G R A
#define WHITE (0xffffffff)
#define RED   (0x0000ff00)
#define GREEN (0x00ff0000)
#define BLUE  (0xff000000)
#define BLACK (0x00000000)

#define FBDEV "/dev/fb0"



#if 1
/* 一些全局变量的定义 */
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

	/* 获取LCD液晶的可变参数 */
	ret = ioctl(fd_fb, FBIOGET_VSCREENINFO, &var);
	if(ret == -1) {
		printf("can't ioctl for /dev/fb0!\n");
		return -1;
	}

	/* 获取LCD液晶的固定参数 */
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

	/* 将汉子库HZK16文件中的内容映射到用户空间 */
	hzk16mem = mmap(NULL, hzk16_stat.st_size, PROT_READ, MAP_SHARED, fd_hzk16, 0);
	if(hzk16mem == (char *)-1) {
		printf("mmap for HZK16 error!\n");
		return -1;
	}

	return 0;
}

/*	LCD液晶清屏
 *	color : 表示要将屏幕成的颜色
 */
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

/*	LCD像素点显示
 *	x : 表示x轴的坐标
 *	y : 表示y轴的坐标
 *	color : 表示像素点要显示的颜色
 */
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

/*	LCD显示字符函数
 *	x : 表示要显示的字符的x坐标
 *	y : 表示要显示的字符的y坐标
 *	c : 表示要显示的字符
 */
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

/*	LCD液晶显示单个汉字
 *	x : 表示x轴的坐标
 *	y : 表示y轴的坐标
 *	str : 表示要显示的汉子的字符编码
 */
static void disp_single_hzk16(int x, int y, char *str)
{
	/* 确定汉字在字符中的位置 */
	int area = str[0] - 0xa0 -1 ;
	int where = str[1] - 0xa0 - 1;
	int offset = (area * 94 + where) * 32;
	__u8 buffer[32] = {
		0x04 ,0x80 ,0x0E ,0xA0 ,0x78 ,0x90 ,0x08 ,0x90,
		0x08 ,0x84 ,0xFF ,0xFE ,0x08 ,0x80 ,0x08 ,0x90,
		0x0A ,0x90 ,0x0C ,0x60 ,0x18 ,0x40 ,0x68 ,0xA0,
		0x09 ,0x20 ,0x0A ,0x14 ,0x28 ,0x14 ,0x10 ,0x0C
	}; //(__u8 *) (hzk16mem + offset);
	__u16 data;
	int i, j;

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

/*	LCD显示中文字符串
 *	x : 表示x轴坐标
 *	y : 表示y轴坐标
 *	str : 表示要显示的汉子字符编码的首地址
 */
static void disp_hzk16(int x, int y, char *str)
{
	assert(str);
	//assert(x < (SCREEN_WIDTH - 16));
	//assert(y < (SCREEN_HEIGHT - 16));
	while (str) {
		disp_single_hzk16(x, y, *str);
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

int main(int argc, char **argv)
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
	disp_single_hzk16(100, 300, "人");
	deinit();
	return 0;
}

#else
typedef struct point {
	int x;
	int y;
} pos_t;

unsigned char font_9[] = {
	/* 57 0x39 '9' */
	0x00, /* 00000000 */
	0x00, /* 00000000 */
	0x7c, /* 01111100 */
	0xc6, /* 11000110 */
	0xc6, /* 11000110 */
	0xc6, /* 11000110 */
	0x7e, /* 01111110 */
	0x06, /* 00000110 */
	0x06, /* 00000110 */
	0x06, /* 00000110 */
	0x0c, /* 00001100 */
	0x78, /* 01111000 */
	0x00, /* 00000000 */
	0x00, /* 00000000 */
	0x00, /* 00000000 */
	0x00, /* 00000000 */
};
/**
 *
 *
 */
static void disp_ascii(pos_t *pos, unsigned char *map, unsigned char ch)
{
	const int bytes_per_pixel = 4;
	const int line_length = 1280;
	int location = 0;
	int x, y;
	for(x = 0; x < 8; x ++) {
		for(y = 0; y < 16; y++) {
			location += x * bytes_per_pixel + y * line_length;
			if (font_9[y] & (0x80 >> x)) {
				*(map + location + 0) = 0x00;// 蓝色的色深
				*(map + location + 1) = 0x00;// 绿色的色深
				*(map + location + 2) = 0xFF;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			} else {
				*(map + location + 0) = 0x00;// 蓝色的色深
				*(map + location + 1) = 0x00;// 绿色的色深
				*(map + location + 2) = 0x00;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			}
		}
	}
}

int setmode(int fbd, struct fb_var_screeninfo *var)
{
	int stat;
	var->xres= 1280;
	var->xres_virtual = 1280;
	var->yres= 800;
	var->yres_virtual = 800;
	var->bits_per_pixel = 32;

	stat = ioctl (fbd, FBIOPUT_VSCREENINFO,&var);
	if (stat<0) {
		printf("Error of FBIOPUT_VSCREENINFO\n");
		return -1;
	}
	return 0;
}

#define SIZE (1280*800*4+54)

int display(char *framebuffer_devices)
{
	int fp = 0;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	int screensize = 0;
	char *fbp = 0;
	int x = 0, y = 0;
	int location = 0;
	int bytes_per_pixel;// 组成每一个像素点的字节数

	int count = 0;
	FILE *file_fd = NULL;
	file_fd = fopen("/usr/sbin/1.bmp", "r");
	char buffer[SIZE];  // 1280*800*3+54=
	int readCnt = fread(buffer, sizeof(buffer), 1, file_fd);
	printf("readCnt = %d\n", readCnt);
	fclose(file_fd);

	fp = open(framebuffer_devices, O_RDWR);
	if(fp < 0) {
		printf("error: Can not open %s device!!!!!!!!!!!!!!!!!!!!!!!!!!\n", framebuffer_devices);
		return -1;
	}

	if(ioctl(fp, FBIOGET_FSCREENINFO, &finfo)) {
		printf("err FBIOGET_FSCREENINFO!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		close(fp);
		return -2;
	}

	if(ioctl(fp, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("err FBIOGET_VSCREENINFO!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		close(fp);
		return -3;
	}

	setmode(fp, &vinfo);
	bytes_per_pixel = vinfo.bits_per_pixel >> 3;
	screensize = vinfo.xres * vinfo.yres * bytes_per_pixel;
	printf("x = %d y = %d bytes_per_pixel = %d\n", vinfo.xres, vinfo.yres, bytes_per_pixel);
	printf("screensize = %d\n", screensize);
	fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
	if((int) fbp == -1)	{
		printf("err mmap!!!!!!!!!!!!!!!!!!!!!!!!!!");
		close(fp);
		return -4;
	}

	printf("----- clear\n");
	for(x = 0; x < vinfo.xres; x ++) {
		for(y = 0; y < vinfo.yres; y++) {
			location = x * bytes_per_pixel + y * finfo.line_length;
			*(fbp + location + 0) = 0xff;// 蓝色的色深
			*(fbp + location + 1) = 0xff;// 绿色的色深
			*(fbp + location + 2) = 0xff;// 红色的色深
			//*(fbp + location + 3) = 0;// 是否透明
		}
	}

	printf("----- 9\n");
	const int w = 8;
	const int h = 16;
	const int index = '9' * 16;

	for(x = 0; x < w; x ++) {
		for(y = 0; y < h; y++) {
			location = x * bytes_per_pixel + y * finfo.line_length;
			if (fontdata_8x16[index + y] & (0x80 >> x)) {
				*(fbp + location + 0) = 0x00;// 蓝色的色深
				*(fbp + location + 1) = 0x00;// 绿色的色深
				*(fbp + location + 2) = 0xFF;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			} else {
				*(fbp + location + 0) = 0x00;// 蓝色的色深
				*(fbp + location + 1) = 0x00;// 绿色的色深
				*(fbp + location + 2) = 0x00;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			}
		}
	}

	for(count = 0; count < 5; count ++) {
#if 0

		printf("----- RED\n");
		for(x = 0; x < vinfo.xres; x ++) {
			for(y = 0; y < vinfo.yres; y++) {
				location = x * bytes_per_pixel + y * finfo.line_length;
				*(fbp + location + 0) = 0x00;// 蓝色的色深
				*(fbp + location + 1) = 0x00;// 绿色的色深
				*(fbp + location + 2) = 0xFF;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			}
		}

		sleep(1);
		printf("----- GREEN");
		for(x = 0; x < vinfo.xres; x ++) {
			for(y = 0; y < vinfo.yres; y++) {
				location = x * bytes_per_pixel + y * finfo.line_length;
				*(fbp + location + 0) = 0x00;// 蓝色的色深
				*(fbp + location + 1) = 0xFF;// 绿色的色深
				*(fbp + location + 2) = 0x00;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			}
		}

		sleep(1);
		puts("----- BLUE");
		for(x = 0; x < vinfo.xres; x ++) {
			for(y = 0; y < vinfo.yres; y++) {
				location = x * bytes_per_pixel + y * finfo.line_length;
				*(fbp + location + 0) = 0xFF;// 蓝色的色深
				*(fbp + location + 1) = 0x00;// 绿色的色深
				*(fbp + location + 2) = 0x00;// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			}
		}
#endif
#if 0
		sleep(1);
		for(x = 0; x < vinfo.xres; x ++) {
			for(y = 0; y < vinfo.yres; y++) {
				location = x * bytes_per_pixel + y * finfo.line_length;

				// 2017/7/3 11:25 矫正BMP显示和LCD显示的上下反 799-y 
				*(fbp + location + 0) = buffer[54+(1280*(799-y)+x)*3+0];// 蓝色的色深
				*(fbp + location + 1) = buffer[54+(1280*(799-y)+x)*3+1];// 绿色的色深
				*(fbp + location + 2) = buffer[54+(1280*(799-y)+x)*3+2];// 红色的色深
				//*(fbp + location + 3) = 0;// 是否透明
			}
		}
#endif
		sleep(3);
	}

	munmap(fbp, screensize);
	close(fp);

	return 0;
}


int main(void)
{
	char fb_path[30] = {0};
	int i = 0;

	system("echo on > /sys/power/state");
	printf("lcd test starting...\n");
	display(FBDEV);

#if 0
	for(i = 0; i < 100; i ++)
	{
		memset(fb_path, 0, sizeof(fb_path));
		sprintf(fb_path, "/dev/fb%d", i);
		printf("show %s...\n", fb_path);
		if(-1 == Show(fb_path))
			break;
		printf("%s ok\n\n", fb_path);
	}
#endif

	return 0;
}
#endif

