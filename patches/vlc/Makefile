TARGET_LIB = librsound_plugin.so
TARGET_OBJ = rsound-plugin.o

PREFIX ?= /usr

VLC_CFLAGS = $(shell pkg-config vlc-plugin --cflags)
VLC_LIBS = $(shell pkg-config vlc-plugin --libs)
VLC_DEFINES = -DMODULE_STRING=\"RSound\"

ifeq ($(VLC_CFLAGS),)
   $(error "Cannot find VLC plugin pkg-conf...")
endif
ifeq ($(VLC_LIBS),)
	$(error "Cannot find VLC plugin pkg-conf...")
endif

all: $(TARGET_LIB)

$(TARGET_LIB): $(TARGET_OBJ)
	$(CC) -o $@ -shared -fPIC $< -lrsound $(VLC_LIBS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) -fPIC -O2 -g -std=gnu99 $(VLC_CFLAGS) $(VLC_DEFINES)

clean:
	rm -f *.so
	rm -f *.o

install:
	mkdir -p $(DESTDIR)$(PREFIX)/lib/vlc/plugins/audio_output
	install -m755 $(TARGET_LIB) $(DESTDIR)$(PREFIX)/lib/vlc/plugins/audio_output

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/vlc/plugins/audio_output/$(TARGET_LIB)

.PHONY: clean install uninstall
