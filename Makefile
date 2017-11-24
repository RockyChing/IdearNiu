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
CFLAGS = -I./include -Wall
LDFLAGS  := -lpthread -lrt
EXTRA_FLAG := -c -D_HOST_


$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_FLAG) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) *.o *.a *~

