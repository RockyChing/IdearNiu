#
# Makefile for the project
# Copyright (c) 2017 Jiangsu Midea Clean Electric Appliances Co., Ltd.
#
# Author: Rocky Ching <dianhao.zheng@midea.com>

TARGET = netlink_manager
CONFIG_HOST = y

SRCDIRS  := .
SRCDIRS  += $(shell ls -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
SRCFIXS  := .cpp

SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCFIXS))))
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

# for debug
$(warning source list $(SRCS))
# for debug
$(warning objs list $(OBJS))

ifeq ($(CONFIG_HOST), y)
CROSS_COMPILE =
else
CROSS_COMPILE = mips-linux-gnu-
endif

CC = $(CROSS_COMPILE)g++
CPLUSPLUS = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar cr
STRIP = $(CROSS_COMPILE)strip

CFLAGS = -I./include -O2 -Wall -Wno-unused-function
LIBS  := -lpthread -lm -lrt
#LIBS  += -L./lib -lnl-3 -lnl-genl-3
# LIBS += -lasound
# LIBS += -lsqlite3
LDFLAG += -Wl,-gc-sections
ifeq ($(CONFIG_HOST), n)
CFLAGS += -muclibc -march=mips32r2
LDFLAG += -muclibc
endif

CFLAGS += -std=c++11

# ./lib/libimp.a ./lib/libalog.a
$(TARGET): $(OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS)
	$(STRIP) $@

# $(CPLUSPLUS) -c $(CFLAGS) -fpermissive -o $@ $<
%.o: %.cpp
	$(CPLUSPLUS) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~
