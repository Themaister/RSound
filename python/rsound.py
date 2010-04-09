#!/usr/bin/env python

import sys
import socket
import struct
import Queue
from threading import Thread

class RSoundThread(Thread):
   
   def __init__(self, host, port):
      self.bufsiz = 2
      self.buffer = Queue.Queue(self.bufsiz)
      self.running = True
      Thread.__init__(self)
      self.socketsend = socketSend(host, port)

   def stop(self):
      self.running = False

   def run(self):
      while self.running:
         try:
            tempbuf = self.buffer.get(True, 3)
            if not self.socketsend.send(tempbuf):
               raise IOError
            self.buffer.task_done()
         except IOError:
            self.running = False
            self.socketsend.close()
         except KeyboardInterrupt:
            self.running = False
         except:
            self.running = False
            self.socketsend.close()

   def write(self, buffer):
      if not self.running:
         return False
      else:
         try:
            self.buffer.put(buffer)
         except KeyboardInterrupt:
            self.running = False
         except:
            self.stop()
      return True
         

   def getAvail(self):
      return self.bufsiz - self.buffer.qsize()


class socketSend:
   def __init__(self, host, port):
      self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      self.ctl = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      self.host = host
      self.port = port
   
   def send(self, buffer):
      try:
         self.s.send(buffer)
      except:
         return False
      return True

   def connect(self):
      try:
         self.s.connect((self.host, self.port))
         self.ctl.connect((self.host, self.port))
      except:
         return False
      return True

   def close(self):
      self.s.close()
      self.ctl.close()

class rsound:
   def __init__(self, host='localhost', port=12345):
      self.host = host
      self.port = port
      self.waveheader = []
      self.thread = RSoundThread(host, port)
   
   def start(self):
         if not self.thread.socketsend.connect():
            return False
         self.send_header()
         self.thread.start()
         return True
   def stop(self):
      try:
         self.thread.stop()
         self.thread.socketsend.close()
         self.waveheader = []
         self.thread.join(1)
      except:
         return False
      return True

   def write(self, buffer):
      try:
         if len(buffer) > 0:
            if self.thread.write(buffer):
               return True
            else:
               self.stop()
               return False
         return False
      except KeyboardInterrupt:
         self.stop()
         exit(1)
      except:
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
      if len(self.waveheader) > 0:
         self.thread.socketsend.send(self.waveheader)
      else:
         # Some bullshit header stuff to fill out useless data :D
         header = '1234123412341234123412'
         header += struct.pack('<L', self.channels)[:2]
         header += struct.pack('<I', self.rate)
         header += '123456'
         header += struct.pack('<L', 16)[:2]
         header += '12341234'
         self.thread.socketsend.send(header)
   def setWaveHeader(self, header):
      self.waveheader = header
      
def readWaveHeader(file):
   try:
      return file.read(44)
   except:
      return []


rsd = rsound()
rsd.setWaveHeader(readWaveHeader(sys.stdin))
if rsd.start():
   while rsd.write(sys.stdin.read(64)):
      pass
   rsd.stop()
else:
   print "Couldn't connect to server"
