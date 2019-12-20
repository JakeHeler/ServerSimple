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

/* Defines to make different thread packages compatible */

#ifndef _MY_PTHREAD_HEADER
#define _MY_PTHREAD_HEADER

#include "my_global.h"
#include "net_comm.h"



#ifndef ETIME
#define ETIME ETIMEDOUT				/* For FreeBSD */
#endif

#ifdef  __cplusplus
#define EXTERNC extern "C"
extern "C" {
#else
#define EXTERNC
#endif /* __cplusplus */


/* READ-WRITE thread locking */



#define GETHOSTBYADDR_BUFF_SIZE 2048

#ifndef HAVE_THR_SETCONCURRENCY
#define thr_setconcurrency(A) pthread_dummy(0)
#endif


//#define pthread_attr_setstacksize(A,B) pthread_attr_setstacksize(A,B)//pthread_dummy(0)


/* Define mutex types, see my_thr_init.c */
#define MY_MUTEX_INIT_SLOW   NULL
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t my_fast_mutexattr;
#define MY_MUTEX_INIT_FAST &my_fast_mutexattr
#else
#define MY_MUTEX_INIT_FAST   NULL
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t my_errorcheck_mutexattr;
#define MY_MUTEX_INIT_ERRCHK &my_errorcheck_mutexattr
#else
#define MY_MUTEX_INIT_ERRCHK   NULL
#endif

#ifndef ESRCH
/* Define it to something */
#define ESRCH 1
#endif

typedef ulong my_thread_id;



/* All thread specific variables are in the following struct */

#define THREAD_NAME_SIZE 10
#ifndef DEFAULT_THREAD_STACK
#if SIZEOF_CHARP > 4
/*
  MySQL can survive with 32K, but some glibc libraries require > 128K stack
  To resolve hostnames. Also recursive stored procedures needs stack.
*/
#define DEFAULT_THREAD_STACK	(256*1024L)
#else
#define DEFAULT_THREAD_STACK	(192*1024)
#endif
#endif

struct st_my_thread_var
{
	int thr_errno;
	pthread_cond_t suspend;
	pthread_mutex_t mutex;
	pthread_mutex_t * volatile current_mutex;
	pthread_cond_t * volatile current_cond;
	pthread_t pthread_self;
	my_thread_id id;
	int cmp_length;
	int volatile abort;
	my_bool init;
	struct st_my_thread_var *next, **prev;
	void *opt_info;
#ifndef DBUG_OFF
	void *dbug;
	char name[THREAD_NAME_SIZE + 1];
#endif
};

extern struct st_my_thread_var *_my_thread_var(void) __attribute__ ((const));
extern uint my_thread_end_wait_time;
#define my_thread_var (_my_thread_var())
#define my_errno my_thread_var->thr_errno
/*
  Keep track of shutdown,signal, and main threads so that my_end() will not
  report errors with them
*/

/* Which kind of thread library is in use */

#define THD_LIB_OTHER 1
#define THD_LIB_NPTL  2
#define THD_LIB_LT    4

extern uint thd_lib_detected;

/*
  thread_safe_xxx functions are for critical statistic or counters.
  The implementation is guaranteed to be thread safe, on all platforms.
  Note that the calling code should *not* assume the counter is protected
  by the mutex given, as the implementation of these helpers may change
  to use atomic operations instead.
*/

/*
  Warning:
  When compiling without threads, this file is not included.
  See the *other* declarations of thread_safe_xxx in include/my_global.h

  Second warning:
  See include/config-win.h, for yet another implementation.
*/
#ifdef THREAD
#ifndef thread_safe_increment
#define thread_safe_increment(V,L) \
        (pthread_mutex_lock((L)), (V)++, pthread_mutex_unlock((L)))
#define thread_safe_decrement(V,L) \
        (pthread_mutex_lock((L)), (V)--, pthread_mutex_unlock((L)))
#endif

#ifndef thread_safe_add
#define thread_safe_add(V,C,L) \
        (pthread_mutex_lock((L)), (V)+=(C), pthread_mutex_unlock((L)))
#define thread_safe_sub(V,C,L) \
        (pthread_mutex_lock((L)), (V)-=(C), pthread_mutex_unlock((L)))
#endif
#endif



/*
  No locking needed, the counter is owned by the thread
*/
#define status_var_increment(V) (V)++
#define status_var_decrement(V) (V)--
#define status_var_add(V,C)     (V)+=(C)
#define status_var_sub(V,C)     (V)-=(C)


#define pthread_handler_t EXTERNC void *
typedef void *(* pthread_handler)(void *);


enum killed_state
{
	NOT_KILLED = 0,
	KILL_BAD_DATA = 1,
	KILL_CON,
	//KILL_QUERY,
	KILLED_NO_VALUE    /* means neither of the states */
};


///////////////////////////////////////////////////////////////
extern pthread_mutex_t	LOCK_connection_count, LOCK_thread_count;
extern uint volatile thread_count, thread_running, global_read_lock;
///////////////////////////////////////////////////////////////


typedef struct st_thd
{

	stNET	  net;				// client connection descriptor
	pthread_mutex_t LOCK_thd_data;

	uint user_id;

	/**
	  - Protects thd->mysys_var (used during KILL statement and shutdown).
	  - Is Locked when THD is deleted.

	  Note: This responsibility was earlier handled by LOCK_thd_data.
	  This lock is introduced to solve a deadlock issue waiting for
	  LOCK_thd_data. As this lock reduces responsibility of LOCK_thd_data
	  the deadlock issues is solved.
	  Caution: LOCK_thd_kill should not be taken while holding LOCK_thd_data.
	           THD::awake() currently takes LOCK_thd_data after holding
	           LOCK_thd_kill.
	*/
	pthread_mutex_t LOCK_thd_kill;

	/*
	  A pointer to the stack frame of handle_one_connection(),
	  which is called first in the thread for handling a client
	*/
	char	  *thread_stack;

	/**
	  Currently selected catalog.
	*/
	char *catalog;

	const char *proc_info;

	/*
	  Used in error messages to tell user in what part of MySQL we found an
	  error. E. g. when where= "having clause", if fix_fields() fails, user
	  will know that the error was in having clause.
	*/
	const char *where;

	double tmp_double_value;                    /* Used in set_var.cc */
	unsigned long client_capabilities;		/* What the client supports */
	unsigned long max_client_packet_length;

	//HASH		handler_tables_hash;
	enum client_command command;


	unsigned int     server_id;
	unsigned int     file_id;			// for LOAD DATA INFILE
	/* remote (peer) port */
	unsigned short peer_port;
	time_t     start_time, user_time;
	// track down slow pthread_create
	ulonglong  prior_thr_create_utime, thr_create_utime;
	ulonglong  start_utime, utime_after_lock;

	pthread_t  real_id;                           /* For debugging */
	my_thread_id  thread_id;
	uint	     tmp_table, global_read_lock;
	int	     server_status;



	enum	killed_state volatile killed;

	bool       slave_thread, one_shot_set;
	/* tells if current statement should binlog row-based(1) or stmt-based(0) */
	bool       current_stmt_binlog_row_based;
	bool	     locked, some_tables_deleted;
	bool       last_cuted_field;
	bool	     no_errors, password;
	/**
	  Set to TRUE if execution of the current compound statement
	  can not continue. In particular, disables activation of
	  CONTINUE or EXIT handlers of stored routines.
	  Reset in the end of processing of the current user request, in
	  @see mysql_reset_thd_for_next_command().
	*/
	bool is_fatal_error;

	/**  is set if some thread specific value(s) used in a statement. */
	//bool       thread_specific_used;
	bool       enable_slow_log;   /* enable slow log for current statement */
	//bool	     abort_on_warning;
	//bool 	     got_warning;       /* Set on call to push_warning() */
	//bool	     no_warnings_for_error; /* no warnings on call to my_error() */

} THD;


#ifdef  __cplusplus
}
#endif

#endif /*_MY_PTHREAD_HEADER */

