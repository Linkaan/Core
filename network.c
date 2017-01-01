#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
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