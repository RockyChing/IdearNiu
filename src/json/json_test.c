#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils.h>
#include "cJSON.h"

/**
 ********** Refer to https://github.com/DaveGamble/cJSON *********
 */
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (-1)
#endif

/* Used by some code below as an example datatype. */
struct record
{
	const char *precision;
	double lat;
	double lon;
	const char *address;
	const char *city;
	const char *state;
	const char *zip;
	const char *country;
};

/* Create a bunch of objects as demonstration. */
static int print_preallocated(cJSON *root)
{
	/* declarations */
	char *out = NULL;
	char *buf = NULL;
	char *buf_fail = NULL;
	size_t len = 0;
	size_t len_fail = 0;

	/* formatted print */
	out = cJSON_Print(root);

	/* create buffer to succeed */
	/* the extra 5 bytes are because of inaccuracies when reserving memory */
	len = strlen(out) + 5;
	buf = (char*) xmalloc(len);
	if (buf == NULL) {
		printf("Failed to allocate memory.\n");
		exit(1);
	}

	/* create buffer to fail */
	len_fail = strlen(out);
	buf_fail = (char*) xmalloc(len_fail);
	if (buf_fail == NULL) {
		printf("Failed to allocate memory.\n");
		exit(1);
	}

	/* Print to buffer */
	if (!cJSON_PrintPreallocated(root, buf, (int)len, 1)) {
		printf("cJSON_PrintPreallocated failed!\n");
		if (strcmp(out, buf) != 0) {
			printf("cJSON_PrintPreallocated not the same as cJSON_Print!\n");
			printf("cJSON_Print result:\n%s\n", out);
			printf("cJSON_PrintPreallocated result:\n%s\n", buf);
		}
		free(out);
		free(buf_fail);
		free(buf);
		return -1;
	}

	/* success */
	printf("%s\n", buf);

	/* force it to fail */
	if (cJSON_PrintPreallocated(root, buf_fail, (int)len_fail, 1)) {
		printf("cJSON_PrintPreallocated failed to show error with insufficient memory!\n");
		printf("cJSON_Print result:\n%s\n", out);
		printf("cJSON_PrintPreallocated result:\n%s\n", buf_fail);
		free(out);
		free(buf_fail);
		free(buf);
		return -1;
	}

	free(out);
	free(buf_fail);
	free(buf);
	return 0;
}

/* Create a bunch of objects as demonstration. */
static void create_objects(void)
{
	/* declare a few. */
	cJSON *root = NULL;
	cJSON *fmt = NULL;
	cJSON *img = NULL;
	cJSON *thm = NULL;
	cJSON *fld = NULL;
	int i = 0;

	/* Our "days of the week" array: */
	const char *strings[7] =
	{
		"Sunday",
		"Monday",
		"Tuesday",
		"Wednesday",
		"Thursday",
		"Friday",
		"Saturday"
	};
	/* Our matrix: */
	int numbers[3][3] =
	{
		{ 0, -1, 0 },
		{ 1, 0, 0 },
		{ 0 ,0, 1 }
	};
	/* Our "gallery" item: */
	int ids[4] = { 116, 943, 234, 38793 };
	/* Our array of "records": */
	struct record fields[2] =
	{
		{
			"zip",
			37.7668,
			-1.223959e+2,
			"",
			"SAN FRANCISCO",
			"CA",
			"94107",
			"US"
			},
		{
			"zip",
			37.371991,
			-1.22026e+2,
			"",
			"SUNNYVALE",
			"CA",
			"94085",
			"US"
		}
	};
	volatile double zero = 0.0;
	char *to_strings;

	/* Here we construct some JSON standards, from the JSON site. */

	/* Our "Video" datatype: */
    printf("Our \"Video\" datatype\n");
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "name", cJSON_CreateString("Jack (\"Bee\") Nimble"));
	cJSON_AddItemToObject(root, "format", fmt = cJSON_CreateObject());
	cJSON_AddStringToObject(fmt, "type", "rect");
	cJSON_AddNumberToObject(fmt, "width", 1920);
	cJSON_AddNumberToObject(fmt, "height", 1080);
	cJSON_AddFalseToObject(fmt, "interlace");
	cJSON_AddNumberToObject(fmt, "frame rate", 24);
	to_strings = cJSON_PrintUnformatted(root);
	printf("to_strings: %s\n", to_strings == NULL ? "error!!!" : to_strings);

	/* Print to text */
	if (print_preallocated(root) != 0) {
		cJSON_Delete(root);
		exit(EXIT_FAILURE);
	}
	cJSON_Delete(root);

	/* Our "days of the week" array: */
    printf("Our \"days of the week\" array:\n");
	root = cJSON_CreateStringArray(strings, 7);

	if (print_preallocated(root) != 0) {
		cJSON_Delete(root);
		exit(EXIT_FAILURE);
	}
	cJSON_Delete(root);

	/* Our matrix: */
    printf("Our matrix:\n");
	root = cJSON_CreateArray();
	for (i = 0; i < 3; i++)
	{
		cJSON_AddItemToArray(root, cJSON_CreateIntArray(numbers[i], 3));
	}

	/* cJSON_ReplaceItemInArray(root, 1, cJSON_CreateString("Replacement")); */
	if (print_preallocated(root) != 0) {
		cJSON_Delete(root);
		exit(EXIT_FAILURE);
	}
	cJSON_Delete(root);

	/* Our "gallery" item: */
    printf("Our \"gallery\" item:\n");
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "Image", img = cJSON_CreateObject());
	cJSON_AddNumberToObject(img, "Width", 800);
	cJSON_AddNumberToObject(img, "Height", 600);
	cJSON_AddStringToObject(img, "Title", "View from 15th Floor");
	cJSON_AddItemToObject(img, "Thumbnail", thm = cJSON_CreateObject());
	cJSON_AddStringToObject(thm, "Url", "http:/*www.example.com/image/481989943");
	cJSON_AddNumberToObject(thm, "Height", 125);
	cJSON_AddStringToObject(thm, "Width", "100");
	cJSON_AddItemToObject(img, "IDs", cJSON_CreateIntArray(ids, 4));

	if (print_preallocated(root) != 0) {
		cJSON_Delete(root);
		exit(EXIT_FAILURE);
	}
	cJSON_Delete(root);

	/* Our array of "records": */
    printf("Our array of \"records\":\n");
	root = cJSON_CreateArray();
	for (i = 0; i < 2; i++) {
		cJSON_AddItemToArray(root, fld = cJSON_CreateObject());
		cJSON_AddStringToObject(fld, "precision", fields[i].precision);
		cJSON_AddNumberToObject(fld, "Latitude", fields[i].lat);
		cJSON_AddNumberToObject(fld, "Longitude", fields[i].lon);
		cJSON_AddStringToObject(fld, "Address", fields[i].address);
		cJSON_AddStringToObject(fld, "City", fields[i].city);
		cJSON_AddStringToObject(fld, "State", fields[i].state);
		cJSON_AddStringToObject(fld, "Zip", fields[i].zip);
		cJSON_AddStringToObject(fld, "Country", fields[i].country);
	}

	/* cJSON_ReplaceItemInObject(cJSON_GetArrayItem(root, 1), "City", cJSON_CreateIntArray(ids, 4)); */

	if (print_preallocated(root) != 0) {
		cJSON_Delete(root);
		exit(EXIT_FAILURE);
	}
	cJSON_Delete(root);

	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "number", 1.0 / zero);

	if (print_preallocated(root) != 0) {
		cJSON_Delete(root);
		exit(EXIT_FAILURE);
	}
	cJSON_Delete(root);
}

static void parse_json1()
{
    cJSON *item = NULL;
    cJSON *found = NULL;

    item = cJSON_Parse("{\"one\":1, \"Two\":2, \"tHree\":3}");

    found = cJSON_GetObjectItem(NULL, "test");
    printf("test(NULL is OK), got: %s\n", found == NULL ? "NULL pointer." : "found");

    found = cJSON_GetObjectItem(item, NULL);
	printf("(NULL is OK), got: %s\n", found == NULL ? "NULL pointer." : "found");


    found = cJSON_GetObjectItem(item, "one");
	if (found) {
		printf("one valueint: %d\n", found->valueint);
		printf("one valuedouble: %f\n", found->valuedouble);
	} else {
		printf("one: Failed to find first item.\n");
	}
    
    found = cJSON_GetObjectItem(item, "tWo");
	if (found) {
		printf("tWo valueint: %d\n", found->valueint);
		printf("tWo valuedouble: %f\n", found->valuedouble);
	} else {
		printf("Two: Failed to find tWo item.\n");
	}

    found = cJSON_GetObjectItem(item, "three");
	if (found) {
		printf("three valueint: %d\n", found->valueint);
		printf("three valuedouble: %f\n", found->valuedouble);
	} else {
		printf("three: Failed to find three item.\n");
	}

    found = cJSON_GetObjectItem(item, "four");
	if (found) {
		printf("four valueint: %d\n", found->valueint);
		printf("four valuedouble: %f\n", found->valuedouble);
	} else {
		printf("four: (NULL is OK)\n");
	}

    cJSON_Delete(item);
	printf("%s return.\n\n\n", __FUNCTION__);
}

static void parse_json2()
{
	cJSON *root = NULL;
	cJSON *found = NULL;
	char buf[255] = {0};
	const char *json_string = "{\"name\":\"Jack Nimble\", \"format\":{\"type\":\"rect\",\"width\":1920,\"height\":1080,\"interlace\":false,\"frame rate\":24}}";
	root = cJSON_Parse(json_string);
	found = cJSON_GetObjectItem(root, "name");
	if (cJSON_IsString(found)) {
		snprintf(buf, 255, "%s", found->valuestring);//把fmt指向的JSON节点的字符串复制到name数组里来。
		printf("name valuestring: %s\n", buf);
	} else {
		printf("name: Failed to find name item.\n");
	}

	cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "format");
	cJSON *framerate_item = cJSON_GetObjectItemCaseSensitive(format, "frame rate");
	if (cJSON_IsNumber(framerate_item)) {
		printf("framerate: %d\n", framerate_item->valueint);
	}

	cJSON *width_item = cJSON_GetObjectItemCaseSensitive(format, "width");
	if (cJSON_IsNumber(width_item)) {
		printf("width_item: %d\n", width_item->valueint);
	}

	cJSON_Delete(root);
	printf("%s return.\n\n\n", __FUNCTION__);
}

void json_test_entry(void)
{
	const char *data = "\xEF\xBB\xBF";
	printf("%x\n", (unsigned char) data[0]);
	printf("%x\n", (unsigned char)data[1]);
	printf("%x\n", (unsigned char)data[2]);
	/* Now some samplecode for building objects concisely: */
	create_objects();

    parse_json1();
	parse_json2();
	//parse_json3();
}

