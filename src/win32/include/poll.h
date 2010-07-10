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

/* Win32 does not implement poll() */

#ifdef _WIN32

#ifndef _POLL_H
#define _POLL_H

enum poll_event { 
	POLLIN = 0x0001,
	POLLOUT = 0x0002,
	POLLHUP = 0x0004
};


struct pollfd
{
	int fd;
	int events;
	int revents;
};

int poll(struct pollfd *fd, int num, int timeout);

#endif

#endif
