/*
 *  common.h
 *    Common macros and defs used in multiple source files
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <errno.h>

/* Define _GNU_SOURCE for pthread_timedjoin_np and asprintf */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Define _DEBUG to enable debug messages */
#define _DEBUG

#define TIMESTAMP_MAX_LENGTH 32

/* Temporary defs before config file is setup */
#define PICAM_STATE_DIR "/mnt/mmcblk0p2/picam/state"
#define PICAM_STOP_HOOK "/mnt/mmcblk0p2/picam/hooks/stop_record"
#define PICAM_START_HOOK "/mnt/mmcblk0p2/picam/hooks/start_record"
#define UNIX_SOCKET_PATH "/tmp/fg.socket"
#define PORT 1337

/* String containing name the program is called with.
   To be initialized by main(). */
extern const char *__progname;

/* Common data structure used by threads */
struct thread_data {
    int                   pir_pin;    
    int                   timerfd;
    int                   timerpipe[2];
    int                   record_eventfd;    
    atomic_bool           fake_isr;
    atomic_bool           is_recording;
    pthread_t             timer_t;
    pthread_t             picam_t;
    pthread_t             events_t;
    pthread_attr_t        attr;
    pthread_mutex_t       record_mutex;
    pthread_mutex_t       wiring_mutex;
    struct fg_events_data etdata;
};

#endif /* _COMMON_H_ */