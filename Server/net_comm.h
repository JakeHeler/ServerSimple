#ifndef _MY_NET_COMM_H
#define _MY_NET_COMM_H

#include "my_global.h"

//#include "aes.h"
//Printf Debug
//#include "log.h"
#define IO_SIZE			4096


struct st_vio;					/* Only C */
typedef struct st_vio Vio;


///////////////////////////////////////////

/////////////////////////////////////////////////////////
// Netwok Variables
/////////////////////////////////////////////////////////
#define NET_BUFFER_LENGTH	/*16384*/	1024*2
#define MAX_ALLOWED_PACKET	/*1024*1024L */	NET_BUFFER_LENGTH


/* Constants when using compression */
#define NET_HEADER_SIZE 		4		/* standard header size */
#define COMP_HEADER_SIZE 	3		/* compression header extra size */

#define MYSQL_ERRMSG_SIZE	512
#define NET_READ_TIME_OUT		60		/* Timeout on read */
#define NET_WRITE_TIME_OUT	60		/* Timeout on write */
#define NET_WAIT_TIMEOUT		8*60*60		/* Wait for new query */
#define ONLY_KILL_QUERY         	1
#define NET_RETRY_COUNT  		10	///< Abort read after this many int.




enum client_command
{
  	CCOM_SLEEP, 
	CCOM_QUIT, 
	CCOM_START_NOTIFICATION, 
	CCOM_STOP_NOTIFICATION, 
	CCOM_SAVE_TOKEN, 
	CCOM_HAND_SHAKING, 
	/* Must be last */
  	CCOM_END
};



#define packet_error (~(unsigned long) 0)


#ifdef __cplusplus
extern "C" {
#endif

#define	AES_KEYLEN		16

//This include the size of aes key and iv key 
#define	AES_KEY_SIZE	AES_KEYLEN*2

typedef struct stNet {

  Vio *vio;
  unsigned char *buff,*buff_end,*write_pos,*read_pos;
  my_socket fd;

	unsigned char aesKey[AES_KEY_SIZE];
  /* For Perl DBI/dbd */
  /*
    The following variable is set if we are doing several queries in one
    command ( as in LOAD TABLE ... FROM MASTER ),
    and do not want to confuse the client with OK at the wrong time
  */
  unsigned long remain_in_buf,length, buf_length, where_b;
  unsigned long max_packet,max_packet_size;
  unsigned int pkt_nr,compress_pkt_nr;
  unsigned int write_timeout, read_timeout, retry_count;
  int fcntl;
  unsigned int *return_status;
  unsigned char reading_or_writing;
  char save_char;
  my_bool unused0; /* Please remove with the next incompatible ABI change. */
  my_bool unused; /* Please remove with the next incompatible ABI change */
  my_bool compress;
  my_bool unused1; /* Please remove with the next incompatible ABI change. */
  /*
    Pointer to query object in query cache, do not equal NULL (0) for
    queries in cache that have not stored its results yet
  */
#if 1

  /*
    'query_cache_query' should be accessed only via query cache
    functions and methods to maintain proper locking.
  */
  unsigned char *query_cache_query;
  unsigned int last_errno;
  unsigned char error; 
  my_bool unused2; /* Please remove with the next incompatible ABI change. */
  my_bool return_errno;
  /** Client library error message buffer. Actually belongs to struct MYSQL. */
  //char last_error[MYSQL_ERRMSG_SIZE];

  /** Client library sqlstate buffer. Set along with the error message. */
  //char sqlstate[SQLSTATE_LENGTH+1];

  void *extension;

  /*
    Controls whether a big packet should be skipped.

    Initially set to FALSE by default. Unauthenticated sessions must have
    this set to FALSE so that the server can't be tricked to read packets
    indefinitely.
  */
  my_bool skip_big_packet;
#endif
} stNET;



bool	netInit(stNET *net, Vio* vio);
void	netLocalInit(stNET *net);
void	netEnd(stNET *net);
void	netClear(stNET *net, my_bool clear_buffer);
bool netRealloc(stNET *net, size_t length);
bool	netFlush(stNET *net);
bool	myNetWrite(stNET *net,const unsigned char *packet, size_t len);
bool	netWriteCommand(stNET *net,unsigned char command,
			  const unsigned char *header, size_t head_len,
			  const unsigned char *packet, size_t len);
int	netRealWrite(stNET *net,const unsigned char *packet, size_t len);
unsigned long myNetRead(stNET *net);


void netSetWriteTimeout(stNET *net, uint timeout);
void netSetReadTimeout(stNET *net, uint timeout);



ulonglong my_getsystime();
time_t my_time(myf flags __attribute__((unused)));
ulonglong my_micro_time();





#ifdef __cplusplus
}
#endif




#endif //_MY_NET_COMM_H