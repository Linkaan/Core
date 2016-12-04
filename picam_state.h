/*
 *	picam.h
 *	  The names of functions callable from within picam_state
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

#ifndef _PICAM_H_
#define _PICAM_H_

#include <stdint.h>
#include <stdbool.h>

/* Used internally by thread to store allocated resources  */
typedef struct
  {  	
  	_Bool watch_state_enabled;
  	char *path;
  	char *content;
  	int fp;
  	int inotify_fd;  
  	uint32_t inotify_mask;
  	struct stat st;
  	struct pollfd poll_fds[3];
  } internal_t_data;

/* This function is invoked by core as the timer thread is created */
extern void *thread_picam_start (void *arg);

#endif /* _PICAM_H_ */