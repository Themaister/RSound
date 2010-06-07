#!/usr/bin/env python

from ctypes import *
import sys

librsound = cdll.LoadLibrary('librsound.so')

rsd_init = librsound.rsd_init
rsd_init.restype = c_int
rsd_init.argtypes = [c_void_p]

rsd_set_param = librsound.rsd_set_param
rsd_set_param.restype = c_int
rsd_set_param.argtypes = [c_void_p, c_int, c_void_p]


rsd_start = librsound.rsd_start
rsd_set_param.restype = c_int
rsd_set_param.argtypes = [c_void_p]


rsd_stop = librsound.rsd_stop
rsd_stop.restype = c_int
rsd_stop.argtypes = [c_void_p]


rsd_write = librsound.rsd_write
rsd_write.restype = c_ulong
rsd_write.argtypes = [c_void_p, c_void_p, c_ulong]


rsd_free = librsound.rsd_free
rsd_free.restype = c_int
rsd_free.argtypes = [c_void_p]

rsd_delay = librsound.rsd_delay
rsd_delay.restype = c_ulong
rsd_delay.argtypes = [c_void_p]

rsd_delay_ms = librsound.rsd_delay_ms
rsd_delay.restype = c_ulong
rsd_delay.argtypes = [c_void_p]

rsd_samplesize = librsound.rsd_samplesize
rsd_samplesize.restype = c_int
rsd_samplesize.argtypes = [c_void_p]

rsd_get_avail = librsound.rsd_get_avail
rsd_get_avail.restype = c_ulong
rsd_get_avail.argtypes = [c_void_p]

rsd_pause = librsound.rsd_pause
rsd_get_avail.restype = c_int
rsd_get_avail.argtypes = [c_void_p, c_int]

rsd_delay_wait = librsound.rsd_delay_wait
rsd_delay_wait.restype = None
rsd_delay_wait.argtypes = [c_void_p]


RSD_SAMPLERATE = 0
RSD_CHANNELS = 1
RSD_HOST = 2
RSD_PORT = 3
RSD_BUFSIZE = 4
RSD_LATENCY = 5
RSD_FORMAT = 6
RSD_IDENTITY = 7

RSD_S16_LE = 0x0001
RSD_S16_BE = 0x0002
RSD_U16_LE = 0x0004
RSD_U16_BE = 0x0008
RSD_U8 =     0x0010
RSD_S8 =     0x0020
RSD_S16_NE = 0x0040
RSD_U16_NE = 0x0080
RSD_ALAW =   0x0100
RSD_MULAW =  0x0200


class RSound:
   def __init__(self):
      self.rd = c_void_p()
      rsd_init(byref(self.rd))

   def setRate(self, rate):
      i_rate = c_int(rate)
      rsd_set_param(self.rd, RSD_SAMPLERATE, byref(i_rate))

   def setChannels(self, chan):
      i_chan = c_int(chan)
      rsd_set_param(self.rd, RSD_CHANNELS, byref(i_chan))

   def setFormat(self, fmt):
      i_fmt = c_int(fmt)
      rsd_set_param(self.rd, RSD_FORMAT, byref(i_fmt))

   def setBufsize(self, bufsize):
      i_size = c_int(bufsize)
      rsd_set_param(self.rd, RSD_BUFSIZE, byref(i_size))

   def setLatency(self, lat):
      i_lat = c_int(lat)
      rsd_set_param(self.rd, RSD_LATENCY, byref(i_lat))

   def setIdentity(self, ident):
      cident = create_string_buffer(str(ident))
      rsd_set_param(self.rd, RSD_IDENTITY, cident)

   def delay_wait(self):
      rsd_delay_wait(self.rd)

   def setHost(self, host):
      chost = create_string_buffer(str(host))
      rsd_set_param(self.rd, RSD_HOST, chost)

   def setPort(self, port):
      cport = create_string_buffer(str(port))
      rsd_set_param(self.rd, RSD_PORT, cport)


   def start(self):
      if rsd_start(self.rd) == 0:
         return True
      else:
         return False

   def stop(self):
      if rsd_stop(self.rd) == 0:
         return True
      else:
         return False


   def write(self, buf):
      return rsd_write(self.rd, str(buf), len(str(buf)))


   def avail(self):
      return rsd_get_avail(self.rd)

   def delay(self):
      return rsd_delay(self.rd)

   def delay_ms(self):
      return rsd_delay_ms(self.rd)

   def __del__(self):
      if self.rd and rsd_stop:
         rsd_stop(self.rd)
      if self.rd and rsd_free:
         rsd_free(self.rd)
   





if __name__ == '__main__':

   rd = RSound()
   rd.setRate(44100)
   rd.setChannels(2)
   rd.setFormat(RSD_S16_LE)
   rd.setHost('localhost')
   rd.setPort(12345)
   
   if rd.start():
      buf = sys.stdin.read(44)
      buf = sys.stdin.read(128)
      while rd.write(buf):
         buf = sys.stdin.read(128)



