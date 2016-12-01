/*
 *  fagelmatare-core
 *    Communicates with picam when a motion event is risen and logs statistics
 *	core.c
 *	  Initialize wiring pi and create threads for each asynchrounous function 
 *****************************************************************************
 *  This file is part of Fågelmataren, an embedded project created to learn
 *  Linux and C. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2016 Linus Styrén
 *
 *  Fågelmataren is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the Licence, or
 *  (at your option) any later version.
 *
 *  Fågelmataren is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

#define _GNU_SOURCE
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <wiringPi/wiringPi.h>

#include "timeout.h"
#include "common.h"

/* The PIR sensor is wired to the physical pin 31 (wiringPi pin 21) */
#define PIR_PIN 21

/* Helper macro to attempt joining a thread, if a timeout runs out it shall
   cancel the thread */
#define join_or_cancel_thread(t) \
		s = pthread_timedjoin_np(t, NULL, &ts); \
   		if (s != 0) \
   		  { \
   		    s = pthread_cancel(t); \
   		    if (s != 0) \
   		    	log_error ("error in pthread_cancel"); \
   		  }

/* Forward declarations used in this file. */
static void do_cleanup (void);

/* Non-zero means we should exit the program as soon as possible */
static sem_t keep_going;

void
handle_sigint (int signum)
{
	sem_post(&keep_going);
}

void
handle_signals ()
{
	struct sigaction new_action, old_action;

	/* Set up the structure to specify the new action. */
	new_action.sa_handler = handle_sigint;
	sigemptyset (&new_action.sa_mask);
	new_action.sa_flags = 0;
	
	/* Handle termination signals but avoid handling signals previously set
	   to be ignored */
	sigaction (SIGINT, NULL, &old_action);
	if (old_action.sa_handler != SIG_IGN)
		sigaction (SIGINT, &new_action, NULL);
	sigaction (SIGHUP, NULL, &old_action);
	if (old_action.sa_handler != SIG_IGN)
		sigaction (SIGHUP, &new_action, NULL);
	sigaction (SIGTERM, NULL, &old_action);
	if (old_action.sa_handler != SIG_IGN)
		sigaction (SIGTERM, &new_action, NULL);
}

int
main (void)
{
	int s;
	struct timespec ts;
	struct thread_data tdata;

	/* Initialize keep_going as binary semaphore initially 0 */
	sem_init (&keep_going, 0, 0);

	handle_signals ();	

	/* Initialize timer used for timeout on video recording */
	tdata.timerfd = timerfd_create (CLOCK_REALTIME, 0);
	if (tdata.timerfd  < 0)
	  {
	  	log_error ("error in timerfd_create");
	  	do_cleanup (&tdata);
		return 1;
	  }

	/* Create a pipe used to singal all threads to begin shutdown sequence */
	s = pipe (tdata.timerpipe);
	if (s != 0)
	  {
	    log_error ("error creating pipe");
	    do_cleanup (&tdata);
	   	return 1;
	  }

	/* Initialize thread creation attributes */
	s = pthread_attr_init (&tdata.attr);
	if (s != 0)
	  {
		log_error ("error in pthread_attr_init");
		do_cleanup (&tdata);
		return 1;
	  }

	/* Explicitly create threads as joinable, only possible error is EINVAL
	   if the second parameter (detachstate) is invalid */
	s = pthread_attr_setdetachstate (&tdata.attr, PTHREAD_CREATE_JOINABLE);
	if (s != 0)
	  {
		log_error ("error in pthread_attr_setdetachstate");
		do_cleanup (&tdata);
		return 1;
	  }

	s = pthread_create (&tdata.timer_t, &tdata.attr, &thread_timeout_start, &tdata);
	if (s != 0)
	  {
		log_error ("error creating timeout thread");
		do_cleanup (&tdata);
		return 1;
	  }

	/* Initialize wiringPi with default pin numbering scheme */
	s = wiringPiSetup ();
	if (s != 0)
	  {
		log_error ("error in wiringPiSetup");
		do_cleanup (&tdata);
		return 1;
	  }

	/* Register a interrupt handler on the pin numbered as PIR_PIN */
	s = wiringPiISR (PIR_PIN, INT_EDGE_BOTH, &motion_callback, &tdata);
	if (s != 0)
	  {
		log_error ("error in wiringPiISR");
		do_cleanup (&tdata);
		return 1;
	  }

	sem_wait (&keep_going);

	/* **************************************************************** */
	/*								    								*/
	/*						Begin shutdown sequence		    			*/
	/*								    								*/
	/* **************************************************************** */

	sem_destroy (&keep_going);

	s = write (tdata.timerpipe[1], NULL, 8);
	if (s != 8)
	  {
	    log_error ("error in write");
	    do_cleanup (&tdata);
		return 1;
	  }

	/* Fetch current time and put it in ts struct */
	s = clock_gettime (CLOCK_REALTIME, &ts);
    if (s != 0)
      {
		log_error ("error in clock_gettime");
	    s = pthread_cancel (timer_t);
	    if (s != 0)
	    	log_error ("error in pthread_cancel");
      }  		
   	else
   	  {
   		ts.tv_sec += 5;
   		join_or_cancel_thread (timer_t);
   	  }

   	s = close (tdata.timerpipe[1]);
   	if (s != 0)
   		log_error ("error in close");   

   	s = close (tdata.timerpipe[0]);
   	if (s != 0)
   		log_error ("error in close");

   	s = pthread_attr_destroy (&tdata.attr);
	if (s != 0)
		log_error ("error in pthread_attr_destroy");

	return 0;
}