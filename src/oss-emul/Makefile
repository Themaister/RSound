TARGET = librsoundoss.so
SOURCE = librsoundoss.c
SCRIPT = ross

all: librsoundoss.c
	$(CC) $(CFLAGS) -g -O2 -Wall -lrsound -ldl -fPIC -shared -o $(TARGET) $(SOURCE)

install: all
	install -m755 $(TARGET) /usr/lib
	install -m755 $(SCRIPT) /usr/bin 
