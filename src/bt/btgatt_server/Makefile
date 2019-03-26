TARGET = btgatt-server

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
CROSS_COMPILE = /home/rocky/workspace/RK3308/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
endif

CC = $(CROSS_COMPILE)gcc
CPLUSPLUS = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar cr
STRIP = $(CROSS_COMPILE)strip

# -I/home/rocky/workspace/RK3308/buildroot/output/rockchip_rk3308_release/build/bluez5_utils-5.50/lib
CFLAGS = -I./include -I./lib -I. -O2 -Wall -Wno-unused-function
LIBS  := -lpthread -lrt

LDFLAG += -Wl,-gc-sections
ifeq ($(CONFIG_HOST), n)
LIBS += -L./lib -lbluetooth
endif

# -DMS_WIFI_ENABLE
CFLAGS += -DMIDEA_DEBUG -DCHECK_SDK -DOSTYPE_LINUX -DLINUX

$(TARGET): $(OBJS)
	$(CC) $(LDFLAG) -o $@ $^ $(LIBS)
	$(STRIP) $@

# $(CPLUSPLUS) -c $(CFLAGS) -fpermissive -o $@ $<
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

