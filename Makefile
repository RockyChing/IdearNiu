# Makefile for the project

TARGET = IdearNiu
SRCS := $(shell ls *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

# for debug
$(warning source list $(SRCS))
# for debug
$(warning objs list $(OBJS))

CC = gcc
CXX = g++
CFLAGS = -I -Wall
EXTRA_FLAG := -c -D_HOST_


$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_FLAG) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) *.o *.a *~

