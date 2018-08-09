#
# Makefile for the project
# Copyright (c) 2017 Jiangsu Midea Clean Electric Appliances Co., Ltd.
#
# Author: Rocky Ching <dianhao.zheng@midea.com>

TARGET = dhcpc

CONFIG_HOST = n

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

CFLAGS = -I./include -O2 -Wall -Wno-unused-function
LIBS  := -lpthread -lm -lrt

ifeq ($(CONFIG_HOST), n)
CFLAGS += -muclibc -march=mips32r2
LDFLAG += -muclibc
endif

$(TARGET): $(OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS)
	$(STRIP) $@

# $(CPLUSPLUS) -c $(CFLAGS) -fpermissive -o $@ $<
%.o: %.c
	$(CPLUSPLUS) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

