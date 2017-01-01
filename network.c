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

#include "log.h"

static void
fg_read_cb (struct bufferevent *bev, void *ctx)
{
	/* TODO: Handle event */
}

static void
fg_event_cb (struct bufferevent *bev, short events, void *ctx)
{
	if (events & BEV_EVENT_ERROR)
		log_error ("error in bufferevent");
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
		bufferevent_free (bev);
}

static void
accept_conn_cb (struct evconnlistener *listener, evutil_socket_t fd,
	struct sockaddr *address, int socklen, void *ctx)
{
	struct event_base *base;
	struct bufferevent *bev;

	base = evconnlistener_get_base (listener);
	bev = bufferevent_socket_new (base, fd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb (bev, echo_read_cb, NULL, echo_event_cb, NULL);	
	bufferevent_enable (bev, EV_READ | EV_WRITE);
}

static void
accept_error_cb (struct evconnlistener *listener, void *ctx)
{
	int err;
	struct event_base *base;

	base = evconnlistener_get_base (listener);
	err = EVUTIL_SOCKET_ERROR ();
	fprintf (stderr, "Error when listening on events (%d): %s\n", err,
		evutil_socket_error_to_string (err));
	event_base_loopexit (base, NULL);
}

struct fg_events_data *
fg_events_init (void (*callback)(char *))
{
	ssize_t s;
	struct sockaddr_in sin;
	struct fg_events_data itdata;

	memset (&itdata, 0, sizeof (itdata));
}