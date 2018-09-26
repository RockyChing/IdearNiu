#
# Makefile for the project
# Copyright (c) 2017 Jiangsu Midea Clean Electric Appliances Co., Ltd.
#
# Author: Rocky Ching <dianhao.zheng@midea.com>

TARGET = madplay

CONFIG_HOST = y

SRCDIRS  := .
SRCDIRS  += $(shell ls -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
SRCFIXS  := .c

SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCFIXS))))
OBJS := $(patsubst %.c,%.o,$(SRCS))

# for debug
# $(warning source list $(SRCS))
# for debug
# $(warning objs list $(OBJS))

ifeq ($(CONFIG_HOST), y)
CROSS_COMPILE =
else
CROSS_COMPILE = mips-linux-gnu-
endif

CC = $(CROSS_COMPILE)gcc
CPLUSPLUS = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar cr
STRIP = $(CROSS_COMPILE)strip

CFLAGS = -O2 -Wall -Wno-unused-function
CFLAGS += -I./madplay-0.15.2b -I./libmad-0.15.1b -I./libid3tag-0.15.1b
LIBS  := -lm -lrt -lz -lmad -lasound

LDFLAG += -Wl,-gc-sections
ifeq ($(CONFIG_HOST), n)
CFLAGS += -muclibc -march=mips32r2
LDFLAG += -muclibc
endif

CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -DDEBUG

$(TARGET): $(OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS)
	$(STRIP) $@

# $(CPLUSPLUS) -c $(CFLAGS) -fpermissive -o $@ $<
%.o: %.c
	$(CPLUSPLUS) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~
