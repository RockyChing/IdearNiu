# Makefile for the project

TARGET = IdearNiu
SRCTYPE = c

SRCDIRS := . ./src/util ./src/crypto ./src/netutils ./src/
SRCDIRS += ./src/socket ./src/thread
SRCDIRS += ./src/tinyalsa
SRCDIRS += ./src/dhcpc
#SRCDIRS  += $(shell ls -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
ifeq ($(SRCTYPE), cpp)
SRCFIXS  := .cpp
else
SRCFIXS  := .c
endif

SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCFIXS))))
ifeq ($(SRCTYPE), cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
else
OBJS := $(patsubst %.c,%.o,$(SRCS))
endif

# for debug
$(warning source list $(SRCS))
# for debug
# $(warning objs list $(OBJS))

CXX = g++
ifeq ($(SRCTYPE), cpp)
CC = g++
else
CC = gcc
endif

CFLAGS = -I./include -Wall -Wno-unused-function
LDFLAGS  := -lpthread -lrt
LDFLAGS += -lasound
LDFLAGS += -lsqlite3
EXTRA_FLAG := -c -DDEBUG_CHECK_PARAMETERS


$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

ifeq ($(SRCTYPE), cpp)
%.o: %.cpp
	$(CC) $(CFLAGS) $(EXTRA_FLAG) -o $@ $<
else
%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_FLAG) -o $@ $<
endif

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

