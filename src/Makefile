include config.mk

TARGET_SERVER = rsd
TARGET_CLIENT = rsdplay
TARGET_LIB = librsound/librsound.so
TARGET_LIB_OBJ = librsound/librsound.o

CFLAGS += -Wall

TARGET_SERVER_LIBS = -lpthread
TARGET_LIB_LIBS = -lpthread
OPT_SERV_LIBS =
OPT_SERV_OBJ =

ifeq ($(HAVE_ALSA), 1)
   OPT_SERV_LIBS += -lasound
   OPT_SERV_OBJ += alsa.o
endif

ifeq ($(HAVE_OSS), 1)
ifeq ($(HAVE_OSSAUDIO), 1)
   OPT_SERV_LIBS += -lossaudio
endif
   OPT_SERV_OBJ += oss.o
endif

ifeq ($(HAVE_LIBAO), 1)
   OPT_SERV_LIBS += -lao
   OPT_SERV_OBJ += ao.c
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

TARGET_CLIENT_OBJ = client.o endian.o $(TARGET_LIB_OBJ)

TARGET_SERVER_LIBS += $(OPT_SERV_LIBS)
TARGET_SERVER_OBJ = $(OPT_SERV_OBJ) audio.o endian.o daemon.o rsound-common.o proto.o

all: $(TARGET_LIB) $(TARGET_CLIENT) $(TARGET_SERVER)

$(TARGET_CLIENT): $(TARGET_CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(TARGET_CLIENT_OBJ) $(TARGET_LIB_LIBS)


$(TARGET_LIB): $(TARGET_LIB_OBJ)
	$(CC) -shared -o $(TARGET_LIB) $(TARGET_LIB_OBJ) $(TARGET_LIB_LIBS)


$(TARGET_SERVER): $(TARGET_SERVER_OBJ)
	$(CC) -o $(TARGET_SERVER) $(TARGET_SERVER_OBJ) $(TARGET_SERVER_LIBS)


librsound/librsound.o: librsound/librsound.c config.h
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

rsound-common.o: rsound-common.c config.h
	$(CC) $(CFLAGS) -c -o $@ $<
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf rsd 
	rm -rf rsdplay 
	rm -rf librsound/*.{o,so}
	rm -rf *.o

install: install-lib install-server install-client


install-lib: $(TARGET_LIB)
	install -m755 $(TARGET_LIB) $(PREFIX)/lib
	install -m644 librsound/rsound.h $(PREFIX)/include

install-server: $(TARGET_SERVER)
	install -m755 $(TARGET_SERVER) $(PREFIX)/bin

install-client: $(TARGET_CLIENT)
	install -m755 $(TARGET_CLIENT) $(PREFIX)/bin

uninstall:
	rm -rf $(PREFIX)/bin/rsd
	rm -rf $(PREFIX)/bin/rsdplay
	rm -rf $(PREFIX)/lib/librsound.so
	rm -rf $(PREFIX)/include/rsound.h


.PHONY: rsd rsdplay clean client lib server install install-lib install-server install-client
