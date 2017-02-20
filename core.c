/*
 *  fagelmatare-core
 *    Communicates with picam when a motion event is risen and logs statistics
 *  core.c
 *    Initialize wiring pi and create threads for each asynchrounous function 
 *****************************************************************************
 *  This file is part of Fågelmataren, an embedded project created to learn
 *  Linux and C. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2017 Linus Styrén
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
#include <stdatomic.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <wiringPi/wiringPi.h>

#include "picam_state.h"
#include "motion.h"
#include "timeout.h"
#include "network.h"
#include "common.h"
#include "log.h"

/* The PIR sensor is wired to the physical pin 31 (wiringPi pin 21) */
#define PIR_PIN 21

/* Forward declarations used in this file. */
static void do_cleanup (struct thread_data *tdata);

static int setup_thread_attr (struct thread_data *);
static int create_timer_thread (struct thread_data *);
static int setup_wiringPi (struct thread_data *);

static void join_or_cancel_thread (pthread_t, struct timespec *);

/* Non-zero means we should exit the program as soon as possible */
static sem_t keep_going;

/* Used for faking interrupts */
static volatile int raise_fake_isr = 0;

/* Signal handler for SIGTSTP, SIGINT, SIGHUP and SIGTERM */
static void
handle_sig (int signum)
{
    struct sigaction new_action;

    if (signum == SIGTSTP)
        raise_fake_isr = 1;
    sem_post (&keep_going);

    new_action.sa_handler = handle_sig;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = SA_RESTART;

    sigaction (signum, &new_action, NULL);
}

/* Setup termination signals to exit gracefully */
static void
handle_signals ()
{
    struct sigaction new_action, old_action;

    /* Turn off buffering on stdout to directly write to log file */
    setvbuf (stdout, NULL, _IONBF, 0);

    /* Set up the structure to specify the new action. */
    new_action.sa_handler = handle_sig;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = SA_RESTART;
    
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
    sigaction (SIGTSTP, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN)
        sigaction (SIGTSTP, &new_action, NULL); 
}

/* Helper function to attempt joining a thread, if a timeout runs out it shall
   cancel the thread */
static void
join_or_cancel_thread (pthread_t t, struct timespec *ts)
{
    ssize_t s;

    s = pthread_timedjoin_np (t, NULL, ts);
    if (s != 0)
      {
        s = pthread_cancel (t);
        if (s != 0)
            log_error_en (s, "error in pthread_cancel");
      }
}

/* Helper function to initialize thread attributes and condition variables */
static int
setup_thread_attr (struct thread_data *tdata)
{
    ssize_t s;

    s = pthread_mutex_init (&tdata->record_mutex, NULL);
    if (s != 0)
      {
        log_error_en (s, "error in pthread_mutex_init");
        do_cleanup (tdata);
        return s;   
      }

    s = eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (s < 0)
      {
        log_error ("error in eventfd");
        do_cleanup (tdata);
        return s;
      }
    tdata->record_eventfd = s;

    /* Initialize thread creation attributes */
    s = pthread_attr_init (&tdata->attr);
    if (s != 0)
      {
        log_error_en (s, "error in pthread_attr_init");
        do_cleanup (tdata);
        return s;
      }

    /* Explicitly create threads as joinable, only possible error is EINVAL
       if the second parameter (detachstate) is invalid */
    s = pthread_attr_setdetachstate (&tdata->attr, PTHREAD_CREATE_JOINABLE);
    if (s != 0)
      {
        log_error_en (s, "error in pthread_attr_setdetachstate");
        do_cleanup (tdata);     
      }

    return s;
}

/* Helper function to create timer thread and start listening on timerfd */
static int
create_timer_thread (struct thread_data *tdata)
{
    struct sched_param param;
    ssize_t s;

    tdata->is_recording = ATOMIC_VAR_INIT(false);
    s = pthread_create (&tdata->timer_t, &tdata->attr, &thread_timeout_start,
                        tdata);
    if (s != 0)
      {
        log_error_en (s, "error creating timeout thread");
        do_cleanup (tdata);
      }

    memset (&param, 0, sizeof (struct sched_param));
    param.sched_priority = 1;

    s = pthread_setschedparam (tdata->timer_t, SCHED_FIFO, &param);
    if (s != 0)
      {
        log_error_en (s, "error in pthread_setschedparam");
        do_cleanup (tdata);
      }
    return s;
}

static int
create_picam_thread (struct thread_data *tdata)
{
    ssize_t s;

    s = pthread_create (&tdata->picam_t, &tdata->attr, &thread_picam_start,
                        tdata);
    if (s != 0)
      {
        log_error_en (s, "error creating picam thread");
        do_cleanup (tdata);
      }
    return s;   
}

/* Helper function to setup wiringPi and register an interrupt handler */
static int
setup_wiringPi (struct thread_data *tdata)
{
    ssize_t s;

    s = pthread_mutex_init (&tdata->wiring_mutex, NULL);
    if (s != 0)
      {
        log_error_en (s, "error in pthread_mutex_init");
        do_cleanup (tdata);
        return s;   
      }

    /* Initialize wiringPi with default pin numbering scheme */
    s = wiringPiSetup ();
    if (s < 0)
      {
        log_error ("error in wiringPiSetup");
        do_cleanup (tdata);
        return s;
      }

    /* Register a interrupt handler on the pin numbered as PIR_PIN */
    tdata->pir_pin = PIR_PIN;
    s = wiringPiISR (tdata->pir_pin, INT_EDGE_BOTH, &on_motion_detect, tdata);
    if (s < 0)
      {
        log_error ("error in wiringPiISR");
        do_cleanup (tdata);
      }

    return s;
}

int
main (void)
{
    ssize_t s;
    uint64_t u;
    struct timespec ts;
    struct thread_data tdata;

    /* Initialize keep_going as binary semaphore initially 0 */
    sem_init (&keep_going, 0, 0);

    memset (&tdata, 0, sizeof (tdata));

    handle_signals ();

    s = setup_wiringPi (&tdata);
    if (s < 0)
      {
        return 1;
      }

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
    if (s < 0)
      {
        log_error ("error creating pipe");
        do_cleanup (&tdata);
        return 1;
      }

    s = setup_thread_attr (&tdata);
    if (s != 0)
      {
        return 1;
      }

    s = create_timer_thread (&tdata);
    if (s != 0)
      {
        return 1;
      }

    s = create_picam_thread (&tdata);
    if (s != 0)
      {
        return 1;
      }

    s = fg_events_server_init (&tdata.etdata, &fg_handle_event, &tdata, PORT,
                               UNIX_SOCKET_PATH);
    if (s != 0)
      {
        log_error ("error initializing fgevents");
        do_cleanup (&tdata);
        return 1;
      }

    while (1)
      {
        sem_wait (&keep_going);
        if (raise_fake_isr)
          {
            atomic_store (&tdata.fake_isr, true);
            on_motion_detect ((void *) &tdata);
            raise_fake_isr = 0;         
            continue;       
          }
        break;
      } 

    /* **************************************************************** */
    /*                                                                  */
    /*                      Begin shutdown sequence                     */
    /*                                                                  */
    /* **************************************************************** */
    sem_destroy (&keep_going);  

    /* Write 8 bytes to notify all threads to exit */
    u = ~0ULL;
    s = write (tdata.timerpipe[1], &u, sizeof (uint64_t));
    if (s < 0)
      {
        log_error ("error in write");
      }

    /* Fetch current time and put it in ts struct, we use the |= operator
       so that if previous call to read failed we will cancel threads */
    s |= clock_gettime (CLOCK_REALTIME, &ts);
    if (s < 0)
      {
        log_error ("error in clock_gettime");
        s = pthread_cancel (tdata.timer_t);
        if (s != 0)
            log_error ("error in pthread_cancel");
        s = pthread_cancel (tdata.picam_t);
        if (s != 0)
            log_error ("error in pthread_cancel");
      }         
    else
      {
        ts.tv_sec += 5;
        join_or_cancel_thread (tdata.timer_t, &ts);
        join_or_cancel_thread (tdata.picam_t, &ts);
      }

    fg_events_server_shutdown (&tdata.etdata);

    s = pthread_mutex_destroy (&tdata.record_mutex);
    if (s != 0)
        log_error_en (s, "error in pthread_mutex_destroy");

    s = pthread_mutex_destroy (&tdata.wiring_mutex);
    if (s != 0)
        log_error_en (s, "error in pthread_mutex_destroy");

    s = close (tdata.record_eventfd);
    if (s < 0)
        log_error ("error in close");

    s = close (tdata.timerpipe[1]);
    if (s < 0)
        log_error ("error in close");

    s = close (tdata.timerpipe[0]);
    if (s < 0)
        log_error ("error in close");

    s = pthread_attr_destroy (&tdata.attr);
    if (s != 0)
        log_error_en (s, "error in pthread_attr_destroy");

    return 0;
}

static void
do_cleanup (struct thread_data *tdata)
{

}
