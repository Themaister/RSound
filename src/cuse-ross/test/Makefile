TARGET := test-oss

SRCS := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRCS:.c=.o)

LIBS :=
DEFINES :=

CFLAGS += -Wall -O3 -g -std=gnu99 -pedantic

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DEFINES)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
