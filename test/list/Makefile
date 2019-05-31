TARGET = list_test

include ../../build/common.mk

SRCS += ./list_test.c
SRCS += ../../src/thread/mutex.c
OBJS := $(patsubst %.c,%.o,$(SRCS))

LIBS  := -lpthread -lrt

# for debug
$(warning source list $(SRCS))
# for debug
# $(warning objs list $(OBJS))

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(STRIP) $@

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS) *.a *~

