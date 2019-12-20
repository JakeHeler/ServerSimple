/* Copyright (c) 2000-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#include "my_global.h"
#include "net_comm.h"

#include "violite.h"


/*
 * Helper to fill most of the Vio* with defaults.
 */

static void vio_init(Vio* vio, enum enum_vio_type type,
                     my_socket sd, HANDLE hPipe, uint flags)
{
	log_info("vio_init");
	log_info("enter", ("type: %d  sd: %d  flags: %d", type, sd, flags));

#ifndef HAVE_VIO_READ_BUFF
	flags &= ~VIO_BUFFERED_READ;
#endif
	bzero((char*) vio, sizeof(*vio));
	vio->type	= type;
	vio->sd	= sd;
	vio->hPipe	= hPipe;
	vio->localhost = flags & VIO_LOCALHOST;
	if ((flags & VIO_BUFFERED_READ) &&
	        !(vio->read_buffer = (char*)malloc(VIO_READ_BUFFER_SIZE)))
		flags &= ~VIO_BUFFERED_READ;
#ifdef HAVE_OPENSSL
	if (type == VIO_TYPE_SSL)
	{
		vio->viodelete	= vio_ssl_delete;
		vio->vioerrno	= vioErrno;
		vio->read		= vio_ssl_read;
		vio->write		= vio_ssl_write;
		vio->fastsend	= vioFastsend;
		vio->viokeepalive	= vioKeepalive;
		vio->should_retry	= vioShouldRetry;
		vio->was_interrupted = vioWasInterrupted;
		vio->vioclose	= vio_ssl_close;
		vio->peer_addr	= vioPeerAddr;
		vio->in_addr	= vioInAddr;
		vio->vioblocking	= vio_ssl_blocking;
		vio->is_blocking	= vioIsBlocking;
		vio->timeout	= vioTimeout;
		DBUG_VOID_RETURN;
	}
#endif /* HAVE_OPENSSL */
	vio->viodelete	= vioDelete;
	vio->vioerrno	= vioErrno;
	vio->read = (flags & VIO_BUFFERED_READ) ? vioReadBuff : vioRead;
	vio->write		= vioWrite;
	vio->fastsend	= vioFastsend;
	vio->viokeepalive	= vioKeepalive;
	vio->should_retry	= vioShouldRetry;
	vio->was_interrupted = vioWasInterrupted;
	vio->vioclose	= vioClose;
	vio->peer_addr	= vioPeerAddr;
	vio->in_addr	= vioInAddr;
	vio->vioblocking	= vioBlocking;
	vio->is_blocking	= vioIsBlocking;
	//vio->timeout	=vio_timeout;
	return;
}


/* Reset initialized VIO to use with another transport type */

void vioReset(Vio* vio, enum enum_vio_type type,
               my_socket sd, HANDLE hPipe, uint flags)
{
	free(vio->read_buffer);
	vio_init(vio, type, sd, hPipe, flags);
}


/* Open the socket or TCP/IP connection and read the fnctl() status */

Vio *vioNew(my_socket sd, enum enum_vio_type type, uint flags)
{
	Vio *vio;
	log_info("vioNew");
	log_info("enter", ("sd: %d", sd));

	log_info("!!! Vio :malloc Size =%d", sizeof(Vio));

	if ((vio = (Vio*) malloc(sizeof(*vio))))
	{
		vio_init(vio, type, sd, 0, flags);
		sprintf(vio->desc, (vio->type == VIO_TYPE_SOCKET ? "socket (%d)" : "TCP/IP (%d)"), vio->sd);

		/*
		  We call fcntl() to set the flags and then immediately read them back
		  to make sure that we and the system are in agreement on the state of
		  things.

		  An example of why we need to do this is FreeBSD (and apparently some
		  other BSD-derived systems, like Mac OS X), where the system sometimes
		  reports that the socket is set for non-blocking when it really will
		  block.
		*/
		fcntl(sd, F_SETFL, 0);
		vio->fcntl_mode = fcntl(sd, F_GETFL);

	}
	return vio;

}




void vioDelete(Vio* vio)
{
	if (!vio)
		return; /* It must be safe to delete null pointers. */

	if (vio->type != VIO_CLOSED)
		vio->vioclose(vio);
	free((uchar*) vio->read_buffer);
	vio->read_buffer =0;
	free((uchar*) vio);
	vio =0;
}


/*
  Cleanup memory allocated by vio or the
  components below it when application finish

*/
void vioEnd(void)
{

}


