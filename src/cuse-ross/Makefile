TARGET := cuse-ross

PREFIX ?= /usr/local

SRCS := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRCS:.c=.o)

LIBS := $(shell pkg-config rsound fuse --libs)
DEFINES := $(shell pkg-config rsound fuse --cflags)

CFLAGS += -Wall -O3 -g -std=gnu99 -pedantic

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DEFINES)

clean:
	rm -f $(TARGET) $(OBJS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

.PHONY: clean install uninstall
