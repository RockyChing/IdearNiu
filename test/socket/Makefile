TARGET = socket2_test

include ../../build/common.mk

SRCS += ./socket2_test.c
SRCS += ../../src/socket/socket2/socket_wrap.c
OBJS := $(patsubst %.c,%.o,$(SRCS))

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

