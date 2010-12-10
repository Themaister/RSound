TARGET_LIB = rsound.so
TARGET_OBJ = rsound-plugin.o

PREFIX ?= /usr/local

all: $(TARGET_LIB)

$(TARGET_LIB): $(TARGET_OBJ)
	$(CC) -o $@ -shared -fPIC $< -lrsound

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) -fPIC -O2 -g -std=gnu99 -I$(PREFIX)/include/deadbeef

clean:
	rm -f *.so
	rm -f *.o

install:
	install -m755 $(TARGET_LIB) $(PREFIX)/lib/deadbeef

uninstall:
	rm -f $(PREFIX)/lib/deadbeef/$(TARGET_LIB)

.PHONY: clean install uninstall
