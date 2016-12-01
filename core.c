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

#include <signal.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <wiringPi/wiringPi.h>

#include "common.h"

#define PIR_PIN 21

/* String containing name the program is called with.
   To be initialized by main(). */
extern const char *__progname;

/* Forward declarations used in this file. */

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
	int timerfd;
	int timerpipe[2];
	pthread_t timer_t;
	struct thread_data tdata;
	struct sigaction new_action, old_action;

	/* Initialize keep_going as binary semaphore initially 0 */
	sem_init(&keep_going, 0, 0);

	handle_signals();	

	/* Initialize timer used for timeout on video recording */
	timerfd = timerfd_create(CLOCK_REALTIME, 0);
	if (timerfd < 0)
	  {
	  	log_error("error in timerfd_create");
		exit(1);
	  }
	tdata.timerfd = timerfd;

	/* Initialize thread creation attributes */
	s = pthread_attr_init(&attr);
	if (s != 0)
	  {
		log_error("error in pthread_attr_init");
		exit(1);
	  }

	s = pthread_create(&timer_t, &attr, &thread_timeout_start, &tdata);
	if (s != 0)
	  {
		log_error("error creating timeout thread");
		exit(1);
	  }

	wiringPiSetup();

	if (wiringPiISR(PIR_PIN, INT_EDGE_BOTH, &motion_callback, &tdata) < 0)
	  {
		log_error("error in wiringPiISR");
		exit(1);
	  }

	if (pipe (timerpipe))
	  {
	    log_error("error creating pipe");
	   	exit(1);
	  }

	sem_wait(&keep_going);
	sem_destroy(&keep_going);

	return 0;
}