TARGET = libasound_module_pcm_rsound.so
PREFIX ?= /usr
DESTDIR =

OBJ = pcm_rsound.o

INCDIRS = -I$(PREFIX)/include/alsa
LIBS = -lrsound -lasound

all: $(TARGET)

%.o: %.c
	$(CC) -o $@ -c $< $(INCDIRS) $(CFLAGS) -std=gnu99 -Wall -O2 -g -pthread -fPIC -DPIC

$(TARGET): $(OBJ)
	$(CC) -shared -o $@ $< -Wl,-no-undefined -pthread $(LIBS) -fPIC

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/lib/alsa-lib
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/lib/alsa-lib

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/alsa-lib/$(TARGET)

clean:
	rm -f $(TARGET)
	rm -f $(OBJ)


.PHONY: clean install uninstall
