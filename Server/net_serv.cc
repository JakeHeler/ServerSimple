/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/**
  @file

  This file is the net layer API for the MySQL client/server protocol.

  Write and read of logical packets to/from socket.

  Writes are cached into net_buffer_length big packets.
  Read packets are reallocated dynamicly when reading big packets.
  Each logical packet has the following pre-info:
  3 byte length & 1 byte package-number.

  This file needs to be written in C as it's used by the libmysql client as a
  C file.
*/

/*
  The following handles the differences when this is linked between the
  client and the server.

  This gives an error if a too big packet is found
  The server can change this with the -O switch, but because the client
  can't normally do this the client should have a bigger max_allowed_packet.
*/

#include "my_global.h"
#include "net_comm.h"

#include "violite.h"

#define max(a, b)	((a) > (b) ? (a) : (b))
#define min(a, b)	((a) < (b) ? (a) : (b))


//#include "my_pthread.h"
void sql_print_error(const char *format, ...);


#define TEST_BLOCKING		8
#define MAX_PACKET_LENGTH (256L*256L*256L-1)

static bool net_write_buff(stNET *net, const uchar *packet, ulong len);


//Do not use Alarm
#define NO_ALARM

void netLocalInit(stNET *net)
{

	net->max_packet =   (uint) NET_BUFFER_LENGTH;

	netSetReadTimeout(net, (uint)NET_READ_TIME_OUT);
	netSetWriteTimeout(net, (uint)NET_WRITE_TIME_OUT);

	net->retry_count =  (uint) NET_RETRY_COUNT;
	net->max_packet_size = max(NET_BUFFER_LENGTH, MAX_ALLOWED_PACKET);

}



/** Init with packet info. */

bool netInit(stNET *net, Vio* vio)
{
	log_info("netInit");
	net->vio = vio;
	netLocalInit(net);			/* Set some limits */
	if (!(net->buff = (uchar*) malloc((size_t) net->max_packet + NET_HEADER_SIZE + COMP_HEADER_SIZE)))
		return (1);
	net->buff_end = net->buff + net->max_packet;
	net->error = 0;
	net->return_errno = 0;
	net->return_status = 0;
	net->pkt_nr = net->compress_pkt_nr = 0;
	net->write_pos = net->read_pos = net->buff;

	//net->last_error[0]=0;

	net->compress = 0;
	net->reading_or_writing = 0;
	net->where_b = net->remain_in_buf = 0;
	net->last_errno = 0;

	net->query_cache_query = 0;
	net->skip_big_packet = FALSE;


	if (vio != 0)					/* If real connection */
	{
		net->fd  = vioFD(vio);			/* For perl DBI/DBD */

		log_info("net->fd: %lu", net->fd);

		vioFastsend(vio);
	}
	return(0);
}


void netEnd(stNET *net)
{
	log_info("netEnd");
	free(net->buff);
	net->buff = 0;

	return;
}


/** Realloc the packet buffer. */

bool netRealloc(stNET *net, size_t length)
{
	uchar *buff;
	size_t pkt_length;
	log_info("netRealloc");
	log_info("length: %lu", (ulong) length);

	if (length >= net->max_packet_size)
	{
		log_error("Packet too large. Max size: %lu", net->max_packet_size);
		/* @todo: 1 and 2 codes are identical. */
		net->error = 1;
		//net->last_errno= ER_NET_PACKET_TOO_LARGE;
#ifdef MYSQL_SERVER
		my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
#endif
		return (1);
	}
	pkt_length = (length + IO_SIZE - 1) & ~(IO_SIZE - 1);
	/*
	  We must allocate some extra bytes for the end 0 and to be able to
	  read big compressed blocks + 1 safety byte since uint3korr() in
	  my_real_read() may actually read 4 bytes depending on build flags and
	  platform.
	*/
	if (!(buff = (uchar*) realloc((char*) net->buff, pkt_length + NET_HEADER_SIZE + COMP_HEADER_SIZE + 1)))
	{
		/* @todo: 1 and 2 codes are identical. */
		net->error = 1;
		//net->last_errno= ER_OUT_OF_RESOURCES;
		/* In the server the error is reported by MY_WME flag. */
		return (1);
	}
	net->buff = net->write_pos = buff;
	net->buff_end = buff + (net->max_packet = (ulong) pkt_length);
	return (0);
}


/**
  Check if there is any data to be read from the socket.

  @param sd   socket descriptor

  @retval
    0  No data to read
  @retval
    1  Data or EOF to read
  @retval
    -1   Don't know if data is ready or not
*/

#if !defined(EMBEDDED_LIBRARY)

static int net_data_is_ready(my_socket sd)
{
#ifdef HAVE_POLL
	struct pollfd ufds;
	int res;

	ufds.fd = sd;
	ufds.events = POLLIN | POLLPRI;
	if (!(res = poll(&ufds, 1, 0)))
		return 0;
	if (res < 0 || !(ufds.revents & (POLLIN | POLLPRI)))
		return 0;
	return 1;
#else
	fd_set sfds;
	struct timeval tv;
	int res;

#ifndef __WIN__
	/* Windows uses an _array_ of 64 fd's as default, so it's safe */
	if (sd >= FD_SETSIZE)
		return -1;
#define NET_DATA_IS_READY_CAN_RETURN_MINUS_ONE
#endif

	FD_ZERO(&sfds);
	FD_SET(sd, &sfds);

	tv.tv_sec = tv.tv_usec = 0;

	if ((res = select((int) (sd + 1), &sfds, NULL, NULL, &tv)) < 0)
		return 0;
	else
		return test(res ? FD_ISSET(sd, &sfds) : 0);
#endif /* HAVE_POLL */
}

#endif /* EMBEDDED_LIBRARY */

/**
  Remove unwanted characters from connection
  and check if disconnected.

    Read from socket until there is nothing more to read. Discard
    what is read.

    If there is anything when to read 'net_clear' is called this
    normally indicates an error in the protocol.

    When connection is properly closed (for TCP it means with
    a FIN packet), then select() considers a socket "ready to read",
    in the sense that there's EOF to read, but read() returns 0.

  @param net			stNET handler
  @param clear_buffer           if <> 0, then clear all data from comm buff
*/

void netClear(stNET *net, my_bool clear_buffer)
{
#if !defined(EMBEDDED_LIBRARY)
	size_t count;
	int ready;
#endif
	log_info("netClear");

#if !defined(EMBEDDED_LIBRARY)
	if (clear_buffer)
	{
		while ((ready = net_data_is_ready(net->vio->sd)) > 0)
		{
			/* The socket is ready */
			if ((long) (count = vioRead(net->vio, net->buff,
			                             (size_t) net->max_packet)) > 0)
			{
				log_info("skipped %ld bytes from file: %s",
				                    (long) count, vioDescription(net->vio));
			}
			else
			{
				log_info("socket ready but only EOF to read - disconnected");
				net->error = 2;
				break;
			}
		}
	}
#endif /* EMBEDDED_LIBRARY */
	net->pkt_nr = net->compress_pkt_nr = 0;		/* Ready for new command */
	net->write_pos = net->buff;
	return;
}


/** Flush write_buffer if not empty. */

bool netFlush(stNET *net)
{
	my_bool error = 0;
	log_info("netFlush");
	if (net->buff != net->write_pos)
	{
		error = test(netRealWrite(net, net->buff,
		                            (size_t) (net->write_pos - net->buff)));
		net->write_pos = net->buff;
	}
	/* Sync packet number if using compression */
	if (net->compress)
		net->pkt_nr = net->compress_pkt_nr;
	return (error);
}


/*****************************************************************************
** Write something to server/client buffer
*****************************************************************************/

/**
  Write a logical packet with packet header.

  Format: Packet length (3 bytes), packet number(1 byte)
  When compression is used a 3 byte compression length is added

  @note
    If compression is used the original package is modified!
*/

bool
myNetWrite(stNET *net, const uchar *packet, size_t len)
{
	uchar buff[NET_HEADER_SIZE];
	if (unlikely(!net->vio)) /* nowhere to write */
		return 0;
	/*
	  Big packets are handled by splitting them in packets of MAX_PACKET_LENGTH
	  length. The last packet is always a packet that is < MAX_PACKET_LENGTH.
	  (The last packet may even have a length of 0)
	*/
	while (len >= MAX_PACKET_LENGTH)
	{
		const ulong z_size = MAX_PACKET_LENGTH;
		int3store(buff, z_size);
		buff[3] = (uchar) net->pkt_nr++;
		if (net_write_buff(net, buff, NET_HEADER_SIZE) ||
		        net_write_buff(net, packet, z_size))
			return 1;
		packet += z_size;
		len -=     z_size;
	}
	/* Write last packet */
	int3store(buff, len);
	buff[3] = (uchar) net->pkt_nr++;
	if (net_write_buff(net, buff, NET_HEADER_SIZE))
		return 1;
	return test(net_write_buff(net, packet, len));
}

/**
  Send a command to the server.

    The reason for having both header and packet is so that libmysql
    can easy add a header to a special command (like prepared statements)
    without having to re-alloc the string.

    As the command is part of the first data packet, we have to do some data
    juggling to put the command in there, without having to create a new
    packet.

    This function will split big packets into sub-packets if needed.
    (Each sub packet can only be 2^24 bytes)

  @param net		stNET handler
  @param command	Command in MySQL server (enum client_command)
  @param header	Header to write after command
  @param head_len	Length of header
  @param packet	Query or parameter to query
  @param len		Length of packet

  @retval
    0	ok
  @retval
    1	error
*/

bool
netWriteCommand(stNET *net, uchar command,
                  const uchar *header, size_t head_len,
                  const uchar *packet, size_t len)
{
	size_t length = len + 1 + head_len;			/* 1 extra byte for command */
	uchar buff[NET_HEADER_SIZE + 1];
	uint header_size = NET_HEADER_SIZE + 1;
	log_info("netWriteCommand");
	log_info("length: %lu", (ulong) len);

	buff[4] = command;				/* For first packet */

	if (length >= MAX_PACKET_LENGTH)
	{
		/* Take into account that we have the command in the first header */
		len = MAX_PACKET_LENGTH - 1 - head_len;
		do
		{
			int3store(buff, MAX_PACKET_LENGTH);
			buff[3] = (uchar) net->pkt_nr++;
			if (net_write_buff(net, buff, header_size) ||
			        net_write_buff(net, header, head_len) ||
			        net_write_buff(net, packet, len))
				return (1);
			packet += len;
			length -= MAX_PACKET_LENGTH;
			len = MAX_PACKET_LENGTH;
			head_len = 0;
			header_size = NET_HEADER_SIZE;
		} while (length >= MAX_PACKET_LENGTH);
		len = length;					/* Data left to be written */
	}
	int3store(buff, length);
	buff[3] = (uchar) net->pkt_nr++;
	return (test(net_write_buff(net, buff, header_size) ||
	                 (head_len && net_write_buff(net, header, head_len)) ||
	                 net_write_buff(net, packet, len) || netFlush(net)));
}

/**
  Caching the data in a local buffer before sending it.

   Fill up net->buffer and send it to the client when full.

    If the rest of the to-be-sent-packet is bigger than buffer,
    send it in one big block (to avoid copying to internal buffer).
    If not, copy the rest of the data to the buffer and return without
    sending data.

  @param net		Network handler
  @param packet	Packet to send
  @param len		Length of packet

  @note
    The cached buffer can be sent as it is with 'net_flush()'.
    In this code we have to be careful to not send a packet longer than
    MAX_PACKET_LENGTH to net_real_write() if we are using the compressed
    protocol as we store the length of the compressed packet in 3 bytes.

  @retval
    0	ok
  @retval
    1
*/

static bool net_write_buff(stNET *net, const uchar *packet, ulong len)
{
	ulong left_length;
	if (net->compress && net->max_packet > MAX_PACKET_LENGTH)
		left_length = (ulong) (MAX_PACKET_LENGTH - (net->write_pos - net->buff));
	else
		left_length = (ulong) (net->buff_end - net->write_pos);

	if (len > left_length)
	{
		if (net->write_pos != net->buff)
		{
			/* Fill up already used packet and write it */
			memcpy((char*) net->write_pos, packet, left_length);
			if (netRealWrite(net, net->buff,
			                   (size_t) (net->write_pos - net->buff) + left_length))
				return 1;
			net->write_pos = net->buff;
			packet += left_length;
			len -= left_length;
		}
		if (net->compress)
		{
			/*
			We can't have bigger packets than 16M with compression
			Because the uncompressed length is stored in 3 bytes
			     */
			left_length = MAX_PACKET_LENGTH;
			while (len > left_length)
			{
				if (netRealWrite(net, packet, left_length))
					return 1;
				packet += left_length;
				len -= left_length;
			}
		}
		if (len > net->max_packet)
			return netRealWrite(net, packet, len) ? 1 : 0;
		/* Send out rest of the blocks as full sized blocks */
	}
	memcpy((char*) net->write_pos, packet, len);
	net->write_pos += len;
	return 0;
}


/**
  Read and write one packet using timeouts.
  If needed, the packet is compressed before sending.

  @todo
    - TODO is it needed to set this variable if we have no socket
*/

int
netRealWrite(stNET *net, const uchar *packet, size_t len)
{
	size_t length;
	const uchar *pos, *end;

	uint retry_count = 0;
	my_bool net_blocking = vioIsBlocking(net->vio);
	log_info("netRealWrite");



	if (net->error == 2)
		return (-1);				/* socket can't be used */

	net->reading_or_writing = 2;


	pos = packet;
	end = pos + len;
	while (pos != end)
	{
		if ((long) (length = vioWrite(net->vio, pos, (size_t) (end - pos))) <= 0)
		{
			my_bool interrupted = vioShouldRetry(net->vio);

			net->error = 2;				/* Close socket */

			break;
		}
		pos += length;
		//update_statistics(thd_increment_bytes_sent(length));
	}
#ifndef __WIN__
end:
#endif


	net->reading_or_writing = 0;
	return(((int) (pos != end)));
}



/**
  Reads one packet to net->buff + net->where_b.
  Long packets are handled by my_net_read().
  This function reallocates the net->buff buffer if necessary.

  @return
    Returns length of packet.
*/

static ulong
my_real_read(stNET *net, size_t *complen)
{
	uchar *pos;
	size_t length;
	uint i, retry_count = 0;
	ulong len = packet_error;

	my_bool net_blocking = vioIsBlocking(net->vio);
	uint32 remain = (net->compress ? NET_HEADER_SIZE + COMP_HEADER_SIZE :	  NET_HEADER_SIZE);
	*complen = 0;

	net->reading_or_writing = 1;


	pos = net->buff + net->where_b;		/* net->packet -4 */
	
	for (i = 0 ; i < 2 ; i++)
	{
		while (remain > 0)
		{
			/* First read is done with non blocking mode */
			if ((long) (length = vioRead(net->vio, pos, remain)) <= 0L)
			{
				my_bool interrupted = vioShouldRetry(net->vio);

				len = packet_error;
				net->error = 2;				/* Close socket */

				goto end;
			}
			remain -= (uint32) length;
			pos += length;
			//update_statistics(thd_increment_bytes_received(length));
		}
		if (i == 0)
		{	/* First parts is packet length */
			ulong helping;
			log_info("where_b :%d ,net->pkt_nr =%d ", net->where_b, net->pkt_nr);
			if (net->buff[net->where_b + 3] != (uchar) net->pkt_nr)
			{
				if (net->buff[net->where_b] != (uchar) 255)
				{
					log_error("Packets out of order (Found: %d, expected %u)", (int) net->buff[net->where_b + 3],	net->pkt_nr);
					/*
					      We don't make noise server side, since the client is expected
					      to break the protocol for e.g. --send LOAD DATA .. LOCAL where
					      the server expects the client to send a file, but the client
					      may reply with a new command instead.
					    */
				}
				len = packet_error;
				/* Not a stNET error on the client. XXX: why? */

				goto end;
			}
			net->compress_pkt_nr = ++net->pkt_nr;

			len = uint3korr(net->buff + net->where_b);
			if (!len)				/* End of big multi-packet */
				goto end;
			helping = max(len, *complen) + net->where_b;
			/* The necessary size of net->buff */
			if (helping >= net->max_packet)
			{
				if (netRealloc(net, helping))
				{
					len = packet_error;         /* Return error and close connection */
					goto end;
				}
			}
			pos = net->buff + net->where_b;
			remain = (uint32) len;
		}
	}

end:

	net->reading_or_writing = 0;
	return(len);
}


/**
  Read a packet from the client/server and return it without the internal
  package header.

  If the packet is the first packet of a multi-packet packet
  (which is indicated by the length of the packet = 0xffffff) then
  all sub packets are read and concatenated.

  If the packet was compressed, its uncompressed and the length of the
  uncompressed packet is returned.

  @return
  The function returns the length of the found packet or packet_error.
  net->read_pos points to the read data.
*/

ulong
myNetRead(stNET *net)
{
	size_t len, complen;

	len = my_real_read(net, &complen);
	if (len == MAX_PACKET_LENGTH)
	{
		/* First packet of a multi-packet.  Concatenate the packets */
		ulong save_pos = net->where_b;
		size_t total_length = 0;
		do
		{
			net->where_b += len;
			total_length += len;
			len = my_real_read(net, &complen);
		} while (len == MAX_PACKET_LENGTH);
		if (len != packet_error)
			len += total_length;
		net->where_b = save_pos;
	}

	net->read_pos = net->buff + net->where_b;
	if (len != packet_error)
		net->read_pos[len] = 0;		/* Safeguard for mysql_use_result */

	return len;

}


void netSetReadTimeout(stNET *net, uint timeout)
{
	log_info("netSetReadTimeout");
	log_info("timeout: %d", timeout);
	net->read_timeout = timeout;
#ifdef NO_ALARM
	if (net->vio)
		vioTimeout(net->vio, 0, timeout);
#endif
	return;
}


void netSetWriteTimeout(stNET *net, uint timeout)
{
	log_info("netSetWriteTimeout");
	log_info("timeout: %d", timeout);
	net->write_timeout = timeout;
#ifdef NO_ALARM
	if (net->vio)
		vioTimeout(net->vio, 1, timeout);
#endif
	return;
}

