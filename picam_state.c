/*
 *	picam.c
 *	  Create start/stop hooks for picam and watch for state changes
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/inotify.h>
#include <poll.h>
#include <pthread.h>
#include <dirent.h>

#include "picam_state.h"
#include "touch.h"
#include "common.h"

/* Used internally by thread to store allocated resources  */
static struct internal_t_data {  	
	_Bool 		watch_state_enabled;
	char		*dir;
	char 		*path;
	char 		*content;
	int 		wd;
	FILE 		*fp;
	atomic_bool	*is_recording;
	int 		inotify_fd;
	size_t 		dir_strlen;
	char 		*picam_start_hook;
	char 		*picam_stop_hook;
	uint32_t 	inotify_mask;
	struct 		timespec start;
	struct 		timespec end;
	struct 		stat st;
	struct 		pollfd poll_fds[3];
};

/* Forward declarations used in this file. */
static void cleanup_handler (void *);

static void handle_state_file_created (struct internal_t_data *);
static void handle_state_file (struct internal_t_data *, const char *, const char *);
static void handle_record_event (struct internal_t_data *, const uint64_t);

static int setup_inotify (struct internal_t_data *);

/* Start routine for picam thread */
void *
thread_picam_start (void *arg)
{
	int s, events;
	uint64_t u;
	struct thread_data *tdata = arg;
	struct internal_t_data itdata;

	pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push (&cleanup_handler, &itdata);

	/* TODO: add these values to a config file */
	asprintf (&itdata.dir, PICAM_STATE_DIR);
	asprintf (&itdata.picam_start_hook, PICAM_START_HOOK);
	asprintf (&itdata.picam_stop_hook, PICAM_STOP_HOOK);
	itdata.dir_strlen = strlen(itdata.dir);

	itdata.is_recording = &tdata->is_recording;
	s = setup_inotify (&itdata);
	if (s < 0)
		itdata.watch_state_enabled = false;

	memset (&itdata.poll_fds, 0, sizeof (itdata.poll_fds));

	itdata.poll_fds[0].fd = itdata.inotify_fd;
	itdata.poll_fds[0].revents = 0;
	itdata.poll_fds[0].events = events = POLLIN | POLLPRI;

	itdata.poll_fds[1] = itdata.poll_fds[0];
	itdata.poll_fds[1].fd = tdata->timerpipe[0];

	itdata.poll_fds[2] = itdata.poll_fds[0];
	itdata.poll_fds[2].fd = tdata->record_eventfd;

	while (1)
	  {
	  	/* Passing -1 to poll as third argument means to block (INFTIM) */
	  	s = poll (itdata.poll_fds, 3, -1);

	  	if (s < 0)
	  		log_error ("poll failed");
	  	else if (s > 0)
	  	  {
            if (itdata.poll_fds[0].revents & events)
              {
              	handle_state_file_created (&itdata);              	
              }
            else /* tdata->timerpipe[0] or tdata->record_eventfd */
              {
                s = read (itdata.poll_fds[0].fd, &u, sizeof (uint64_t));
                if (s != sizeof (uint64_t))
                    log_error ("read failed");

                if (itdata.poll_fds[1].revents & events)
              		break;

                handle_record_event (&itdata, u);
              }
          }
      }

	/* Call our cleanup handler */
	pthread_cleanup_pop (1);

	return NULL;
}

/* When there is a inotify event to be read */
static void
handle_state_file_created (struct internal_t_data *itdata)
{
	int s;

	struct inotify_event event;
	s = read (itdata->inotify_fd, &event, sizeof (struct inotify_event));
    if (s != sizeof (struct inotify_event))
        log_error ("read failed");
    if (event.len)
      {
      	if (event.mask & itdata->inotify_mask)
      	  {
      	  	/* Ignore all directories */
      	  	if (!(event.mask & IN_ISDIR))
      	  	  {
      	  	  	/* We add 2 bytes for the delimiter and nul-terminator */
      	  	  	int path_len = itdata->dir_strlen + strlen (event.name) + 2;
      	  	  	itdata->path = malloc (path_len);
      	  	  	if (itdata->path == NULL)
      	  	  	  log_error ("malloc for file path failed");
      	  	  	else
      	  	  	  {
      	  	  	  	s = snprintf (itdata->path, path_len, "%s/%s", itdata->dir, event.name);

      	  	  	  	itdata->fp = fopen (itdata->path, "rb");
      	  	  	  	if (itdata->fp)
      	  	  	  	  {
      	  	  	  	  	fseek (itdata->fp, 0, SEEK_END);
      	  	  	  	  	long content_len = ftell (itdata->fp);
      	  	  	  	  	fseek (itdata->fp, 0, SEEK_SET);

      	  	  	  	  	itdata->content = malloc (content_len + 1);                  	  	  	  	  	
      	  	  	  	  	if (itdata->content)
      	  	  	  	  	  {
      	  	  	  	  	  	fread (itdata->content, 1, content_len, itdata->fp);      	  	  	  	  	  	
      	  	  	  	  	  	itdata->content[content_len] = '\0';
      	  	  	  	  	  }
      	  	  	  	  	else
      	  	  	  	  		log_error ("malloc for file content failed");
      	  	  	  	  	fclose (itdata->fp);
      	  	  	  	  	itdata->fp = NULL;
      	  	  	  	  }
      	  	  	  	else
      	  	  	  		log_error ("fopen failed");
      	  	  	  	handle_state_file (itdata, event.name, itdata->content);
      	  	  	  	free (itdata->content);
      	  	  	  	itdata->content = NULL;
      	  	  	  }
      	  	  	s = unlink (itdata->path);
      	  	  	if (s < 0)
      	  	  		log_error ("unlink failed");
      	  	  	free (itdata->path);
      	  	  	itdata->path = NULL;
      	  	  }
      	  }
      }
}

/* Helper function for when a new state file is created */
static void
handle_state_file (struct internal_t_data *itdata, const char *filename, const char *content)
{
	int s;

	if (strcmp(filename, "record") == 0)
	  {
	  	if (content != NULL)
	  	  {
	  	  	if (strcmp(content, "false") == 0)
	  	  	  {
				if (atomic_compare_exchange_weak (itdata->is_recording, (_Bool[]) { true }, false))
				  {
			        s = clock_gettime (CLOCK_REALTIME, &itdata->end);
			        if (s < 0)
			        	printf ("recorded video\n");
			       	else
			       	  {
						double elapsed = (itdata->end.tv_sec - itdata->start.tv_sec) * 1E9 + itdata->end.tv_nsec - itdata->start.tv_nsec;
			        	printf ("recorded video of length %lf seconds\n", elapsed / 1E9);
			       	  }			        
				  }
	  	  	  }
	  	  	else if (strcmp (content, "true") == 0)
	  	  	  {
	  	  	  	printf ("started recording\n");
	  	  	  	s = clock_gettime (CLOCK_REALTIME, &itdata->start);
	  	  	  }
	  	  	else
	  	  		printf ("record state changed to %s\n", content);
	  	  }
	  }
}

/* Helper function to create picam hooks on record event */
static void
handle_record_event (struct internal_t_data *itdata, const uint64_t u)
{
	int s;

	switch (u)
      {
      	case PICAM_START_RECORD:
      		s = touch (itdata->picam_start_hook);
      		break;
      	case PICAM_STOP_RECORD:
      		s = touch (itdata->picam_stop_hook);
      		if (s == 0 && !itdata->watch_state_enabled)
      			atomic_store (itdata->is_recording, false);
      		break;
      	default:
      		errno = EINVAL;
      		s = -1;
      		break;
      }

    if (s < 0)
    	log_error ("could not handle record event");
}

/* Helper function to initialize inotify event */
static int
setup_inotify (struct internal_t_data *itdata)
{
	int s;	

	s = inotify_init ();
	if (s < 0)
	  {
	  	log_error ("inotify_init failed");
		return 1;
	  }
	itdata->inotify_fd = s;

	s = stat (itdata->dir, &itdata->st);
	if (s < 0)
	  {
	  	if (errno == ENOENT)
	  	  {
	  	  	log_error ("is picam running? state dir not available");
	  	  	return 1;
	  	  }
	  	else
	  	  {
	  	  	log_error ("stat failed");
	  	  	return 1;
	  	  }
	  }
	else
      {
        if (!S_ISDIR (itdata->st.st_mode))
          {
      	  	log_error ("state path is not a directory");
      	  	return 1;
		  }
      }

    if (access (itdata->dir, R_OK) < 0)
      {
      	log_error ("access failed");
      	return 1;
      }

    itdata->inotify_mask = IN_CLOSE_WRITE;
    itdata->wd = inotify_add_watch (itdata->inotify_fd, itdata->dir, itdata->inotify_mask);

	return 0;
}

/* This function is used to cleanup thread */
static void
cleanup_handler(void *arg)
{
    struct internal_t_data *itdata = arg;

    inotify_rm_watch (itdata->inotify_fd, itdata->wd);
    close (itdata->inotify_fd);

    free (itdata->dir);
    free (itdata->picam_start_hook);
    free (itdata->picam_stop_hook);

    if (itdata->path)
    	free (itdata->path);
    if (itdata->content)
    	free (itdata->content);

    if (itdata->fp)
    	fclose (itdata->fp);
}