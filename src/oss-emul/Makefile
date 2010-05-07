TARGET = librsoundoss.so
SOURCE = librsoundoss.c

all: librsoundoss.c
	$(CC) $(CFLAGS) -g -O2 -Wall -lrsound -fPIC -shared -o $(TARGET) $(SOURCE)

install: all
	install -m755 $(TARGET) /usr/lib
