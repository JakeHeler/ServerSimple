/*
   Copyright (c) 2007, 2012, Oracle and/or its affiliates. All rights reserved.

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
  Functions to autenticate and handle reqests for a connection
*/

#include "./json/document.h"
#include "./json/writer.h"


#include "my_global.h"
#include "net_comm.h"

#include "scheduler.h"
#include "violite.h"
#include "my_pthread.h"

#include "NotifyMessage.h"
#include "JsonDefine.h"


#include <mysql.h>

#include "aes.hpp"
#include "curve25519-donna.hpp"

// For encrypted data in commnication , we use the CBC method and the Diffie -Hellman key exchange
#define	ENABLE_DATA_ENCRYPTION_IN_COMMUNICATION	0


unsigned char baseKey[32]={0x00,};




#ifdef __cplusplus
extern "C" {
#endif

extern void close_connection(THD *thd, uint errcode, bool lock);
extern int StartNotification( char* packet);
extern void StopNotification( int id);


#ifdef __cplusplus
}
#endif


/*
  Get structure for logging connection data for the current user
*/

bool do_command(THD *thd);
bool dispatch_command(enum client_command command, THD *thd,
                      char* packet, uint packet_length);

void net_end_statement(THD *thd);
int SaveToken( char* packet, uint* uid);
int sendKeytoClient(THD *thd,char* packet);
uint DecryptData(THD *thd,char* packet,uint packet_len);

/*
  Close an established connection

  NOTES
    This mainly updates status variables
*/

static void end_connection(THD *thd)
{

}

/*
  Thread handler for a connection

  SYNOPSIS
    handle_one_connection()
    arg		Connection object (THD)

  IMPLEMENTATION
    This function (normally) does the following:
    - Initialize thread
    - Initialize THD to be used with this thread
    - Authenticate user
    - Execute all queries sent on the connection
    - Take connection down
    - End thread  / Handle next connection using thread from thread cache
*/

pthread_handler_t handle_one_connection(void *arg)
{

	THD *thd = (THD*) arg;

	thd->thr_create_utime = my_micro_time();


	if (thread_scheduler.init_new_connection_thread())
	{
		log_info("Thread Return ");
		close_connection(thd, 1041, 1);
		//statistic_increment(aborted_connects,&LOCK_status);
		thread_scheduler.end_thread(thd, 0);
		return 0;
	}

	/*
		If a thread was created to handle this connection:
		increment slow_launch_threads counter if it took more than
		slow_launch_time seconds to create the thread.
	*/
	if (thd->prior_thr_create_utime)
	{
		ulong launch_time = (ulong) (thd->thr_create_utime - thd->prior_thr_create_utime);
		//if (launch_time >= slow_launch_time*1000000L)
		//  statistic_increment(slow_launch_threads, &LOCK_status);
		thd->prior_thr_create_utime = 0;
	}

	/*
		handle_one_connection() is normally the only way a thread would
		start and would always be on the very high end of the stack ,
		therefore, the thread stack always starts at the address of the
	 first local variable of handle_one_connection, which is thd. We
		need to know the start of the stack so that we could check for
		stack overruns.
	*/

	thd->thread_stack = (char*) &thd;
	

	for (;;)
	{
		stNET *net = &thd->net;
		vioKeepalive(net->vio, TRUE);

		while (!net->error && net->vio != 0 && !(thd->killed == KILL_CON))
		{
			if (do_command(thd))
				break;

		}
		end_connection(thd);


end_thread:
		close_connection(thd, 0, 1);
		if (thread_scheduler.end_thread(thd, 1))
			return 0;                                 // Probably no-threads

		/*
		If end_thread() returns, we are either running with
		thread-handler=no-threads or this thread has been schedule to
		handle the next connection.
		*/
		//thd= current_thd;
		//thd->thread_stack= (char*) &thd;
		return 0;  

	}
}


/**
  Read one command from connection and execute it (query or simple command).
  This function is called in loop from thread function.

  For profiling to work, it must never be called recursively.

  @retval
    0  success
  @retval
    1  request of thread shutdown (see dispatch_command() description)
*/
#define net_new_transaction(net) ((net)->pkt_nr=0)

bool do_command(THD *thd)
{
	bool return_value;
	char *packet = 0;
	ulong packet_length;
	stNET *net = &thd->net;
	enum client_command command;
	log_info("do_command");

	net->pkt_nr = net->compress_pkt_nr = 0; 	/* Ready for new command */

	/*
		This thread will do a blocking read from the client which
		will be interrupted when the next command is received from
		the client, the connection is closed or "net_wait_timeout"
		number of seconds has passed
	*/
	netSetReadTimeout(net, NET_READ_TIME_OUT);

	net_new_transaction(net);

	packet_length = myNetRead(net);
	
	log_info("Thread ID =%ld  Packet Length=%ld", thd->thread_id, packet_length);

	if (packet_length == packet_error)
	{
		log_info("Got error %d reading command from socket %s", net->error, vioDescription(net->vio));

		/* Check if we can continue without closing the connection */

		/* The error must be set. */
		//DBUG_ASSERT(thd->is_error());
		net_end_statement(thd);

		if (net->error != 3)
		{
			return_value = TRUE;                      // We have to close it.
			goto out;
		}

		net->error = 0;
		return_value = FALSE;
		goto out;
	}

	packet = (char*) net->read_pos;
	/*
		'packet_length' contains length of data, as it was stored in packet
		header. In case of malformed header, my_net_read returns zero.
		If packet_length is not zero, my_net_read ensures that the returned
		number of bytes was actually read from network.
		There is also an extra safety measure in my_net_read:
		it sets packet[packet_length]= 0, but only for non-zero packets.
	*/
	if (packet_length == 0)                       /* safety */
	{
		/* Initialize with CCOM_SLEEP packet */
		packet[0] = (uchar) CCOM_SLEEP;
		packet_length = 1;
	}
	/* Do not rely on my_net_read, extra safety against programming errors. */
	packet[packet_length] = '\0';                 /* safety */

	command = (enum client_command) (uchar) packet[0];


	if (command >= CCOM_END)
		command = CCOM_END;				// Wrong command

	log_info("Command on %s = %d ", vioDescription(net->vio), command);

	/* Restore read timeout value */
	netSetReadTimeout(net, NET_READ_TIME_OUT);

	//DBUG_ASSERT(packet_length);
	return_value = dispatch_command(command, thd, packet + 1, (uint) (packet_length - 1));

out:
	return (return_value);
}

 
bool dispatch_command(enum client_command command, THD *thd,
                      char* packet, uint packet_length)
{
	stNET *net = &thd->net;
	bool error = 0;
	log_info("dispatch_command");
	//log_info("Length =%d ,packet: '%s'; command: %d", packet_length, packet, command);
	log_info("Length =%d ; command: %d", packet_length,  command);

	/*
	  Commands which always take a long time are logged into
	  the slow log only if opt_log_slow_admin_statements is set.
	*/
	thd->enable_slow_log = TRUE;
	thd->user_id =0;

	VOID(pthread_mutex_lock(&LOCK_thread_count));


	thread_running++;
	/* TODO: set thd->lex->sql_command to SQLCOM_END here */
	VOID(pthread_mutex_unlock(&LOCK_thread_count));


#if (ENABLE_DATA_ENCRYPTION_IN_COMMUNICATION==1)	
	// In case of hand shaking , we get directly data from clients
	if(command !=CCOM_HAND_SHAKING && command !=CCOM_QUIT  )
	{
	
		int len =DecryptData(thd,packet,packet_length);
		//check if the enrypted data has the right padding data. If packet is wrong . we go to quit mode.
		if(len ==0 || packet_length == len)
			command =CCOM_QUIT;	
		else
			packet_length =len;
	}
#endif

	thd->command = command;	

	/**
	  Clear the set of flags that are expected to be cleared at the
	  beginning of each command.
	*/

	thd->server_status =0;

	rapidjson::Document datastring;

	//Check if json format is real
	bool checked =datastring.Parse(packet).HasParseError();	

	//Chcek if both json format is real and it is objected. Very important that both cases must be checked. 
	if(  (command ==CCOM_START_NOTIFICATION || command ==CCOM_STOP_NOTIFICATION ||command ==CCOM_SAVE_TOKEN)   \
		&&  (packet ==NULL || checked ==TRUE  || (datastring.IsObject() ==0)) )
	{
		net->error=0;				
    		error=TRUE;
		thd->server_status	=CLIENT_REQUEST_RET_NOT_JSON_FORMAT;			
		goto here;
	}

	switch (command) {

		//Create thread for notification of Messages
		case CCOM_START_NOTIFICATION:
		{	
			if( datastring.HasMember(JSON_NAME_USER_INDEX) && datastring[JSON_NAME_USER_INDEX].IsInt())
			{
				if ( 0 == access( CHECK_SERVER_FILE_NAME, F_OK))
					thd->server_status =CLIENT_REQUEST_RET_NOT_SERVICE_BY_CHECKING_SERVER;
				else
				{
					if ( 0 == access( NOT_SERVICE_TIME_FILE_NAME, F_OK))
						thd->server_status =RET_NOT_SERVICE_TIME;
					else
						thd->server_status =StartNotification(packet);
				}	
			}	
			else
			{
				net->error=0;				
    				error=TRUE;
				thd->server_status	=CLIENT_REQUEST_RET_NOT_JSON_FORMAT;				
			}

			break;
		}
		case CCOM_STOP_NOTIFICATION:
		{	//status_var_increment(thd->status_var.com_other);
			//my_ok(thd);				// Tell client we are alive


			unsigned int id ;

			if(  datastring.HasMember(JSON_NAME_USER_INDEX) && datastring[JSON_NAME_USER_INDEX].IsInt())
			{
				id =datastring[JSON_NAME_USER_INDEX].GetInt(); 
				StopNotification(id);
			}	
			else
			{
				net->error=0;				// Don't give 'abort' message
    				error=TRUE;
				thd->server_status	=CLIENT_REQUEST_RET_NOT_JSON_FORMAT;						
				break;
			}

		}
			break;
		case CCOM_SAVE_TOKEN:
		{
			thd->server_status =SaveToken(packet,&thd->user_id);

		}
			break;

		case CCOM_HAND_SHAKING:
		{
			
			thd->server_status =sendKeytoClient(thd,packet);

		}
			break;

					
		//Stop the thread
		case CCOM_END:
		case CCOM_QUIT:
		{
			net->error=0;				// Don't give 'abort' message
			error=TRUE;
			break;

		}
			
		default:
			//my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
			break;
	}

here:

	net_end_statement(thd);

	thd->command = CCOM_SLEEP;

	VOID(pthread_mutex_lock(&LOCK_thread_count)); // For process list
	thread_running--;
	VOID(pthread_mutex_unlock(&LOCK_thread_count));

	return (error);
}

#define	WRITE_HEADER_BYTE		2
void net_end_statement(THD *thd)
{

	stNET *net= &thd->net;
	uchar buffer[1024];

	bool error = FALSE;
	int strsize=0;

	if(thd->server_status !=0)
		log_error("Error code =%d ",thd->server_status);
	
	if( thd->command==CCOM_QUIT ||thd->command==CCOM_END)
		return;

	memset(buffer,0x00,sizeof(buffer));
	
	std::string json_data ;
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);
	


	writer.StartObject();	// this means '{'
	
	writer.String(JSON_NAME_RETURN_STATUS);
	writer.Int(thd->server_status);

	if ( thd->server_status ==CLIENT_REQUEST_RET_NOT_SERVICE_BY_CHECKING_SERVER)
	{
		#define  BUFF_SIZE   256

		char   buff[BUFF_SIZE]={0x00,};
		int    fd;
		
		if ( 0 < ( fd = open( (char *)CHECK_SERVER_FILE_NAME, O_RDONLY)))
		{
		   	read( fd, buff, BUFF_SIZE);
		   	writer.String(JSON_NAME_RETURN_STATUS_CONTENTS);
			writer.String(buff);
		   	close( fd);
		}

	}

	if( thd->user_id !=0)
	{
		writer.String(JSON_NAME_USER_INDEX);
		writer.Int(thd->user_id);

	}
	writer.EndObject(); // this means '}'
	json_data = s.GetString();

	//For clients to get delimited message , we insert stopword null character.
	//json_data.insert(json_data.end(),'\0');

	//Add 1  to include the null character '\0' 
	strsize =json_data.size() +1;

	
#if (ENABLE_DATA_ENCRYPTION_IN_COMMUNICATION==1)	

	unsigned int pad_len=0;
    	struct AES_ctx ctx;    
	AES_init_ctx_iv(&ctx, net->aesKey, &net->aesKey[AES_KEYLEN]);

	char *in=PaddingDataPKCS7((unsigned char *)json_data.c_str(),strsize,&pad_len);
	AES_CBC_encrypt_buffer(&ctx, (unsigned char *)in, pad_len);
	
	// Length is 2 bytes
	int2store(buffer, pad_len);
	
	memcpy(&buffer[WRITE_HEADER_BYTE],in,pad_len);

	error= netRealWrite(net, (const uchar *)buffer, pad_len+WRITE_HEADER_BYTE);
	if (!error)
  		error= netFlush(net);


	free(in);
#else
	error= netRealWrite(net, (const uchar *)json_data.c_str(), strsize);
	if (!error)
		error= netFlush(net);

#endif
}


int SaveToken( char* packet, uint* uid)
{
	int errors=0;
	rapidjson::Document datastring;
	datastring.Parse(packet);

	std::string token,manufacturer,model,version, appver,type;

	if(  datastring.HasMember(JSON_NAME_TOKENS) && datastring[JSON_NAME_TOKENS].IsString())
		token =datastring[JSON_NAME_TOKENS].GetString();
	else
		return CLIENT_REQUEST_RET_NOT_JSON_FORMAT;

	if(  datastring.HasMember(JSON_NAME_DEVICE_MANUFACTURER)&& datastring[JSON_NAME_DEVICE_MANUFACTURER].IsString())
		manufacturer =datastring[JSON_NAME_DEVICE_MANUFACTURER].GetString();
	else
		return CLIENT_REQUEST_RET_NOT_JSON_FORMAT;

	if(  datastring.HasMember(JSON_NAME_DEVICE_MODEL)&& datastring[JSON_NAME_DEVICE_MODEL].IsString())
		model =datastring[JSON_NAME_DEVICE_MODEL].GetString();
	else
		return CLIENT_REQUEST_RET_NOT_JSON_FORMAT;

	if(  datastring.HasMember(JSON_NAME_DEVICE_VERSION)&& datastring[JSON_NAME_DEVICE_VERSION].IsString())
		version =datastring[JSON_NAME_DEVICE_VERSION].GetString();
	else
		return CLIENT_REQUEST_RET_NOT_JSON_FORMAT;

	if(  datastring.HasMember(JSON_NAME_DEVICE_APP_VERSION)&& datastring[JSON_NAME_DEVICE_APP_VERSION].IsString())
		appver =datastring[JSON_NAME_DEVICE_APP_VERSION].GetString();
	else
		return CLIENT_REQUEST_RET_NOT_JSON_FORMAT;

	if(  datastring.HasMember(JSON_NAME_DEVICE_TYPE)&& datastring[JSON_NAME_DEVICE_TYPE].IsString())
		type =datastring[JSON_NAME_DEVICE_TYPE].GetString();
	else
		return CLIENT_REQUEST_RET_NOT_JSON_FORMAT;


	MYSQL *con;
	MYSQL_RES *res;
	MYSQL_ROW record;


	int col_count = 0;
	int row_count = 0;

	//Must Use localhost instead 127.0.0.1
	const char *host ="localhost";

	const char *user =MYSQL_USER;
	const char *pw =MYSQL_USER_PW;
	const char *database =MYSQL_DATABASE;
	const char *socket=MYSQL_SOCKET;

	con = mysql_init(NULL); 
	if(!con)	
	{
		log_error("Can't Init Mysql ");
		return CLIENT_REQUEST_RET_OPEN_ERROR;
	}

	if(!mysql_real_connect(con, host, user, pw, database, 3306, socket, 0))
	{
		log_error("Connect Error %u (%s) : %s \n", mysql_errno(con), mysql_sqlstate(con), mysql_error(con));
		mysql_close(con);
		return CLIENT_REQUEST_RET_OPEN_ERROR;
	}

	std::string table;
	char query[1024]={0,};

	table = "Users";
	table +="(UserIndex,DeviceToken,DeviceManufacturer,DeviceModel,DeviceVersion,AppVersion,DeviceType,RegisterDateTime)";
	sprintf(query, "INSERT INTO %s VALUES (0,'%s','%s','%s','%s','%s','%s',now())",table.c_str(),token.c_str(),manufacturer.c_str(),model.c_str(),version.c_str(),appver.c_str(),type.c_str());


	if(mysql_query(con, query))
	{	
		log_error("Error %u (%s) : %s \n", mysql_errno(con), mysql_sqlstate(con), mysql_error(con));
		return CLIENT_REQUEST_RET_QUERY_ERROR;
	}
	else
	{
		* uid =mysql_insert_id(con);
	}
	log_info("Insert ID =%d",* uid );

	return CLIENT_REQUEST_RET_OK;
}


int sendKeytoClient(THD *thd,char* packet)
{

	stNET *net= &thd->net;
	bool error = FALSE;

	uint8 aeskey[AES_KEY_SIZE],personalkeys[AES_KEY_SIZE],basepoints[AES_KEY_SIZE],pubkeys[AES_KEY_SIZE];

	memset(aeskey,0x00,AES_KEY_SIZE);
	memset(personalkeys,0x00,AES_KEY_SIZE);
	memset(basepoints,0x00,AES_KEY_SIZE);

	memset(pubkeys,0x00,AES_KEY_SIZE);
	
	KeyGenerator(personalkeys,AES_KEY_SIZE);

	curve25519_donna(aeskey,personalkeys,(uint8*)packet);
	memcpy(net->aesKey,aeskey,AES_KEY_SIZE);	

	//Save AES Key for decryption and encryption
	memcpy(basepoints,baseKey,AES_KEY_SIZE);

	curve25519_donna(pubkeys,personalkeys,basepoints);

	error= netRealWrite(net, (const uchar *)pubkeys, sizeof(pubkeys));
	if (!error)
  		error= netFlush(net);


	return CLIENT_REQUEST_RET_OK;
}

uint DecryptData(THD *thd,char* packet,uint packet_len)
{
	stNET *net= &thd->net;

	// if packet length is multiples of  AES_KEYLEN
	if(  packet_len%AES_KEYLEN  !=0 || packet_len ==0)
		return 0;
	
    	struct AES_ctx ctx;    
	AES_init_ctx_iv(&ctx, net->aesKey, &net->aesKey[AES_KEYLEN]);
	AES_CBC_decrypt_buffer(&ctx,(unsigned char *)packet,packet_len); 

	unsigned int real_length =UnPaddingDataPKCS7((unsigned char *)packet,packet_len); 
	//log_info("DecryptData =%s",packet);

	log_info("DecryptData real_length=%d",real_length);

	return real_length;

}

