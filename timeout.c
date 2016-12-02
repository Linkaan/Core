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
	thread_data *tdata = arg;
	internal_t_data itdata;

	itdata.record_mutex = tdata->record_mutex;
	pthread_cleanup_push(&cleanup_handler, &itdata);

	/* Put the thread in deferred cancellation mode to avoid the scenario
	   where the thread gets cancelled in the middle of the execution of 
	   our cleanup handler */
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* Call our cleanup handler */
	pthread_cleanup_pop(1);

	return NULL;
}