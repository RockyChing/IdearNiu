# Makefile for the project

TARGET = IdearNiu

SRCDIRS  := .
SRCDIRS  += $(shell ls -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
SRCFIXS  := .c

SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCFIXS))))
OBJS := $(patsubst %.c,%.o,$(SRCS))

# for debug
$(warning source list $(SRCS))
# for debug
$(warning objs list $(OBJS))

CC = gcc
CXX = g++
CFLAGS = -I./include -Wall -Wno-unused-function
LDFLAGS  := -lpthread -lrt
LDFLAGS += -lasound
LDFLAGS += -lsqlite3
EXTRA_FLAG := -c -DDEBUG_CHECK_PARAMETERS


$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_FLAG) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

