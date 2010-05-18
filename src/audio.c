/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 * 
 *  RSound is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RSound is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RSound.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "audio.h"
#include "endian.h"

inline static void swap_bytes16(uint16_t *data, size_t bytes)
{
   int i;
   for ( i = 0; i < (int)bytes/2; i++ )
   {
      swap_endian_16(&data[i]);
   }
}

void audio_converter(void* data, enum rsd_format fmt, int operation, size_t bytes)
{
   if ( operation == RSD_NULL )
      return;

   // Temporarily hold the data that is to be sign-flipped.
   int32_t temp32;
   int i;
   int swapped = 0;
   int bits = rsnd_format_to_bytes(fmt) * 8;

   uint8_t buffer[bytes];

   // Fancy union to make the conversions more clean looking ;)
   union
   {
      uint8_t *u8;
      int8_t *s8;
      uint16_t *u16;
      int16_t *s16;
      void *ptr;
   } u;

   memcpy(buffer, data, bytes);
   u.ptr = buffer;

   i = 0;
   if ( is_little_endian() )
      i++;
   if ( fmt & (RSD_S16_BE | RSD_U16_BE) )
      i++;

   // If we're gonna operate on the data, we better make sure that we are in native byte order
   if ( (i % 2) == 0 && bits == 16 )
   {
      swap_bytes16(u.u16, bytes);
      swapped = 1;
   }
   
   if ( operation & RSD_S_TO_U )
   {
      if ( bits == 8 )
      {
         for ( i = 0; i < (int)bytes; i++ )
         {
            temp32 = (u.s8)[i];
            temp32 += (1 << 7);
            (u.u8)[i] = (uint8_t)temp32;
         }
      }

      else if ( bits == 16 )
      {
         for ( i = 0; i < (int)bytes/2; i++ )
         {
            temp32 = (u.s16)[i];
            temp32 += (1 << 15);
            (u.u16)[i] = (uint16_t)temp32;
         }
      }
   }

   else if ( operation & RSD_U_TO_S )
   {
      if ( bits == 8 )
      {
         for ( i = 0; i < (int)bytes; i++ )
         {
            temp32 = (u.u8)[i];
            temp32 -= (1 << 7);
            (u.s8)[i] = (int8_t)temp32;
         }
      }

      else if ( bits == 16 )
      {
         for ( i = 0; i < (int)bytes/2; i++ )
         {
            temp32 = (u.u16)[i];
            temp32 -= (1 << 15);
            (u.s16)[i] = (int16_t)temp32;
         }
      }
   }

   if ( operation & RSD_SWAP_ENDIAN )
   {
      if ( !swapped && bits == 16 )
      {
         swap_bytes16(u.u16, bytes);
      }
   }

   else if ( swapped ) // We need to flip back. Kinda expensive, but hey. An endian-flipping-less algorithm will be more complex. :)
   {
      swap_bytes16(u.u16, bytes);
   }

   memcpy(data, buffer, bytes);
}




