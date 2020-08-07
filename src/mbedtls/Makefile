# Makefile for the project

TARGET = mbedtls_test

TOP_DIR := .

SRCDIRS := $(TOP_DIR)/user
SRCDIRS += $(TOP_DIR)/mbedtls-2.7.15/library

SRCFIXS  := .c


SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCFIXS))))
OBJS := $(patsubst %.c,%.o,$(SRCS))

# for debug
# $(warning source list $(SRCS))
# for debug
# $(warning objs list $(OBJS))

CC = gcc -g -Wall -fmax-errors=1

#-Werror 

CFLAGS = -I$(TOP_DIR)/user -I$(TOP_DIR)/mbedtls-2.7.15/include  -Wall -Wno-unused-function

LDFLAGS  := -lpthread -lrt -lm
EXTRA_FLAG := -c


$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_FLAG) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

