/*
   Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/
#include "my_global.h"
#include "net_comm.h"


#include "violite.h"

#define max(a, b)	((a) > (b) ? (a) : (b))
#define min(a, b)	((a) < (b) ? (a) : (b))

void my_inet_ntoa(struct in_addr in, char *buf)
{
	char *ptr;
	//pthread_mutex_lock(&THR_LOCK_net);
	ptr = (char *)inet_ntoa(in);
	strmov(buf, ptr);
	//pthread_mutex_unlock(&THR_LOCK_net);
}


int vioErrno(Vio *vio __attribute__((unused)))
{
	//return socket_errno;		/* On Win32 this mapped to WSAGetLastError() */
	return 0;
}


size_t vioRead(Vio * vio, uchar* buf, size_t size)
{
	size_t r;
	log_info("vioRead");
	log_info("sd: %d  buf: 0x%x  size: %u", vio->sd, (uchar) buf, (uint) size);

	/* Ensure nobody uses vio_read_buff and vio_read simultaneously */
	errno=0;					/* For linux */
	r = read(vio->sd, buf, size);

#ifndef DBUG_OFF
	if (r == (size_t) -1)
	{
		log_info( "Got error %d during read", errno);
	}
#endif /* DBUG_OFF */
	log_info("Vio_Read Size =%ld", (long) r);

//	int i=0;
//	for( i=0;i<size;i++)
//		printf("%02x",(uint8_t)buf[i]);

	return (r);

}


/*
  Buffered read: if average read size is small it may
  reduce number of syscalls.
*/

size_t vioReadBuff(Vio *vio, uchar* buf, size_t size)
{
	size_t rc;
#define VIO_UNBUFFERED_READ_MIN_SIZE 2048
	log_info("vioReadBuff");
	log_info("sd: %d  buf: 0x%lx  size: %u", vio->sd, (uchar) buf, (uint) size);

	if (vio->read_pos < vio->read_end)
	{
		rc = min((size_t) (vio->read_end - vio->read_pos), size);
		memcpy(buf, vio->read_pos, rc);
		vio->read_pos += rc;
		/*
		  Do not try to read from the socket now even if rc < size:
		  vio_read can return -1 due to an error or non-blocking mode, and
		  the safest way to handle it is to move to a separate branch.
		*/
	}
	else if (size < VIO_UNBUFFERED_READ_MIN_SIZE)
	{
		rc = vioRead(vio, (uchar*) vio->read_buffer, VIO_READ_BUFFER_SIZE);
		if (rc != 0 && rc != (size_t) -1)
		{
			if (rc > size)
			{
				vio->read_pos = vio->read_buffer + size;
				vio->read_end = vio->read_buffer + rc;
				rc = size;
			}
			memcpy(buf, vio->read_buffer, rc);
		}
	}
	else
		rc = vioRead(vio, buf, size);
	return(rc);
#undef VIO_UNBUFFERED_READ_MIN_SIZE
}


size_t vioWrite(Vio * vio, const uchar* buf, size_t size)
{
	size_t r;
	log_info("vioWrite");
	log_info("sd: %d  buf: 0x%lx  size: %u", vio->sd, (long) buf, (uint) size);
#ifdef __WIN__
	r = send(vio->sd, buf, size, 0);
#else
	r = write(vio->sd, buf, size);
#endif /* __WIN__ */
#ifndef DBUG_OFF
	if (r == (size_t) -1)
	{
		log_info("vio_error");
	}
#endif /* DBUG_OFF */
	//log_info("exit", ("%u", (uint) r));
	return(r);
}

int vioBlocking(Vio * vio __attribute__((unused)), my_bool set_blocking_mode,
                 my_bool *old_mode)
{
	int r = 0;
	log_info("vioBlocking");

	*old_mode = test(!(vio->fcntl_mode & O_NONBLOCK));
	log_info("set_blocking_mode: %d  old_mode: %d", (int) set_blocking_mode, (int) *old_mode);

#if !defined(__WIN__)
#if !defined(NO_FCNTL_NONBLOCK)
	if (vio->sd >= 0)
	{
		int old_fcntl = vio->fcntl_mode;
		if (set_blocking_mode)
			vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
		else
			vio->fcntl_mode |= O_NONBLOCK; /* set bit */
		if (old_fcntl != vio->fcntl_mode)
		{
			r = fcntl(vio->sd, F_SETFL, vio->fcntl_mode);
			if (r == -1)
			{
				//log_info("info", ("fcntl failed, errno %d", errno));
				vio->fcntl_mode = old_fcntl;
			}
		}
	}
#else
	r = set_blocking_mode ? 0 : 1;
#endif /* !defined(NO_FCNTL_NONBLOCK) */
#else /* !defined(__WIN__) */
	if (vio->type != VIO_TYPE_NAMEDPIPE && vio->type != VIO_TYPE_SHARED_MEMORY)
	{
		ulong arg;
		int old_fcntl = vio->fcntl_mode;
		if (set_blocking_mode)
		{
			arg = 0;
			vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
		}
		else
		{
			arg = 1;
			vio->fcntl_mode |= O_NONBLOCK; /* set bit */
		}
		if (old_fcntl != vio->fcntl_mode)
			r = ioctlsocket(vio->sd, FIONBIO, (void*) &arg);
	}
	else
		r =  test(!(vio->fcntl_mode & O_NONBLOCK)) != set_blocking_mode;
#endif /* !defined(__WIN__) */

	return(r);
}

my_bool
vioIsBlocking(Vio * vio)
{
	my_bool r;
	log_info("vioIsBlocking");
	r = !(vio->fcntl_mode & O_NONBLOCK);
	//log_info("exit", ("%d", (int) r));
	return(r);
}


int vioFastsend(Vio * vio __attribute__((unused)))
{
	int r = 0;
	log_info("vioFastsend");

#if defined(IPTOS_THROUGHPUT)
	{
		//int tos = IPTOS_THROUGHPUT;
		//r = setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos));
	}
#endif                                    /* IPTOS_THROUGHPUT */
	//if (!r)
	{
#ifdef __WIN__
		BOOL nodelay = 1;
#else
		int nodelay = 1;
#endif

		r = setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY,
		               (void*) &nodelay,
		               sizeof(nodelay));

	}
	if (r)
	{
		log_info("warning Couldn't set socket option for fast send");
		r = -1;
	}

	return(r);
}

int vioKeepalive(Vio* vio, my_bool set_keep_alive)
{
	int r = 0;
	uint opt = 0;
	log_info("vioKeepalive");
	log_info("sd: %d  set_keep_alive: %d", vio->sd, (int)  set_keep_alive);
	if (vio->type != VIO_TYPE_NAMEDPIPE)
	{
		if (set_keep_alive)
			opt = 1;
		r = setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
		               sizeof(opt));
	}
	return(r);
}


my_bool
vioShouldRetry(Vio * vio __attribute__((unused)))
{
	int en = socket_errno;
	return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	        en == SOCKET_EWOULDBLOCK);
}


my_bool
vioWasInterrupted(Vio *vio __attribute__((unused)))
{
	int en = socket_errno;
	return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	        en == SOCKET_EWOULDBLOCK || en == SOCKET_ETIMEDOUT);
}


int
mysql_socket_shutdown(my_socket mysql_socket, int how)
{
	int result;

#ifdef __WIN__
	static LPFN_DISCONNECTEX DisconnectEx = NULL;
	if (DisconnectEx == NULL)
	{
		DWORD dwBytesReturned;
		GUID guidDisconnectEx = WSAID_DISCONNECTEX;
		WSAIoctl(mysql_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		         &guidDisconnectEx, sizeof(GUID),
		         &DisconnectEx, sizeof(DisconnectEx),
		         &dwBytesReturned, NULL, NULL);
	}
#endif

	/* Non instrumented code */
#ifdef __WIN__
	if (DisconnectEx)
		result = (DisconnectEx(mysql_socket, (LPOVERLAPPED) NULL,
		                       (DWORD) 0, (DWORD) 0) == TRUE) ? 0 : -1;
	else
#endif
		result = shutdown(mysql_socket, how);

	return result;
}


int vioClose(Vio * vio)
{
	int r = 0;
	log_info("vioClose : Type =%d",vio->type);

	if (vio->type != VIO_CLOSED)
	{

		if (shutdown(vio->sd, SHUT_RDWR))
			r = -1;

		//log_info("vio_close sd cl0se =%d",vio->sd);
		if (close(vio->sd))
			r = -1;
	}
	if (r)
	{
		log_error("vio_error =%d",errno);
		/* FIXME: error handling (not critical for MySQL) */
	}
	vio->type = VIO_CLOSED;
	vio->sd =   -1;
	return(r);
}


const char *vioDescription(Vio * vio)
{
	return vio->desc;
}

enum enum_vio_type vio_type(Vio* vio)
{
	return vio->type;
}

my_socket vioFD(Vio* vio)
{
	return vio->sd;
}


my_bool vioPeerAddr(Vio * vio, char *buf, uint16 *port)
{
	log_info("vioPeerAddr");

	if (vio->localhost)
	{
		strmov(buf, "127.0.0.1");
		*port = 0;
	}
	else
	{
		int addrLen = sizeof(vio->remote);
		if (getpeername(vio->sd, (struct sockaddr *) (&vio->remote),
		                &addrLen) != 0)
		{
			log_error("exit");
			return(1);
		}
		my_inet_ntoa(vio->remote.sin_addr, buf);
		*port = ntohs(vio->remote.sin_port);
	}

	return(0);
}


/*
  Get in_addr for a TCP/IP connection

  SYNOPSIS
    vio_in_addr()
    vio		vio handle
    in		put in_addr here

  NOTES
    one must call vio_peer_addr() before calling this one
*/

void vioInAddr(Vio *vio, struct in_addr *in)
{
	log_info("vioInAddr");
	if (vio->localhost)
		bzero((char*) in, sizeof(*in));
	else
		*in = vio->remote.sin_addr;
	return;
}


/* Return 0 if there is data to be read */

my_bool vioPollRead(Vio *vio, uint timeout)
{
#ifndef HAVE_POLL
	return 0;
#else
	struct pollfd fds;
	int res;
	log_info("vio_poll");
	fds.fd = vio->sd;
	fds.events = POLLIN;
	fds.revents = 0;
	if ((res = poll(&fds, 1, (int) timeout * 1000)) <= 0)
	{
		return(res < 0 ? 0 : 1);		/* Don't return 1 on errors */
	}
	return(fds.revents & POLLIN ? 0 : 1);
#endif
}


void vioTimeout(Vio *vio, uint which, uint timeout)
{
#if defined(SO_SNDTIMEO) && defined(SO_RCVTIMEO)
	int r;
	log_info("vioTimeout");

	{
#ifdef __WIN__
		/* Windows expects time in milliseconds as int */
		int wait_timeout = (int) timeout * 1000;
#else
		/* POSIX specifies time as struct timeval. */
		struct timeval wait_timeout;
		wait_timeout.tv_sec = timeout;
		wait_timeout.tv_usec = 0;
#endif

		r = setsockopt(vio->sd, SOL_SOCKET, which ? SO_SNDTIMEO : SO_RCVTIMEO,
		               (const void*)&wait_timeout,
		               sizeof(wait_timeout));

	}

#ifndef DBUG_OFF
	if (r != 0)
		log_error("error: setsockopt failed: ");
#endif

	return;
#else
	/*
	  Platforms not suporting setting of socket timeout should either use
	  thr_alarm or just run without read/write timeout(s)
	*/
#endif
}



