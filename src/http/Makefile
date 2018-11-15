# Makefile for the project

TARGET = wget
SRCTYPE = c

SRCDIRS := ./src ./lib

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
CC = gcc -g -fmax-errors=1
endif

CFLAGS = -I./lib -I./src  -Wall -Wno-unused-function
#CFLAGS += -DENABLE_DEBUG=1 -DHAVE_LIBSSL=1 -DHAVE_LOCALTIME_R=1 -DHAVE_LONG_LONG_INT=1
#CFLAGS += -DHAVE_MMAP=1 -DHAVE_SA_FAMILY_T=1

#CFLAGS += -DHAVE_OPENSSL_MD5=1 -DHAVE_OPENSSL_SHA1=1 -DHAVE_OPENSSL_SHA256=1 -DHAVE_OPENSSL_SHA512=1


LDFLAGS  := -lpthread -lrt -lssl -lcrypto
EXTRA_FLAG := -c


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

