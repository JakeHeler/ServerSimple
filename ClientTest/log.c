
/* Copyright (c) 2003-2007 MySQL AB

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

//#include "log.h"

//#include "my_global.h"
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>


#if  (DEBUG_OPNTION ==0)
int debug_opntion=0;
char *desOption="\nNo Debug OutPuts , But Log of Errors saved in the File.\n";

#elif  (DEBUG_OPNTION ==1)
int debug_opntion=1;
char *desOption="\nAll outputs go to stdout\n";

#elif  (DEBUG_OPNTION ==2)
int debug_opntion=2;
char *desOption="\nEach outputs go to  ists File.\n";

#elif  (DEBUG_OPNTION ==3)
int debug_opntion=3;
char *desOption="\nNo Debug OutPuts\n";


#else
int debug_opntion=0;
char *desOption="\nNo Debug OutPut , But Log of Errors saved in the File.\n";

#endif

#define ERR_OPEN "%s: can't open debug output stream \"%s\": "
#define ERR_CLOSE "%s: can't close debug file: "
#define ERR_ABORT "%s: debugger aborting because %s\n"
#define ERR_CHOWN "%s: can't change owner/group of \"%s\": "


#define	LOG_FILE_ERRRO		"/tmp/log_server_error"
#define	LOG_FILE_OUTPUT	"/tmp/log_output"



static void log(FILE *file, const char *level_tag, const char *format, va_list args);

typedef struct st_output_dev
{
	FILE	*ptr;
	char	id;
} debug_output_device;

debug_output_device logout;
debug_output_device error_log;


static void DBUGOpenFile( debug_output_device *filePtr,  const char *name,  int append)
{

	char newfile;
	register FILE *fp=0;


	if (name != NULL)
	{

		newfile = !access(name,F_OK);
		umask(0);

		if (!(filePtr->ptr = fopen(name, append ? "a+" : "w")))
		{
			(void) fprintf(stderr, ERR_OPEN, "Debug File Open", name);
			perror("");
			fflush(stderr);
			
		}
		else
		{
			
			if (newfile)
			{
				if (chown(name, getuid(), getgid()) == -1)
				{
					(void) fprintf(stderr, ERR_CHOWN, "Debug File Open", name);
					perror("");
					(void) fflush(stderr);
				}

			}
				
		}

	}

	

}


char *strmake(register char *dst, register const char *src, size_t length)
{
//#ifdef EXTRA_DEBUG
	/*
	  'length' is the maximum length of the string; the buffer needs
	  to be one character larger to accomodate the terminating '\0'.
	  This is easy to get wrong, so we make sure we write to the
	  entire length of the buffer to identify incorrect buffer-sizes.
	  We only initialise the "unused" part of the buffer here, a) for
	  efficiency, and b) because dst==src is allowed, so initialising
	  the entire buffer would overwrite the source-string. Also, we
	  write a character rather than '\0' as this makes spotting these
	  problems in the results easier.
	*/
	uint n = 0;
	while (n < length && src[n++]);
	memset(dst + n, (int) 'Z', length - n + 1);
//#endif

	while (length--)
		if (! (*dst++ = *src++))
			return dst - 1;
	*dst = 0;
	return dst;
}


/*
  TODO:
  - add flexible header support
  - rewrite all fprintf with fwrite
  - think about using 'write' instead of fwrite/fprintf on POSIX systems
*/

/*
  Format log entry and write it to the given stream.
  SYNOPSIS
    log()
*/
struct tm *localtime_r(const time_t *clock, struct tm *res)
{
	struct tm *tmp;

	tmp = localtime(clock);
	*res = *tmp;

	return res;
}

static void log(FILE *file, const char *level_tag, const char *format, va_list args)
{
	/*
	  log() should be thread-safe; it implies that we either call fprintf()
	  once per log(), or use flockfile()/funlockfile(). But flockfile() is
	  POSIX, not ANSI C, so we try to vsnprintf the whole message to the
	  stack, and if stack buffer is not enough, to malloced string. When
	  message is formatted, it is fprintf()'ed to the file.
	*/
	if( file == NULL)
		return;
	
	/* Format time like MYSQL_LOG does. */
	time_t now = time(0);
	struct tm bd_time;                            // broken-down time
	localtime_r(&now, &bd_time);

	char buff_date[128];
	sprintf(buff_date, "[%02d/%02d/%02d %02d:%02d:%02d] [%s] ",
	        (int) bd_time.tm_year % 100,
	        (int) bd_time.tm_mon + 1,
	        (int) bd_time.tm_mday,
	        (int) bd_time.tm_hour,
	        (int) bd_time.tm_min,
	        (int) bd_time.tm_sec,
	        (const char *) level_tag);
	/* Format the message */
	char buff_stack[256];

	int n = vsnprintf(buff_stack, sizeof(buff_stack), format, args);
	/*
	  return value of vsnprintf can vary, according to various standards;
	  try to check all cases.
	*/
	char *buff_msg = buff_stack;
	if (n < 0 || n == sizeof(buff_stack))
	{
		int size = sizeof(buff_stack) * 2;
		buff_msg = (char*) malloc(size);
		while (1)
		{
			if (buff_msg == 0)
			{
				strmake(buff_stack, "log(): message is too big, my_malloc() failed",
				        sizeof(buff_stack) - 1);
				buff_msg = buff_stack;
				break;
			}
			n = vsnprintf(buff_msg, size, format, args);
			if (n >= 0 && n < size)
				break;
			size *= 2;
			/* realloc() does unnecessary memcpy */
			free(buff_msg);
			buff_msg = (char*) malloc(size);
		}
	}
	else if ((size_t) n > sizeof(buff_stack))
	{
		buff_msg = (char*) malloc(n + 1);
#ifdef DBUG
		DBUG_ASSERT(n == vsnprintf(buff_msg, n + 1, format, args));
#else
		vsnprintf(buff_msg, n + 1, format, args);
#endif
	}
	fprintf(file, "%s%s\n", buff_date, buff_msg);
	if (buff_msg != buff_stack)
		free(buff_msg);



	/* don't fflush() the file: buffering strategy is set in log_init() */
}

/**************************************************************************
  Logging: implementation of public interface.
**************************************************************************/

/*
  The function initializes logging sub-system.

  SYNOPSIS
    log_init()
*/
debug_output_device pointtest;
void log_init()
{

	/*
	  stderr is unbuffered by default; there is no good of line buffering,
	  as all logging is performed linewise - so remove buffering from stdout
	  also
	*/

	switch(debug_opntion)
	{
		case 0:
		{
			printf("Debug Option : %s",desOption);

			logout.ptr=NULL;
			
			DBUGOpenFile(&error_log,LOG_FILE_ERRRO,1);
			if(error_log.ptr == NULL)
			{	fprintf(stderr, "Error :Log of Error Open File \n");
				fflush(stderr);
			}
			
		}
		break;
		case 1:
		{
			printf("Debug Option : %s",desOption);
			logout.ptr=stdout;
			error_log.ptr =stdout;
			

		}
		break;
		case 2:
		{
			printf("Debug Option : %s",desOption);
			
			DBUGOpenFile(&error_log,LOG_FILE_ERRRO,1);
			if(error_log.ptr ==NULL)
			{	fprintf(stderr, "Error :Log of Error Open File\n");
				fflush(stderr);
			}

			DBUGOpenFile(&logout,LOG_FILE_OUTPUT,1);
			if(logout.ptr ==NULL)
			{	fprintf(stderr, "Error :Log Open File\n");
				fflush(stderr);
			}

		}
		break;

		case 3:
		{
			printf("Debug Option : %s",desOption);

			logout.ptr=NULL;
			error_log.ptr =NULL;

			
		}
		break;		
	}
	setbuf(stdout, 0);

	

}


/*
  The function is intended to log error messages. It precedes a message
  with date, time and [ERROR] tag and print it to the stderr and stdout.

  We want to print it on stdout to be able to know in which context we got the
  error

  SYNOPSIS
    log_error()
    format      [IN] format string
    ...         [IN] arguments to format
*/

void log_error(const char *format, ...)
{
	if(error_log.ptr ==NULL)
		return;


	va_list args;
	va_start(args, format);
	log(error_log.ptr, "ERROR", format, args);
	fflush(error_log.ptr);
	
	//log(stderr, "ERROR", format, args);
	//fflush(stderr);
	va_end(args);
}


/*
  The function is intended to log information messages. It precedes
  a message with date, time and [INFO] tag and print it to the stdout.

  SYNOPSIS
    log_error()
    format      [IN] format string
    ...         [IN] arguments to format
*/

void log_info(const char *format, ...)
{
	if(logout.ptr ==NULL)
		return;
	
	va_list args;
	va_start(args, format);
	log(logout.ptr, "INFO", format, args);
	va_end(args);
}

/*
  The function prints information to the error log and eixt(1).

  SYNOPSIS
    die()
    format      [IN] format string
    ...         [IN] arguments to format
*/

void die(const char *format, ...)
{
	va_list args;
	fprintf(stderr, "%s: ", "jake");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

