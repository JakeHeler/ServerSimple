/* Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* get time since epoc in 100 nanosec units */
/* thus to get the current time we should use the system function
   with the highest possible resolution */

/* 
   TODO: in functions my_micro_time() and my_micro_time_and_time() there
   exists some common code that should be merged into a function.
*/

#include "my_global.h"



ulonglong my_getsystime()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (ulonglong)tv.tv_sec*10000000+(ulonglong)tv.tv_usec*10;

}


/*
  Return current time

  SYNOPSIS
    my_time()
    flags	If MY_WME is set, write error if time call fails

*/

time_t my_time(myf flags __attribute__((unused)))
{
  time_t t;

  /* The following loop is here beacuse time() may fail on some systems */
  while ((t= time(0)) == (time_t) -1)
  {
    //if (flags & MY_WME)
    //  fprintf(stderr, "%s: Warning: time() call failed\n", my_progname);
  }
  return t;

}


/*
  Return time in micro seconds

  SYNOPSIS
    my_micro_time()

  NOTES
    This function is to be used to measure performance in micro seconds.
    As it's not defined whats the start time for the clock, this function
    us only useful to measure time between two moments.

    For windows platforms we need the frequency value of the CUP. This is
    initalized in my_init.c through QueryPerformanceFrequency().

    If Windows platform doesn't support QueryPerformanceFrequency() we will
    obtain the time via GetClockCount, which only supports milliseconds.

  RETURN
    Value in microseconds from some undefined point in time
*/

ulonglong my_micro_time()
{

  ulonglong newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  newtime= (ulonglong)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;

}


/*
  Return time in seconds and timer in microseconds (not different start!)

  SYNOPSIS
    my_micro_time_and_time()
    time_arg		Will be set to seconds since epoch (00:00:00 UTC,
                        January 1, 1970)

  NOTES
    This function is to be useful when we need both the time and microtime.
    For example in MySQL this is used to get the query time start of a query
    and to measure the time of a query (for the slow query log)

  IMPLEMENTATION
    Value of time is as in time() call.
    Value of microtime is same as my_micro_time(), which may be totally
    unrealated to time()

  RETURN
    Value in microseconds from some undefined point in time
*/

#define DELTA_FOR_SECONDS LL(500000000)  /* Half a second */

ulonglong my_micro_time_and_time(time_t *time_arg)
{

  ulonglong newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  *time_arg= t.tv_sec;
  newtime= (ulonglong)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;

}


/*
  Returns current time

  SYNOPSIS
    my_time_possible_from_micro()
    microtime		Value from very recent my_micro_time()

  NOTES
    This function returns the current time. The microtime argument is only used
    if my_micro_time() uses a function that can safely be converted to the
    current time.

  RETURN
    current time
*/

time_t my_time_possible_from_micro(ulonglong microtime __attribute__((unused)))
{

  return (time_t) (microtime / 1000000);

}


