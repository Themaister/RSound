TARGET := cuse-ross

SRCS := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRCS:.c=.o)

LIBS := $(shell pkg-config rsound fuse --libs)
DEFINES := $(shell pkg-config rsound fuse --cflags)

CFLAGS += -Wall -O3 -g -std=gnu99 -pedantic

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DEFINES)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
