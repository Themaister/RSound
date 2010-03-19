#!/usr/bin/env python

import sys
import socket
import struct

class rsound:
   def __init__(self, host='localhost', port=12345):
      self.host = host
      self.port = port
      self.waveheader = []
   
   def start(self):
      try:
         self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         self.ctl = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         self.s.connect((self.host, self.port))
         self.ctl.connect((self.host, self.port))
         self.send_header()
      except IOError:
         print 'Failed to connect'
         return False
      except:
         print 'Unknown exception'
         return False
      return True
   def stop(self):
      try:
         self.s.close()
         self.ctl.close()
         self.waveheader = []
      except:
         print 'Failed to close connection'
         return False
      return True

   def write(self, buffer):
      try:
         if len(buffer) > 0:
            self.s.send(buffer)
            return True
         return False
      except KeyboardInterrupt:
         print 'Exiting ...'
         self.stop()
         exit(1)
      except:
         print 'Failed to write stream'
         self.stop()
         return False
      return True
   def setHost(self, host):
      self.host = host
   def setPort(self, port):
      self.port = port
   def setChannels(self, channels):
      self.channels = channels
   def setRate(self, rate):
      self.rate = rate
   def send_header(self):
      try:
         if len(self.waveheader) > 0:
            self.s.send(self.waveheader)
         else:
            # Some bullshit header stuff to fill out useless data :D
            header = '1234123412341234123412'
            header += struct.pack('<L', self.channels)[:2]
            header += struct.pack('<I', self.rate)
            header += '123456'
            header += struct.pack('<L', 16)[:2]
            header += '12341234'
            self.s.send(header)
      except:
         print 'Failed to send header'
   def setWaveHeader(self, header):
      self.waveheader = header
      
def readWaveHeader(file):
   try:
      return file.read(44)
   except:
      print "Couldn't read WAV header"
      return []


