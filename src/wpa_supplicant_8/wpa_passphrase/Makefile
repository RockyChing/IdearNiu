#
# Makefile for the project
#
# Author: Rocky Ching <zdh1021680772@163.com>

TARGET = wpa_passphrase

CONFIG_HOST = y


SRCDIRS  := . ../src/crypto
SRCDIRS  += $(shell ls -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
SRCFIXS  := .c

SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCFIXS))))
OBJS := $(patsubst %.c,%.o,$(SRCS))

# for debug
$(warning source list $(SRCS))
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

CFLAGS = -I../src/crypto/ -O2 -Wall -Wno-unused-function
LIBS  := -lpthread -lm -lrt
# LIBS += -lasound
# LIBS += -lsqlite3
LDFLAG += -Wl,-gc-sections
ifeq ($(CONFIG_HOST), n)
CFLAGS += -muclibc -march=mips32r2
LDFLAG += -muclibc
endif

CFLAGS += -I$(abspath ./src)
CFLAGS += -I$(abspath ./src/utils)

CFLAGS += -DCONFIG_CRYPTO_INTERNAL

$(TARGET): $(OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS)
	$(STRIP) $@

# $(CPLUSPLUS) -c $(CFLAGS) -fpermissive -o $@ $<
%.o: %.c
	$(CPLUSPLUS) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

