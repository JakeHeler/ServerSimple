/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * Vio Lite.
 * Purpose: include file for Vio that will work with C and C++
 */

#ifndef vio_violite_h_
#define	vio_violite_h_

//#include "my_net.h"			/* needed because of struct in_addr */

#include "my_global.h"


/* Simple vio interface in C;  The functions are implemented in violite.c */

#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */


#define HANDLE void *

enum enum_vio_type
{
  VIO_CLOSED, VIO_TYPE_TCPIP, VIO_TYPE_SOCKET, VIO_TYPE_NAMEDPIPE,
  VIO_TYPE_SSL, VIO_TYPE_SHARED_MEMORY
};


#define VIO_LOCALHOST 1                         /* a localhost connection */
#define VIO_BUFFERED_READ 2                     /* use buffered read */
#define VIO_READ_BUFFER_SIZE 16384              /* size of read buffer */


/* This enumerator is used in parser - should be always visible */
enum SSL_type
{
  SSL_TYPE_NOT_SPECIFIED= -1,
  SSL_TYPE_NONE,
  SSL_TYPE_ANY,
  SSL_TYPE_X509,
  SSL_TYPE_SPECIFIED
};


/* HFTODO - hide this if we don't want client in embedded server */
/* This structure is for every connection on both sides */
struct st_vio
{
  my_socket		sd;		/* my_socket - real or imaginary */
  HANDLE hPipe;
  my_bool		localhost;	/* Are we from localhost? */
  int			fcntl_mode;	/* Buffered fcntl(sd,F_GETFL) */
  struct sockaddr_in	local;		/* Local internet address */
  struct sockaddr_in	remote;		/* Remote internet address */
  enum enum_vio_type	type;		/* Type of connection */
  char			desc[30];	/* String description */
  char                  *read_buffer;   /* buffer for vio_read_buff */
  char                  *read_pos;      /* start of unfetched data in the read buffer */
  char                  *read_end;      /* end of unfetched data */

  /* function pointers. They are similar for socket/SSL/whatever */
  void    (*viodelete)(Vio*);
  int     (*vioerrno)(Vio*);
  size_t  (*read)(Vio*, uchar *, size_t);
  size_t  (*write)(Vio*, const uchar *, size_t);
  int     (*vioblocking)(Vio*, my_bool, my_bool *);
  my_bool (*is_blocking)(Vio*);
  int     (*viokeepalive)(Vio*, my_bool);
  int     (*fastsend)(Vio*);
  my_bool (*peer_addr)(Vio*, char *, uint16*);
  void    (*in_addr)(Vio*, struct in_addr*);
  my_bool (*should_retry)(Vio*);
  my_bool (*was_interrupted)(Vio*);
  int     (*vioclose)(Vio*);
  void	  (*timeout)(Vio*, unsigned int, unsigned int timeout);
#ifdef HAVE_OPENSSL
  void	  *ssl_arg;
#endif
};




Vio*	vioNew(my_socket sd, enum enum_vio_type type, uint flags);


//int	mysql_socket_shutdown(my_socket mysql_socket, int how);

void	vioDelete(Vio* vio);
int	vioClose(Vio* vio);
void    vioReset(Vio* vio, enum enum_vio_type type,
                  my_socket sd, HANDLE hPipe, uint flags);
size_t	vioRead(Vio *vio, uchar *	buf, size_t size);
size_t  vioReadBuff(Vio *vio, uchar * buf, size_t size);
size_t	vioWrite(Vio *vio, const uchar * buf, size_t size);
int	vioBlocking(Vio *vio, my_bool onoff, my_bool *old_mode);
my_bool	vioIsBlocking(Vio *vio);
/* setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible */
int	vioFastsend(Vio *vio);
/* setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible */
int	vioKeepalive(Vio *vio, my_bool	onoff);
/* Whenever we should retry the last read/write operation. */
my_bool	vioShouldRetry(Vio *vio);
/* Check that operation was timed out */
my_bool	vioWasInterrupted(Vio *vio);
/* Short text description of the socket for those, who are curious.. */
const char* vioDescription(Vio *vio);
/* Return the type of the connection */
enum enum_vio_type vio_type(Vio* vio);
/* Return last error number */
int	vioErrno(Vio*vio);
/* Get socket number */
my_socket vioFD(Vio*vio);
/* Remote peer's address and name in text form */
my_bool	vioPeerAddr(Vio* vio, char *buf, uint16 *port);
/* Remotes in_addr */
void	vioInAddr(Vio *vio, struct in_addr *in);
my_bool	vioPollRead(Vio *vio,uint timeout);

void vioTimeout(Vio *vio, uint which, uint timeout);





void vioEnd(void);



#ifdef	__cplusplus
}
#endif


#endif /* vio_violite_h_ */
