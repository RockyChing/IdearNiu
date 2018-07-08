# Makefile for the project

TARGET = objthreads
SRCTYPE = cpp

SRCDIRS  := .
SRCDIRS  += $(shell ls -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
ifeq ($(SRCTYPE), cpp)
SRCFIXS  := .cpp
else
SRCFIXS  := .cpp
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
$(warning objs list $(OBJS))

CXX = g++
ifeq ($(SRCTYPE), cpp)
CC = g++
else
CC = gcc
endif

CFLAGS = -I./include -I./include/utils -I./include/system -I./include/netutils -I/usr/include/libnl3
CFLAGS += -Wall -Wno-unused-function
ifeq ($(SRCTYPE), cpp)
CFLAGS += -std=c++11
endif
LDFLAGS  := -lpthread -lrt
#LDFLAGS  += -lnl-3 -lnl-genl-3 -lnl-nf-3
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

