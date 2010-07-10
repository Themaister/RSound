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

#ifdef _WIN32

#include "poll.h"
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>

int poll(struct pollfd *fd, int num, int timeout)
{
	fd_set read_fd;
	fd_set write_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	int i;

	int maxfd = 0;


	for( i = 0; i < num; i++ )
	{
		if ( fd[i].events & POLLIN )
			FD_SET((SOCKET)fd[i].fd, &read_fd);
		else if ( fd[i].events & POLLOUT )
			FD_SET((SOCKET)fd[i].fd, &write_fd);
		if ( fd[i].fd > maxfd )
			maxfd = fd[i].fd;
		fd[i].revents = 0;
	}

	struct timeval tv = {
		.tv_sec = (timeout * 1000) / 1000000,
		.tv_usec = (timeout * 1000) % 1000000
	};

	if ( select(maxfd+1, &read_fd, &write_fd, NULL, &tv) < 0 )
   {
      errno = EBADF;
		return -1;
   }

	for ( i = 0; i < num; i++ )
	{
		if ( FD_ISSET(fd[i].fd, &read_fd) )
		{
			fd[i].revents = POLLIN;
		}

		else if ( FD_ISSET(fd[i].fd, &write_fd) )
		{
			fd[i].revents = POLLOUT;
		}
	}

	return 0;

}

#endif
