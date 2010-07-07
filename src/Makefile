include config.mk

TARGET_SERVER = rsd
TARGET_CLIENT = rsdplay
TARGET_LIB = librsound/librsound.so.1.0.1
TARGET_LIB_OBJ = librsound/librsound.o
TARGET_LIB_OBJ_STATIC = librsound/librsound.a

PKGCONF_PATH = $(PREFIX)/lib/pkgconfig/rsound.pc

TARGET_SERVER_LIBS = -lpthread
TARGET_LIB_LIBS = -lpthread
OPT_SERV_LIBS =
OPT_SERV_OBJ =

ifeq ($(HAVE_ALSA), 1)
   OPT_SERV_LIBS += -lasound
   OPT_SERV_OBJ += alsa.o
endif

ifeq ($(HAVE_OSS), 1)
ifeq ($(NEED_OSSAUDIO), 1)
   OPT_SERV_LIBS += -lossaudio
endif
   OPT_SERV_OBJ += oss.o
endif

ifeq ($(HAVE_LIBAO), 1)
   OPT_SERV_LIBS += -lao
   OPT_SERV_OBJ += ao.o
endif

ifeq ($(HAVE_PULSE), 1)
   OPT_SERV_LIBS += -lpulse-simple
   OPT_SERV_OBJ += pulse.o
endif

ifeq ($(HAVE_MUROAR), 1)
   OPT_SERV_LIBS += -lmuroar
   OPT_SERV_OBJ += muroar.o
endif

ifeq ($(HAVE_AL), 1)
   OPT_SERV_LIBS += -lopenal
   OPT_SERV_OBJ += al.o
endif

ifeq ($(HAVE_PORTAUDIO), 1)
   OPT_SERV_LIBS += -lportaudio
   OPT_SERV_OBJ += porta.o
endif

ifeq ($(NEED_RT), 1)
   TARGET_SERVER_LIBS += -lrt
   TARGET_LIB_LIBS += -lrt
endif

TARGET_CLIENT_OBJ = client.o endian.o $(TARGET_LIB_OBJ_STATIC)

TARGET_SERVER_LIBS += $(OPT_SERV_LIBS)
TARGET_SERVER_OBJ = $(OPT_SERV_OBJ) audio.o endian.o daemon.o rsound-common.o proto.o

all: client lib server

client: $(TARGET_CLIENT)

lib: $(TARGET_LIB)

server: $(TARGET_SERVER)

$(TARGET_CLIENT): $(TARGET_CLIENT_OBJ)
	@echo "LD $@"
	@$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(TARGET_CLIENT_OBJ) $(TARGET_LIB_LIBS)


$(TARGET_LIB): $(TARGET_LIB_OBJ) $(TARGET_LIB_OBJ_STATIC)
	@echo "LD `echo $@ | sed 's|^\S\+/\([^.]\+\).*|\1.so|'`"
	@$(CC) -shared -Wl,-soname,librsound.so.1 -o $(TARGET_LIB) $(TARGET_LIB_OBJ) $(TARGET_LIB_LIBS)
	@ln -sf librsound.so.1.0.1 librsound/librsound.so.1
	@ln -sf librsound.so.1.0.1 librsound/librsound.so

$(TARGET_SERVER): $(TARGET_SERVER_OBJ)
	@echo "LD $@"
	@$(CC) -o $(TARGET_SERVER) $(TARGET_SERVER_OBJ) $(TARGET_SERVER_LIBS)


librsound/librsound.o: librsound/librsound.c config.h
	@echo "CC `echo $< | sed 's|^\S\+/\(\S\+\)|\1|'`"
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(TARGET_LIB_OBJ_STATIC): $(TARGET_LIB_OBJ)
	@echo "AR `echo $@ | sed 's|^\S\+/\([^.]\+\).*|\1.a|'`"
	@$(AR) rcs $(TARGET_LIB_OBJ_STATIC) $(TARGET_LIB_OBJ) >/dev/null 2>/dev/null



rsound-common.o: rsound-common.c config.h
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<
	
%.o: %.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf rsd 
	rm -rf rsdplay 
	rm -rf librsound/librsound.so*
	rm -rf librsound/librsound.o
	rm -rf librsound/librsound.a
	rm -rf *.o
	rm -rf rsound.pc

distclean: clean
	rm -rf config.h
	rm -rf config.mk

install: install-lib install-server install-client


install-lib: $(TARGET_LIB) $(TARGET_LIB_OBJ_STATIC) install-pkgconf
	mkdir -p $(DESTDIR)$(PREFIX)/{lib/pkgconfig,include,share/man/man1}
	install -m755 $(TARGET_LIB) $(DESTDIR)$(PREFIX)/lib
	cp -P librsound/librsound.so librsound/librsound.so.1 $(DESTDIR)$(PREFIX)/lib
	install -m644 $(TARGET_LIB_OBJ_STATIC) $(DESTDIR)$(PREFIX)/lib
	install -m644 librsound/rsound.h $(DESTDIR)$(PREFIX)/include
	install -m644 rsound.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m644 ../doc/rsd.1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m644 ../doc/rsdplay.1 $(DESTDIR)$(PREFIX)/share/man/man1

install-server: $(TARGET_SERVER)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m755 $(TARGET_SERVER) $(DESTDIR)$(PREFIX)/bin

install-client: $(TARGET_CLIENT)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m755 $(TARGET_CLIENT) $(DESTDIR)$(PREFIX)/bin

install-pkgconf:
	@echo "prefix=$(PREFIX)" > rsound.pc
	@echo "exec_prefix=\$${prefix}" >> rsound.pc
	@echo "libdir=\$${prefix}/lib" >> rsound.pc
	@echo "includedir=\$${prefix}/include" >> rsound.pc
	@echo "" >> rsound.pc
	@echo "Name: RSound" >> rsound.pc
	@echo "Description: An audio library designed for seamless networked transfer of audio streams." >> rsound.pc
	@echo "Requires:" >> rsound.pc
	@echo "Version: 0.9.2" >> rsound.pc
	@echo "Libs: -L\$${prefix}/lib -lrsound" >> rsound.pc
	@echo "Cflags: -I\$${prefix}/include" >> rsound.pc

uninstall:
	rm -rf $(PREFIX)/bin/rsd
	rm -rf $(PREFIX)/bin/rsdplay
	rm -rf $(PREFIX)/lib/librsound.*
	rm -rf $(PREFIX)/include/rsound.h
	rm -rf $(PREFIX)/lib/pkgconfig/rsound.pc
	rm -rf $(PREFIX)/share/man/man1/rsd.1
	rm -rf $(PREFIX)/share/man/man1/rsdplay.1


.PHONY: clean distclean client lib server install install-lib install-server install-client all uninstall
