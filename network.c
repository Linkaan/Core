/*
 *  network.c
 *    Handles events over TCP using libevent2 and fagelmatare-serializer
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "network.h"
#include "log.h"

/* These values should be initalized from a config file */
#define PORT 1337

static void
fg_read_cb (struct bufferevent *bev, void *arg)
{
	size_t len;
  	unsigned char *buffer, *ptr;
	struct fg_events_data *itdata;
	struct evbuffer *input;

	itdata = (struct fg_events_data *) arg;
	input = bufferevent_get_input(bev);

  	len = evbuffer_get_length (input);
  	buffer = malloc (len);
  	if (!buffer) {
  		log_error_en ("malloc failed");
  		return;
  	}
  	evbuffer_copyout (input, data, len);

  	while (ptr - buffer < len) {
  		struct fgevent fgev;

  		ptr = deserialize_fgevent (ptr, &fgev);

  		/* Check if event was successfully parsed, if not we must increment
  		   ptr by payload length to prevent any events next to be incorrectly
  		   parsed */
  		if (fgev.length > 0 && !fgev.payload)
  		  {
  		  	log_error_en ("failed to allocate memory for event");
  		  	ptr += fgev.length;
  		  }
  		ítdata->cb (fgev.length, fgev.payload);
  	}

  	free (buffer);
	/* TODO: Deserialize event, handle event and write back if necessary */
}

static void
fg_event_cb (struct bufferevent *bev, short events, void *arg)
{
	if (events & BEV_EVENT_ERROR)
		log_error ("error in bufferevent");
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
		bufferevent_free (bev);
}

static void
accept_conn_cb (struct evconnlistener *listener, evutil_socket_t fd,
	struct sockaddr *address, int socklen, void *arg)
{
	struct event_base *base;
	struct bufferevent *bev;

	base = evconnlistener_get_base (listener);
	bev = bufferevent_socket_new (base, fd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb (bev, echo_read_cb, NULL, echo_event_cb, arg);	
	bufferevent_enable (bev, EV_READ | EV_WRITE);
}

static void
accept_error_cb (struct evconnlistener *listener, void *arg)
{
	int err;
	struct event_base *base;

	base = evconnlistener_get_base (listener);
	err = EVUTIL_SOCKET_ERROR ();
	fprintf (stderr, "Error when listening on events (%d): %s\n", err,
		evutil_socket_error_to_string (err));
	event_base_loopexit (base, NULL);
}

static void *
events_thread_start (void *param)
{
	struct fg_events_data *itdata;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	itdata = (struct fg_events_data *) param;
	itdata->base = event_base_new ();
	if (itdata->base == NULL)
	  {
	  	fprintf (stderr, "Could not create event base\n");
	  	return NULL;
	  }

	/* TODO: also listen on UNIX socket */
	memset (&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons (PORT);

	listener = evconnlistener_new_bind (base, &accept_conn_cb, param,
										LEV_OPT_CLOSE_ON_FREE |
										LEV_OPT_REUSABLE, -1,
										(struct sockaddr *) &sin,
										sizeof (sin));
	if (listener == NULL)
	  {
	  	log_error ("Could not create INET listener");
	  	return NULL;
	  }
	evconnlistener_set_error_cb (listener, &accept_error_cb);

	event_base_dispatch (itdata->base);

	return NULL;
}

int
fg_events_init (struct thread_data *tdata, fg_event_cb cb)
{
	ssize_t s;

	memset (&tdata->etdata, 0, sizeof (tdata->etdata));
	tdata->etdata->cb = cb;

	s = pthread_create (&tdata->events_t, &tdata->attr, &events_thread_start,
                        &tdata->etdata);
    if (s != 0) {
        log_error_en (s, "error creating events thread");
        return 1;
    }
    tdata->etdata->events_t = &tdata->events_t;

    return 0;
}

void
fg_events_shutdown (struct *fg_events_data itdata)
{
	event_base_loopexit (itdata->base);
	pthread_join (*itdata->events_t);
}