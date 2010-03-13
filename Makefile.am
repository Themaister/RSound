bin_PROGRAMS = src/rsound src/rsoundserv
AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

rsounddir = @RSOUNDDIR@
rsound_LTLIBRARIES = src/librsound/librsound.la
src_librsound_librsound_la_SOURCES = src/librsound/librsound.c src/librsound/rsound.h
src_librsound_librsound_la_CFLAGS = -fPIC

sources_serv = 
AM_CFLAGS = -Wall -O2

if HAVE_ALSA
   sources_serv += src/alsa.c
   sources_serv += src/alsa.h
   AM_CFLAGS += -D_ALSA
   SERV_LIBS += -lasound
endif

if HAVE_AO
   sources_serv += src/ao.c
   sources_serv += src/ao.h
   AM_CFLAGS += -D_AO
   SERV_LIBS += -lao
endif

if HAVE_PORTA
   sources_serv += src/porta.c
   sources_serv += src/porta.h
   AM_CFLAGS += -D_PORTA
   SERV_LIBS += -lportaudio
endif

if HAVE_OSS
   sources_serv += src/oss.c
   sources_serv += src/oss.h
   AM_CFLAGS += -D_OSS
endif

src_rsound_SOURCES = src/client.c \
                     src/librsound/rsound.h

src_rsoundserv_SOURCES =   src/daemon.c \
                           src/endian.c \
                           src/endian.h \
                           src/rsound-common.c \
                           src/rsound.h \
                           src/audio.h $(sources_serv)

src_rsoundserv_LDADD = $(SERV_LIBS)
src_rsound_LDADD = -lpthread -lrt src/librsound/librsound.la

