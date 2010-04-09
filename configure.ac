AC_INIT(RSound, 0.8, maister@archlinux.us)

AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE( alsa,       [  --disable-alsa          Disables alsa support], enable_alsa=no, enable_alsa=yes)
AC_ARG_ENABLE( oss,        [  --disable-oss           Disables oss support], enable_oss=no, enable_oss=yes)
AC_ARG_ENABLE( portaudio,  [  --disable-portaudio     Disables portaudio support], enable_porta=no, enable_porta=yes)
AC_ARG_ENABLE( libao,      [  --disable-libao         Disables libao support], enable_ao=no, enable_ao=yes)
AC_ARG_ENABLE( openal,     [  --disable-openal        Disables openal support], enable_al=no, enable_al=yes)

AC_PROG_CC
AM_PROG_CC_C_O

SERV_LIBS="-lpthread"

AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h unistd.h getopt.h])
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset select socket strerror getopt_long])

AC_CHECK_HEADER(sys/soundcard.h, ,enable_oss=no,)
AC_CHECK_HEADER(alsa/asoundlib.h, ,enable_alsa=no,)
AC_CHECK_HEADER(ao/ao.h, ,enable_ao=no,)
AC_CHECK_HEADER(portaudio.h, ,enable_porta=no,)
AC_CHECK_HEADER("AL/al.h", , enable_al=no,)
AC_CHECK_LIB(asound, snd_pcm_open, ,enable_alsa=no)
AC_CHECK_LIB(ao, ao_open_live, ,enable_ao=no)
AC_CHECK_LIB(portaudio, Pa_OpenStream, ,enable_porta=no)
AC_CHECK_LIB(openal, alcOpenDevice, ,enable_al=no)
AC_CHECK_LIB(pthread, pthread_create, , ,)
AC_CHECK_LIB(rt, clock_gettime, , ,)

AC_SUBST(SERV_LIBS)

AM_CONDITIONAL(HAVE_ALSA, test x$enable_alsa = xyes)
AM_CONDITIONAL(HAVE_OSS, test x$enable_oss = xyes)
AM_CONDITIONAL(HAVE_PORTA, test x$enable_porta = xyes)
AM_CONDITIONAL(HAVE_AO, test x$enable_ao = xyes)
AM_CONDITIONAL(HAVE_AL, test x$enable_al = xyes)

AM_PROG_LIBTOOL

AC_OUTPUT(Makefile)
