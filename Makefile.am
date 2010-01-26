bin_PROGRAMS = src/rsound src/rsoundserv

sources_serv = 
AM_CFLAGS =

if HAVE_ALSA
   sources_serv += src/alsa.c
   AM_CFLAGS += -D_ALSA
   SERV_LIBS += -lasound
endif

if HAVE_AO
   sources_serv += src/ao.c
   AM_CFLAGS += -D_AO
   SERV_LIBS += -lao
endif

if HAVE_PORTA
   sources_serv += src/porta.c
   AM_CFLAGS += -D_PORTA
   SERV_LIBS += -lportaudio
endif

if HAVE_OSS
   sources_serv += src/oss.c
   AM_CFLAGS += -D_OSS
endif

src_rsound_SOURCES = src/client.c src/endian.c
src_rsoundserv_SOURCES = src/daemon.c src/endian.c src/rsound-common.c $(sources_serv)
src_rsoundserv_LDADD = $(SERV_LIBS)

