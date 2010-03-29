#include <rsound.h>
#include <string>

class RSound
{
   private:

      rsound_t *rd;

   public:

      RSound& setHost(std::string host) 
      { 
         rsd_set_param(rd, RSD_HOST, (void*)host.c_str()); return *this; 
      }
      
      RSound& setChannels(int channels) 
      { 
         rsd_set_param(rd, RSD_CHANNELS, (void*)&channels); return *this; 
      }
      
      RSound& setPort(std::string port) 
      { 
         rsd_set_param(rd, RSD_PORT, (void*)port.c_str()); return *this;
      }
      
      RSound& setRate(int rate) 
      { 
         rsd_set_param(rd, RSD_SAMPLERATE, (void*)&rate); return *this; 
      }

      bool start() 
      { 
         if ( rsd_start(rd) < 0 ) 
            return false; 
         return true; 
      }
      
      bool stop() 
      { 
         if ( rsd_stop(rd) < 0 ) 
            return false; 
         return true; 
      }

      size_t write(const char* buf, size_t size) 
      { 
         if ( rsd_write(rd, buf, size) <= 0 ) 
         { 
            rsd_stop(rd); 
            return 0; 
         } 
         return size; 
      }

      RSound()
      {
         rsd_init(&rd);
         setHost("localhost").setPort("12345").setChannels(2).setRate(44100);
      }

      RSound(std::string host)
      {
         rsd_init(&rd);
         setHost(host).setPort("12345").setChannels(2).setRate(44100);
      }
      
      RSound(std::string host, std::string port)
      {
         rsd_init(&rd);
         setHost(host).setPort(port).setChannels(2).setRate(44100);
      }

      ~RSound()
      {
         rsd_stop(rd);
         rsd_free(rd);
      }
};


