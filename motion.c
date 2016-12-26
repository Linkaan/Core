/*
 *  motion.c
 *    Interrupt handler for PIR sensor
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

#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <wiringPi/wiringPi.h>

#include "common.h"
#include "log.h"

/* Forward declarations used in this file. */
static int reset_timer (struct thread_data *, const int, const int);

/* Callback function for interrupts on pin numbered as PIR_PIN (see core.c) */
void
on_motion_detect (void *arg)
{
    int b;
    ssize_t s;
    uint64_t u;
    struct thread_data *tdata = arg;

    pthread_mutex_lock (&tdata->wiring_mutex);
    b = digitalRead (tdata->pir_pin) == HIGH;
    pthread_mutex_unlock (&tdata->wiring_mutex);

    _log_debug ("isr %s\n", b ? "rising" : "falling");
    if (b || atomic_compare_exchange_weak (&tdata->fake_isr, (_Bool[])
             { true }, false))
      {
        reset_timer (tdata, 5, 0);
        if (atomic_compare_exchange_weak (&tdata->is_recording, (_Bool[])
            { false }, true))
          {
            /* Send start recording event */
            pthread_mutex_lock (&tdata->record_mutex);        
            u = 1;
            s = write (tdata->record_eventfd, &u, sizeof (uint64_t));
            if (s < 0)
              {
                log_error ("write failed");
                atomic_store (&tdata->is_recording, false);
              }
            pthread_mutex_unlock (&tdata->record_mutex);
          }
      }
    else if (atomic_load (&tdata->is_recording))
        reset_timer (tdata, 5, 0);
}

/* Function to reset timer if PIR sensor is still HIGH */
int
check_sensor_active (struct thread_data *tdata)
{
    int b;

    pthread_mutex_lock (&tdata->wiring_mutex);
    b = digitalRead (tdata->pir_pin) == HIGH;
    pthread_mutex_unlock (&tdata->wiring_mutex);
    if (b != 0)
        reset_timer (tdata, 5, 0);

    return b;
}

/* Helper function to set timerfd to a specified timer value */
static int
reset_timer(struct thread_data *tdata, const int secs, const int isecs)
{
  ssize_t s;
  struct itimerspec timer_value;

  _log_debug ("resetting timer (is_recording = %s)\n",
              atomic_load (&tdata->is_recording) ? "true" : "false");

  /*
   * Set timer_value to a delay of secs seconds, if isecs is set to a
   * value > 0 this timer will be recurring
   */
  memset (&timer_value, 0, sizeof(timer_value));
  timer_value.it_value.tv_sec = secs;
  timer_value.it_value.tv_nsec = 0;
  timer_value.it_interval.tv_sec = isecs;
  timer_value.it_interval.tv_nsec = 0;
  
  s = timerfd_settime (tdata->timerfd, 0, &timer_value, NULL);
  if (s < 0)
    log_error ("timerfd_settime failed");    

  return s;
}