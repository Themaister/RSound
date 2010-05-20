AC_INIT(RSound, 0.9, maister@archlinux.us)

AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/daemon.c])
AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER(src/config.h)

AC_ARG_ENABLE( alsa,       [  --disable-alsa          Disables alsa support], enable_alsa=no, enable_alsa=yes)
AC_ARG_ENABLE( oss,        [  --disable-oss           Disables oss support], enable_oss=no, enable_oss=yes)
AC_ARG_ENABLE( pulse,      [  --disable-pulse         Disables pulse support], enable_pulse=no, enable_pulse=yes)
AC_ARG_ENABLE( portaudio,  [  --disable-portaudio     Disables portaudio support], enable_porta=no, enable_porta=yes)
AC_ARG_ENABLE( libao,      [  --disable-libao         Disables libao support], enable_ao=no, enable_ao=yes)
AC_ARG_ENABLE( openal,     [  --disable-openal        Disables openal support], enable_al=no, enable_al=yes)
AC_ARG_ENABLE( muroar,     [  --disable-muroar        Disables muroar support], enable_muroar=no, enable_muroar=yes)

AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h unistd.h getopt.h netdnet/dn.h netdnet/dnetdb.h sys/soundcard.h soundcard.h])
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset select socket strerror getopt_long])

AC_CHECK_HEADER(soundcard.h, have_oss=yes ,have_oss=no)
AC_CHECK_HEADER(sys/soundcard.h, have_sys_oss=yes , have_sys_oss=no)

AC_CHECK_HEADER(alsa/asoundlib.h, ,enable_alsa=no,)
AC_CHECK_HEADER(ao/ao.h, ,enable_ao=no,)
AC_CHECK_HEADER(portaudio.h, ,enable_porta=no,)
AC_CHECK_HEADER("AL/al.h", , enable_al=no,)
AC_CHECK_HEADER(muroar.h, , enable_muroar=no,)
AC_CHECK_HEADER(pulse/simple.h, , enable_pulse=no,)
AC_CHECK_LIB(asound, snd_pcm_open, ,enable_alsa=no)
AC_CHECK_LIB(ao, ao_open_live, ,enable_ao=no)
AC_CHECK_LIB(portaudio, Pa_OpenStream, ,enable_porta=no)
AC_CHECK_LIB(openal, alcOpenDevice, ,enable_al=no)
AC_CHECK_LIB(muroar, muroar_stream, ,enable_muroar=no)
AC_CHECK_LIB(pulse-simple, pa_simple_new, ,enable_pulse=no)
AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR([Could not find pthread]),)

AC_CHECK_LIB(dnet, dnet_conn, , enable_decnet=no ,enable_decnet=yes)
AC_CHECK_LIB(rt, clock_gettime, , enable_rt=no, enable_rt=yes)
AC_CHECK_LIB(ossaudio, _oss_ioctl, , need_ossaudio=no, need_ossaudio=yes)

AM_CONDITIONAL(HAVE_SYS_OSS_H, test x$have_sys_oss = xyes)
AM_CONDITIONAL(HAVE_OSS_H, test x$have_oss = xyes)
AM_CONDITIONAL(HAVE_OSS, test x$enable_oss = xyes)
AM_CONDITIONAL(NEED_OSSAUDIO_LIB, test x$need_ossaudio = xyes)
AM_CONDITIONAL(HAVE_DECNET, test x$enable_decnet = xyes)
AM_CONDITIONAL(HAVE_RT, test x$enable_rt = xyes)
AM_CONDITIONAL(HAVE_ALSA, test x$enable_alsa = xyes)
AM_CONDITIONAL(HAVE_PORTA, test x$enable_porta = xyes)
AM_CONDITIONAL(HAVE_AO, test x$enable_ao = xyes)
AM_CONDITIONAL(HAVE_AL, test x$enable_al = xyes)
AM_CONDITIONAL(HAVE_PULSE, test x$enable_pulse = xyes)
AM_CONDITIONAL(HAVE_MUROAR, test x$enable_muroar = xyes)

AM_PROG_LIBTOOL

AC_OUTPUT(Makefile rsound.pc)
