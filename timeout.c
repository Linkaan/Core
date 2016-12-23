/*
 *  timeout.c
 *    Asynchronous function to handle timeout event set by motion handler
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
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <wiringPi/wiringPi.h>

#include "motion.h"
#include "timeout.h"
#include "common.h"

/* Forward declarations used in this file. */
static void cleanup_handler (void *);

/* Used internally by thread to store allocated resources  */
struct internal_t_data {
    int             poll_fds_len;
    pthread_mutex_t record_mutex;
    struct pollfd   poll_fds[2];
};

/* Start routine for timer thread */
void *
thread_timeout_start (void *arg)
{
    ssize_t s, events;
    uint64_t u;
    struct thread_data *tdata = arg;
    struct internal_t_data itdata;

    itdata.record_mutex = tdata->record_mutex;

    /* Put the thread in deferred cancellation mode to avoid the scenario
       where the thread gets cancelled in the middle of the execution of 
       our cleanup handler */
    s = pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);
    if (s != 0)
      {
        log_error_en (s, "pthread_setcanceltype failed");
      }

    memset (&itdata, 0, sizeof (itdata));
    memset (&itdata.poll_fds, 0, sizeof (itdata.poll_fds));

    itdata.poll_fds[itdata.poll_fds_len].fd = tdata->timerfd;
    itdata.poll_fds[itdata.poll_fds_len].revents = 0;
    itdata.poll_fds[itdata.poll_fds_len++].events = events = POLLIN | POLLPRI;

    itdata.poll_fds[itdata.poll_fds_len] = itdata.poll_fds[0];
    itdata.poll_fds[itdata.poll_fds_len++].fd = tdata->timerpipe[0];

    while (1)
      {
        /* Passing -1 to poll as third argument means to block (INFTIM) */
        s = poll (itdata.poll_fds, 2, -1);

        if (s < 0)
            log_error("poll failed");
        else if (s > 0)
          {
            if (!check_sensor_active (tdata) &&
                atomic_load (&tdata->is_recording))
              {
                pthread_cleanup_push (&cleanup_handler, &itdata);
                pthread_mutex_lock (&itdata.record_mutex);

                /* Instead of using a pthread condition variable we use a
                   eventfd object to notify other threads because we can then
                   poll on multiple file descriptors */
                u = 2;
                s = write (tdata->record_eventfd, &u, sizeof (uint64_t));
                if (s < 0)
                    log_error ("write failed");

                pthread_cleanup_pop (1);
              }
            else
              {
                _log_debug ("not stopping recording (is_recording = %s)\n",
                            atomic_load (itdata->is_recording) ? "true" :
                                                                 "false");
              }

            /* If there is data to read on timerpipe, we shall exit */
            if (itdata.poll_fds[1].revents & events)
              {
                break;
              }

            if (itdata.poll_fds[0].revents & events)
              {
                s = read (itdata.poll_fds[0].fd, &u, sizeof (uint64_t));
                if (s < 0)
                    log_error ("read failed");
              }            
          }
      }

    return NULL;
}

/* This function is used to cleanup thread */
static void
cleanup_handler(void *arg)
{
    struct internal_t_data *itdata = arg;

    pthread_mutex_unlock (&itdata->record_mutex);
}