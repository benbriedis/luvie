CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -fPIC -O2 \
          -I../../deps/sources/lv2/include
LDFLAGS = -shared -fPIC -lm

TARGET  = luvie_dsp.so
SRCS    = luvie_dsp.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
