/*
 *	motion.c
 *	  Interrupt handler for PIR sensor
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

#include <wiringPi/wiringPi.h>

#include "common.h"

/* Forward declarations used in this file. */
static int reset_timer (int, int, int);

/* Callback function for interrupts on pin numbered as PIR_PIN (see core.c) */
void
on_motion_detect (void *arg)
{
	int s;
	thread_data *tdata = arg;

	if (digitalRead (tdata->pir_pin) == HIGH)
	  {
		reset_timer (tdata->timerfd, 5, 0);
		if (atomic_compare_exchange_weak (&tdata->is_recording, (_Bool[]) { false }, true))
		  {
            pthread_mutex_lock (&tdata->record_mutex);
            pthread_cond_signal (&tdata->record_cond);
            pthread_mutex_unlock (&tdata->record_mutex);
		  }
		else
		  {
		  	if (atomic_compare_exchange_weak (&tdata->is_recording, (_Bool[]) { true }, true))
      			reset_timer (tdata->timerfd, 5, 0);
		  }
	  }
}

/* Helper function to set timerfd to a specified timer value */
static int
reset_timer(int timerfd, int secs, int isecs)
{
  int s;
  struct itimerspec timer_value;

  /*
   * Set timer_value to a delay of secs seconds, if isecs is set to a
   * value > 0 this timer will be recurring
   */
  memset (&timer_value, 0, sizeof(timer_value));
  timer_value.it_value.tv_sec = secs;
  timer_value.it_value.tv_nsec = 0;
  timer_value.it_interval.tv_sec = isecs;
  timer_value.it_interval.tv_nsec = 0;

  s = timerfd_settime (timerfd, 0, &timer_value, NULL);
  if (s != 0)
    log_error("timerfd_settime failed");    

  return s;
}