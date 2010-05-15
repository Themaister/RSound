bin_PROGRAMS = src/rsdplay src/rsd
AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4


lib_LTLIBRARIES = src/librsound/librsound.la
src_librsound_librsound_la_SOURCES = src/librsound/librsound.c 
src_librsound_librsound_la_LDFLAGS = -version-info 1:1 -lpthread -lrt
src_librsound_librsound_includedir = $(includedir)
src_librsound_librsound_include_HEADERS = src/librsound/rsound.h

if HAVE_DECNET
   src_librsound_librsound_la_LDFLAGS += -ldnet
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rsound.pc

sources_serv = 
AM_CFLAGS = -Wall -Wextra -O2

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

if HAVE_AL
   sources_serv += src/al.c
   sources_serv += src/al.h
   AM_CFLAGS += -D_AL
   SERV_LIBS += -lopenal
endif

if HAVE_PULSE
   sources_serv += src/pulse.c
   sources_serv += src/pulse.h
   AM_CFLAGS += -D_PULSE
   SERV_LIBS += -lpulse-simple 
endif

if HAVE_MUROAR
   sources_serv += src/muroar.c
   sources_serv += src/muroar-internal.h
   AM_CFLAGS += -D_MUROAR
   SERV_LIBS += -lmuroar
endif

src_rsdplay_SOURCES = src/client.c \
                     src/librsound/rsound.h

src_rsd_SOURCES =   src/daemon.c \
                     src/proto.h \
                     src/proto.c \
                           src/endian.h \
                           src/rsound-common.c \
                           src/rsound.h \
                           src/audio.h $(sources_serv)

src_rsd_LDADD = $(SERV_LIBS)
src_rsdplay_LDADD = -lpthread -lrt -lrsound

### Docs ###
man_MANS = doc/rsd.1 doc/rsdplay.1
