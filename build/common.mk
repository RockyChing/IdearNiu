CURDIR:=$(shell pwd)




SRCS := $(TOPDIR)/src/util/utils.c
SRCS += $(TOPDIR)/src/util/log_util.c

CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)gcc
LD		= $(CROSS_COMPILE)ld
AR		= $(CROSS_COMPILE)ar cr
STRIP	= $(CROSS_COMPILE)strip

CFLAGS	= -Wall -Wno-unused-function -fmax-errors=1
LDFLAGS	= -Wl,-gc-sections
LIBS	:= -lrt
