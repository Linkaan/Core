/*
 *	picam.c
 *	  Create start/stop hooks for picam and watch for state changes
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <dirent.h>

#include "picam.h"
#include "common.h"

/* Forward declarations used in this file. */
static void cleanup_handler (void *);

static int watch_state_enabled = 1;

/* Start routine for picam thread */
void *
thread_picam_start (thread_data *tdata)
{
	int s;
	thread_data *tdata = arg;
	internal_t_data itdata;

	pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push (&cleanup_handler, &itdata);

	s = inotify_init();
	if (s < 0)
	  {
	  	log_error("inotify_init failed");
		pthread_exit();
	  }

	/* Call our cleanup handler */
	pthread_cleanup_pop (1);

	return NULL;
}