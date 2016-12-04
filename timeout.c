/*
 *	timeout.c
 *	  Asynchronous function to handle timeout event set by motion handler
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

#include <pthread.h>
#include <stdint.h>

#include <wiringPi/wiringPi.h>

#include "timeout.h"
#include "common.h"

/* Forward declarations used in this file. */
static void cleanup_handler (void *);

static void
cleanup_handler(void *arg)
{
	internal_t_data *itdata = arg;

	pthread_mutex_unlock (itdata->record_mutex);
}

void *
thread_timeout_start(void *arg)
{
	int s, events;
	thread_data *tdata = arg;
	internal_t_data itdata;

	itdata.record_mutex = tdata->record_mutex;
	pthread_cleanup_push(&cleanup_handler, &itdata);

	memset(&itdata.poll_fds, 0, sizeof(itdata.poll_fds));

	itdata.poll_fds[0].fd = tdata->timerfd;
	itdata.poll_fds[0].revents = 0;
	itdata.poll_fds[0].events = events = POLLIN | POLLPRI;

	itdata.poll_fds[1] = itdata.poll_fds[0];
	itdata.poll_fds[1].fd = tdata->timerpipe[0];

	while (1)
	  {
	  	/* Passing -1 to poll as third argument means to block (INFTIM) */
	  	s = poll(&itdata.poll_fds, 2, -1);
	  	if (s < 0)
	  		log_error("poll failed");
	  	else
	  	  {
	  	  	/* If there is data to read on poll_fds, we shall exit */
	  	  	if (itdata.poll_fds[1].revents & events)
	  	  	  {
	  	  	  	break;
	  	  	  }
	  	  }
	  }

	/* Put the thread in deferred cancellation mode to avoid the scenario
	   where the thread gets cancelled in the middle of the execution of 
	   our cleanup handler */
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* Call our cleanup handler */
	pthread_cleanup_pop(1);

	return NULL;
}