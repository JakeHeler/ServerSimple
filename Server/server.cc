
#include <map>
#include <new>
#include <iostream>
#include <utility>
#include <sys/stat.h>


#include "my_global.h"
#include "net_comm.h"

#include "scheduler.h"
#include "violite.h"
#include "my_pthread.h"



#include "NotifyMessage.h"


#include <signal.h>


#include "JsonDefine.h"
#include "./json/document.h"


using namespace std;


#define SOCKET_NAME    "/var/dmsocket"


static map <unsigned int , NotifyMessage* > mapNotifyMessageHandle;


//TestClass*  mapNotifyMessageHandle[110000];


#define MAX_ACCEPT_RETRY	10	// Test accept this many times


scheduler_functions thread_scheduler;

unsigned int my_port = 9877;
unsigned int my_port_timeout = 1000;
unsigned long my_bind_addr;
unsigned long  back_log;
ulong max_connections = 1000000;

static ulong killed_threads, thread_created;

my_bool abort_loop;

int ip_sock;
int socket_error;


uint connection_count = 0;
ulong max_used_connections;
ulong thread_id = 1L;

uint volatile thread_count, thread_running;

pthread_mutex_t	LOCK_connection_count, LOCK_thread_count;
pthread_mutex_t	LOCK_SetupNoti;

pthread_attr_t connection_attrib;

ulong   my_thread_stack_size= 64*1024;

pthread_t signal_thread;

static bool volatile  signal_thread_in_use;

unsigned int	USER_USE_START_NUMBER=500000;

static void network_init(void);
static void create_new_thread(THD *thd);
static int init_thread_environment();

int handle_connections_sockets(void *arg __attribute__((unused)));
bool create_thread_to_handle_connection(THD *thd);
bool one_thread_per_connection_end(THD *thd, bool put_in_cache);

#ifdef __cplusplus
extern "C" {
#endif

void close_connection(THD *thd, uint errcode, bool lock);

int StartNotification( char* packet);
void StopNotification( unsigned int id);

#ifdef __cplusplus
}
#endif



pthread_handler_t handle_one_connection(void *arg);
pthread_handler_t signal_hand(void *arg);

static void init_signals(void);
static void start_signal_handler(void);


static bool init_dummy(void) {
	return 0;
}

//Register Fucntion
void one_thread_per_connection_scheduler(scheduler_functions* func)
{
	func->max_threads = max_connections;
	func->init_new_connection_thread = init_dummy;
	func->add_connection = create_thread_to_handle_connection;
	func->end_thread = one_thread_per_connection_end;
}



/*
* Main to deal connections of clients.
*/
int main()
{
	//memset(NotifyMessageHandle,0x00,sizeof(NotifyMessageHandle));

	log_init();

	init_signals();
	
	log_info("!!! Main Start !!! ");

	init_thread_environment();
	//start_signal_handler();
	
	one_thread_per_connection_scheduler(&thread_scheduler);

	//Initialize Network
	network_init();

	handle_connections_sockets(0);

	log_error("!!! Fatal Error : Main Exit !!! ");

	return 0;
}

static void init_signals(void)
{

	sigset_t set;
	(void) sigemptyset(&set);

	signal(SIGPIPE,SIG_IGN);
	sigaddset(&set, SIGPIPE);

	// Block the signal of "Ctl+\"	
	sigaddset(&set, SIGQUIT);

	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGTERM);

	// Block the signal of "Ctl+c"
	sigaddset(&set, SIGINT);

	// Block the signal of "Ctl+z"
	(void) sigaddset(&set, SIGTSTP);
	
	sigprocmask(SIG_SETMASK, &set, NULL);	
	pthread_sigmask(SIG_SETMASK, &set, NULL);

}

#if 0
static void start_signal_handler(void)
{
	int error;
	pthread_attr_t thr_attr;
	log_info("start_signal_handler");

	(void) pthread_attr_init(&thr_attr);

	pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
	(void) pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);


	pthread_attr_setstacksize(&thr_attr, my_thread_stack_size);


	(void) pthread_mutex_lock(&LOCK_thread_count);
	if ((error = pthread_create(&signal_thread, &thr_attr, signal_hand, 0)))
	{
		log_error("Can't create interrupt-thread (error %d, errno: %d)",   error, errno);
		exit(1);
	}
	
	pthread_mutex_unlock(&LOCK_thread_count);

	(void) pthread_attr_destroy(&thr_attr);

	return;

}

pthread_handler_t signal_hand(void *arg __attribute__((unused)))
{
	sigset_t set;
	int sig;
	
	log_info("signal_hand");
	signal_thread_in_use = 1;

		// Setup up SIGINT for debug
	(void) sigemptyset(&set);


	(void) sigaddset(&set, SIGQUIT);
	(void) sigaddset(&set, SIGHUP);

	(void) sigaddset(&set, SIGTERM);
	(void) sigaddset(&set, SIGTSTP);


	/*
	  signal to start_signal_handler that we are ready
	  This works by waiting for start_signal_handler to free mutex,
	  after which we signal it that we are ready.
	  At this pointer there is no other threads running, so there
	  should not be any other pthread_cond_signal() calls.
	*/
	(void) pthread_mutex_lock(&LOCK_thread_count);
	(void) pthread_mutex_unlock(&LOCK_thread_count);


	(void) pthread_sigmask(SIG_BLOCK, &set, NULL);
	for (;;)
	{
		int error;					// Used when debugging

		while ((error = sigwait(&set, &sig)) == EINTR) ;
	
	
		log_info("Signal_hand Sig =%d",sig);		
		switch (sig) {
			case SIGTERM:
			case SIGQUIT:
			case SIGKILL:

				if (!abort_loop)
				{
					abort_loop = 1;				// mark abort for threads

					//kill_server((void*) sig);	// MIT THREAD has a alarm thread

				}
				break;
			case SIGHUP:

				break;
			default:

				break;					/* purecov: tested */
		}
	}
	return(0);							/* purecov: deadcode */
}
#endif

static void network_init(void)
{

	struct sockaddr_un 	server;
	int	arg = 1;
	int	ret = 0;
	uint	waited;
	uint	this_wait;
	uint	retry;

	struct sockaddr_in	IPaddr;

	log_info("network_init");
	my_bind_addr=htonl(INADDR_ANY);


	if (my_port != 0 )
	{
		//log_info("general IP Socket is %d", my_port);

		ip_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (ip_sock == 0)
		{
			log_error("Error Socket Open\n");
			exit(1);
		}

		memset((char*) &IPaddr, 0, sizeof(IPaddr));
		IPaddr.sin_family = AF_INET;
		IPaddr.sin_addr.s_addr = my_bind_addr;
		IPaddr.sin_port = (unsigned short) htons((unsigned short) my_port);

		(void) setsockopt(ip_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&arg, sizeof(arg));
	

		if (bind(ip_sock, (struct sockaddr *) (&IPaddr),  sizeof(IPaddr)) < 0)
		{

			log_error("Can't start server: Bind on udp");
			exit(1);

		}
		//Setup the permmition of domain sock file
		//umask(666);
		if (listen(ip_sock, (int) back_log) < 0)
		{
			log_error("listen() on  UDP Failed with error %d", socket_error);
			exit(1);
		}

	}


}


int handle_connections_sockets(void *arg __attribute__((unused)))
{
	my_socket sock = 0, new_sock = 0;
	uint error_count = 0;
	uint max_used_connection =  (uint) (ip_sock + 1);
	fd_set readFDs, clientFDs;

	//THD *thd;

	struct sockaddr_in cAddr;
	int ip_flags = 0, flags = 0;
	//int socket_flags=0;
	//struct st_vio *vio_tmp;
	Vio *vio_tmp;
	uint retry = 0;
	THD *thd = NULL;
	log_info("handle_connections_sockets \n");

	FD_ZERO(&clientFDs);
	if (ip_sock != INVALID_SOCKET)
	{
		FD_SET(ip_sock, &clientFDs);
	}


	while (!abort_loop)
	{
		readFDs = clientFDs;
		if (select((int) max_used_connection, &readFDs, 0, 0, 0) < 0)
		{
			//if (socket_errno != SOCKET_EINTR)
			{
				//if (!select_errors++ && !abort_loop)	/* purecov: inspected */
				//printf("mysqld: Got error %d from select",socket_errno); /* purecov: inspected */
			}

			continue;
		}

		if (abort_loop)
		{
			break;
		}

		/* Is this a new connection request ? */
		{
			sock = ip_sock;
			flags = ip_flags;
		}

		for (retry = 0; retry < MAX_ACCEPT_RETRY; retry++)
		{
			uint length = sizeof(struct sockaddr_in);
			new_sock = accept(sock, (struct sockaddr *) (&cAddr), 	&length);
			

			if (new_sock != INVALID_SOCKET )//||  (socket_errno != SOCKET_EINTR && socket_errno != SOCKET_EAGAIN))
				break;

		}
		log_info("New Socket =%d", new_sock);

		if (new_sock == INVALID_SOCKET)
		{
			if ((error_count++ & 255) == 0)		// This can happen often
				log_error("Error in accept");

			//if (socket_errno == SOCKET_ENFILE || socket_errno == SOCKET_EMFILE)
			sleep(1);				// Give other threads some time
			continue;
		}



		uint dummyLen;
		struct sockaddr dummy;
		dummyLen = sizeof(struct sockaddr);
		if (getsockname(new_sock, &dummy, &dummyLen) < 0)
		{
			log_error("Error on new connection socket");
			shutdown(new_sock, SHUT_RDWR);
			(void) closesocket(new_sock);
			continue;
		}

		log_info("!!! Client Accept");


		/*
		** Don't allow too many connections
		*/

		//Allocate Memory THD
		thd = (THD *)malloc(sizeof(THD));

		if(thd == NULL)
		{
			log_error("[Error : Malloc Memory Fatal !!!");
			(void) closesocket(new_sock);
			continue;
		}
		memset(thd, 0, sizeof(THD));
		if (!(vio_tmp = vioNew(new_sock, VIO_TYPE_SOCKET, VIO_LOCALHOST)) || netInit(&thd->net, vio_tmp))
		{
			/*
			Only delete the temporary vio if we didn't already attach it to the
			stNET object. The destructor in THD will delete any initialized net
			structure.
			*/

			if (vio_tmp && thd->net.vio != vio_tmp)
				vioDelete(vio_tmp);
			else
			{
				shutdown(new_sock, SHUT_RDWR);
				(void) closesocket(new_sock);
			}
			free( thd);
			continue;
		}
		//if (sock == unix_sock)
		//  thd->security_ctx->host=(char*) my_localhost;

		create_new_thread(thd);
	}

	return 0;
}


static int init_thread_environment()
{
	(void) pthread_mutex_init(&LOCK_thread_count, NULL);
	(void) pthread_mutex_init(&LOCK_connection_count, NULL);

	(void) pthread_mutex_init(&LOCK_SetupNoti, NULL);

	/* Parameter for threads created for connections */
	(void) pthread_attr_init(&connection_attrib);
	
	//When a thread exits , this function returns its resources. Must Set . Or use the other function pthread_detach for it.
	(void) pthread_attr_setdetachstate(&connection_attrib, PTHREAD_CREATE_DETACHED);

	pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);

	pthread_attr_setstacksize(&connection_attrib, my_thread_stack_size);






	return 0;
}


static void create_new_thread(THD *thd)
{
	stNET *net = &thd->net;

	log_info("Create_new_thread Net FD = %d",net->fd);


	//if (protocol_version > 9)
	//  net->return_errno=1;

	/*
	 Don't allow too many connections. We roughly check here that we allow
		only (max_connections + 1) connections.
	*/

	pthread_mutex_lock(&LOCK_connection_count);

	if (connection_count >= max_connections + 1 || abort_loop)
	{
		pthread_mutex_unlock(&LOCK_connection_count);

		log_error("Too many connections");
		close_connection(thd, 0, 1);

		free( thd);
		return;
	}

	++connection_count;

	if (connection_count > max_used_connections)
		max_used_connections = connection_count;

	pthread_mutex_unlock(&LOCK_connection_count);

	/* Start a new thread to handle connection. */

	pthread_mutex_lock(&LOCK_thread_count);

	/*
		The initialization of thread_id is done in create_embedded_thd() for
		the embedded library.
		TODO: refactor this to avoid code duplication there
	*/
	thd->thread_id = thread_id++;

	thread_count++;

	thread_scheduler.add_connection(thd);


	
}



bool create_thread_to_handle_connection(THD *thd)
{

	//char error_message_buff[MYSQL_ERRMSG_SIZE];
	/* Create new thread to handle connection */
	int error;

	log_info("creating thread %lu", thd->thread_id);

	thd->prior_thr_create_utime = thd->start_utime = my_micro_time();
	if ((error = pthread_create(&thd->real_id, &connection_attrib, handle_one_connection, (void*) thd)))
	{
		/* purecov: begin inspected */
		log_error( "Can't create thread to handle request (error %d)", error);
		thread_count--;
		thd->killed = KILL_CON;			// Safety
		(void) pthread_mutex_unlock(&LOCK_thread_count);

		pthread_mutex_lock(&LOCK_connection_count);
		--connection_count;
		pthread_mutex_unlock(&LOCK_connection_count);

		(void) pthread_mutex_lock(&LOCK_thread_count);
		close_connection(thd,0,0);
		//delete thd;
		free( thd);
		(void) pthread_mutex_unlock(&LOCK_thread_count);
		return false;
		/* purecov: end */
	}

	(void) pthread_mutex_unlock(&LOCK_thread_count);
	log_info("Thread created");

	//size_t stack_size = 0;
	//pthread_attr_getstacksize(&connection_attrib, &stack_size);
	//log_info("Stack Size = %d", stack_size);
	
	return true;
}

void cleanup(THD *thd)
{
	log_info("THD::cleanup");

	vioDelete(thd->net.vio);
	netEnd(&thd->net);
	
}

/*
  Unlink thd from global list of available connections and free thd

  SYNOPSIS
    unlink_thd()
    thd		 Thread handler

  NOTES
    LOCK_thread_count is locked and left locked
*/

void unlink_thd(THD *thd)
{
	log_info("unlink_thd");


	cleanup(thd);

	pthread_mutex_lock(&LOCK_connection_count);
	--connection_count;
	pthread_mutex_unlock(&LOCK_connection_count);

	(void) pthread_mutex_lock(&LOCK_thread_count);
	/*
	  Used by binlog_reset_master.  It would be cleaner to use
	  DEBUG_SYNC here, but that's not possible because the THD's debug
	  sync feature has been shut down at this point.
	*/
	//DBUG_EXECUTE_IF("sleep_after_lock_thread_count_before_delete_thd", sleep(5););
	thread_count--;
	//delete thd;

	
	free( thd);
	//log_info("Delete : thread_count = %d", thread_count);

}


/*
  End thread for the current connection

  SYNOPSIS
    one_thread_per_connection_end()
    thd		  Thread handler
    put_in_cache  Store thread in cache, if there is room in it
                  Normally this is true in all cases except when we got
                  out of resources initializing the current thread

  NOTES
    If thread is cached, we will wait until thread is scheduled to be
    reused and then we will return.
    If thread is not cached, we end the thread.

  RETURN
    0    Signal to handle_one_connection to reuse connection
*/

bool one_thread_per_connection_end(THD *thd, bool put_in_cache)
{
	log_info("one_thread_per_connection_end");

	unlink_thd(thd);

	//Not use cache thread
	//if (put_in_cache)
	//  put_in_cache= cache_thread();

	pthread_mutex_unlock(&LOCK_thread_count);

	//if (put_in_cache)
	//  DBUG_RETURN(0);                             // Thread is reused

	/* It's safe to broadcast outside a lock (COND... is not deleted here) */
	log_info("signal", ("Broadcasting COND_thread_count"));
                                   // Must match DBUG_ENTER()
	//my_thread_end();
	//(void) pthread_cond_broadcast(&COND_thread_count);

	//In this function , it escapes. Not go to "return 0"
	pthread_exit(0);

	return 0;                                     // Avoid compiler warnings
}


/**
  Close a connection.

  @param thd		Thread handle
  @param errcode	Error code to print to console
  @param lock	        1 if we have have to lock LOCK_thread_count

  @note
    For the connection that is doing shutdown, this is called twice
*/
void close_connection(THD *thd, uint errcode, bool lock)
{
	Vio *vio;
	log_info("close_connection");
	
	if (lock)
		(void) pthread_mutex_lock(&LOCK_thread_count);
	thd->killed = KILL_CON;
	if ((vio = thd->net.vio) != 0)
	{
		//if (errcode)
		//  net_send_error(thd, errcode, ER(errcode)); /* purecov: inspected */
		vioClose(vio);			/* vio is freed in delete thd */
	}
	if (lock)
		(void) pthread_mutex_unlock(&LOCK_thread_count);
	return;
}




void StopNotification(unsigned int id)
{

	log_info("Stop Notification : %d",id);

	//must use this mutex . in case that the request from users is done very fast , if we don't use mutex and then we go to mal function.
	pthread_mutex_lock(&LOCK_SetupNoti);

	if ( mapNotifyMessageHandle.count(id) == 1)
	{
		mapNotifyMessageHandle[id]->CleanUp();
		delete mapNotifyMessageHandle[id];
		mapNotifyMessageHandle.erase(id);
	}

	
	pthread_mutex_unlock(&LOCK_SetupNoti);


}


int StartNotification( char* packet)
{
	int error=0;
	
	//must use this mutex . in case that the request from users is done very fast , we go to mal function.
	pthread_mutex_lock(&LOCK_SetupNoti);

	if(mapNotifyMessageHandle.size() > USER_USE_START_NUMBER)
	{
		log_error("!!! Max User Start\n");

		return CLIENT_REQUEST_RET_MAX_USER;
	}
	
	rapidjson::Document datastring;
	datastring.Parse(packet);

	int id =datastring["userIndex"].GetInt(); 


	log_info("StartNotification : %d",id);

	//Check if the map with the id exists.
	if ( mapNotifyMessageHandle.count(id) == 0)
	{
		try {

			mapNotifyMessageHandle[id] = new NotifyMessage(packet);
		}
		
		catch( ...) {
		
			 log_error("!!! new NotifyMessage Error id =%d\n",id);
			mapNotifyMessageHandle.erase(id); 
			error =CLIENT_REQUEST_RET_MAX_USER;
		}
	}
	else
		error =CLIENT_REQUEST_RET_ALREADY_STARTED;
	
	pthread_mutex_unlock(&LOCK_SetupNoti);

	return error;
}





