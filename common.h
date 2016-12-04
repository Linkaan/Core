/*
 *	timeout.h
 *	  Common macros and defs used in multiple source files
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

#ifndef _COMMON_H_
#define _COMMON_H_

/* Define _GNU_SOURCE for pthread_timedjoin_np and asprintf */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Temporary defs before config file is setup */
#define PICAM_STATE_DIR "/"

/* Used in record event to start or stop recording */
#define PICAM_START_RECORD ~0ULL
#define PICAM_STOP_RECORD 0

#define log_error (msg) \
		fprintf(stderr, "%s: %s: %d: %s: %s\n", __progname, \
				__FILE__, __LINE__, msg, strerror(errno))

/* String containing name the program is called with.
   To be initialized by main(). */
extern const char *__progname;

/* Common data structure used by threads */
typedef struct
  {
  	int 			pir_pin;
  	int 			timerfd;
  	int 			timerpipe[2];
  	int 			record_eventfd;
  	pthread_t 		timer_t;
  	pthread_t 		picam_t;
	pthread_attr_t	attr;
	pthread_t_mutex record_mutex;
  } thread_data;

#endif /* _COMMON_H_ */