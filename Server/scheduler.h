/* Copyright (C) 2007 MySQL AB

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

/*
  Classes for the thread scheduler
*/
#ifndef _scheduler_h
#define _scheduler_h

#include "my_global.h"
#include "my_pthread.h"



/* Functions used when manipulating threads */
struct  st_scheduler_functions
{

  uint max_threads;
  bool (*init)(void);
  bool (*init_new_connection_thread)(void);
  bool (*add_connection)(THD *thd);
  bool (*post_kill_notification)(void);
  bool (*end_thread)(THD *thd, bool cache_thread);
  void (*end)(void);

};

typedef struct  st_scheduler_functions scheduler_functions;

extern scheduler_functions thread_scheduler;


enum scheduler_types
{
  SCHEDULER_ONE_THREAD_PER_CONNECTION=0,
  SCHEDULER_NO_THREADS,
  SCHEDULER_POOL_OF_THREADS
};

void one_thread_per_connection_scheduler(scheduler_functions* func);


#endif

