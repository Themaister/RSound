using System;
using System.Runtime.InteropServices;

namespace RSound
{
	public unsafe class AudioStream : IDisposable
	{
		
		public enum Format : int
		{
			RSD_S16_LE = 0x0001,
		    RSD_S16_BE = 0x0002,
		    RSD_U16_LE = 0x0004,
		    RSD_U16_BE = 0x0008,
		    RSD_U8     = 0x0010,
		    RSD_S8     = 0x0020,
		    RSD_S16_NE = 0x0040,
		    RSD_U16_NE = 0x0080,
		    RSD_ALAW   = 0x0100,
		    RSD_MULAW  = 0x0200,
		    RSD_S32_LE = 0x0400,
		    RSD_S32_BE = 0x0800,
		    RSD_S32_NE = 0x1000,
		    RSD_U32_LE = 0x2000,
		    RSD_U32_BE = 0x4000,
		    RSD_U32_NE = 0x8000,	
		}
		
		private void* rd;
		
#if win32
		private const string library = "rsound.dll";
#else
		private const string library = "librsound.so"; 
#endif
		
		[DllImport(library)]
		private static extern int rsd_simple_start(void** rd, string host, string port, 
		                                           string ident, int rate, int channels, int format);
		
		[DllImport(library)]
		private static extern IntPtr rsd_write(void* rd, void* buffer, IntPtr size);
		
		[DllImport(library)]
		private static extern int rsd_free(void* rd);
		
		[DllImport(library)]
		private static extern int rsd_stop(void* rd);
		
		[DllImport(library)]
		private static extern IntPtr rsd_get_avail(void* rd);
		
		[DllImport(library)]
		private static extern IntPtr rsd_delay_ms(void* rd);
		
		
		public AudioStream(int rate, int channels, Format fmt, string host = null, string port = null, string ident = null)
		{
			this.rd = null;
			int rc;
			
			fixed (void **ptr = &this.rd)
			{
				rc = rsd_simple_start(ptr, host, port, ident, rate, channels, (int)fmt);
			}
			if (rc < 0)
			{
				throw new Exception("Couldn't connect to server.");
			}
		}
		
		public void Write(byte[] buffer) 
		{
			IntPtr rc;
			fixed (byte *ptr = buffer)
			{
				rc = rsd_write(this.rd, ptr, (IntPtr)buffer.Length);
			}
			if (rc == (IntPtr)0)
			{
				throw new Exception("Connection closed");
			}
		}
		
		public float Delay()
		{
			IntPtr ms = rsd_delay_ms(this.rd);
			return (float)ms / (float)1000.0; 
		}
		
		public void Dispose()
		{
			if (this.rd != null)
			{
				rsd_stop(this.rd);
				rsd_free(this.rd);
				this.rd = null;
			}
		}
		
		public int WriteAvail()
		{
			IntPtr avail = rsd_get_avail(this.rd);
			return (int)avail;
		}
		
		~AudioStream()
		{
			if (this.rd != null)
			{
				rsd_stop(this.rd);
				rsd_free(this.rd);
			}
		}
		
	}
}

