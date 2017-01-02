/*
 *  network.h
 *    The names of functions callable from within network
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

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>

typedef void (*fg_event_cb)(int32_t, int32_t *);

struct fg_events_data {	
	struct event_base *base;
    pthread_t         *events_t;
    void 			  fg_event_cb cb;
};

/* Initialize libevent and add asynchronous event listener, register cb */
extern int fg_events_init (fg_event_cb);

/* Tear down event loop and cleanup */
extern void fg_events_shutdown (struct *fg_events_data);

#endif /* _NETWORK_H_ */