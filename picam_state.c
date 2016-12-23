/*
 *  picam.c
 *    Create start/stop hooks for picam and watch for state changes
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

/* Read for inotify fd requires a buffer, we approximate the size */
#define BUF_LEN (10 * (sizeof (struct inotify_event) + 32 + 1))

/* Used internally by thread to store allocated resources  */
struct internal_t_data {    
    _Bool       watch_state_enabled;
    char        *dir;
    char        *path;
    char        *content;
    int         wd;
    FILE        *fp;
    atomic_bool *is_recording;
    int         inotify_fd;
    size_t      dir_strlen;
    int         poll_fds_len;
    char        *picam_start_hook;
    char        *picam_stop_hook;
    uint32_t    inotify_mask;
    struct      timespec start;
    struct      timespec end;
    struct      stat st;
    struct      pollfd poll_fds[3];
};

/* Forward declarations used in this file. */
static void cleanup_handler (void *);

static void handle_state_file_created (struct internal_t_data *);
static void handle_state_file (struct internal_t_data *, const char *,
                               const char *);
static void handle_record_event (struct internal_t_data *, const uint64_t);

static int setup_inotify (struct internal_t_data *);

/* Start routine for picam thread */
void *
thread_picam_start (void *arg)
{
    ssize_t s, events;
    uint64_t u;
    struct thread_data *tdata = arg;
    struct internal_t_data itdata;

    pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);
    pthread_cleanup_push (&cleanup_handler, &itdata);

    memset (&itdata, 0, sizeof (itdata));

    /* TODO: add these values to a config file */
    asprintf (&itdata.dir, PICAM_STATE_DIR);
    asprintf (&itdata.picam_start_hook, PICAM_START_HOOK);
    asprintf (&itdata.picam_stop_hook, PICAM_STOP_HOOK);
    itdata.dir_strlen = strlen(itdata.dir);

    itdata.is_recording = &tdata->is_recording;
    s = setup_inotify (&itdata);
    itdata.watch_state_enabled = (_Bool) s >= 0;

    memset (&itdata.poll_fds, 0, sizeof (itdata.poll_fds));

    itdata.poll_fds[itdata.poll_fds_len].fd = itdata.inotify_fd;
    itdata.poll_fds[itdata.poll_fds_len].revents = 0;
    itdata.poll_fds[itdata.poll_fds_len++].events = events = POLLIN | POLLPRI;

    itdata.poll_fds[itdata.poll_fds_len] = itdata.poll_fds[0];
    itdata.poll_fds[itdata.poll_fds_len++].fd = tdata->timerpipe[0];

    itdata.poll_fds[itdata.poll_fds_len] = itdata.poll_fds[0];
    itdata.poll_fds[itdata.poll_fds_len++].fd = tdata->record_eventfd;

    while (1)
      {
        /* Passing -1 to poll as third argument means to block (INFTIM) */
        s = poll (itdata.poll_fds, 3, -1);

        if (s < 0)
            log_error ("poll failed");
        else if (s > 0)
          {
            if (itdata.poll_fds[1].revents & events)
              {
                break;
              }
            else if (itdata.poll_fds[0].revents & events)
              {
                handle_state_file_created (&itdata);                
              }
            else /* tdata->timerpipe[0] or tdata->record_eventfd */
              {
                if (itdata.poll_fds[2].revents & events)
                  {
                    pthread_mutex_lock (&tdata->record_mutex);
                    s = read (itdata.poll_fds[2].fd, &u, sizeof (uint64_t));
                    if (s < 0)
                        log_error ("read failed");
                    pthread_mutex_unlock (&tdata->record_mutex);
                    handle_record_event (&itdata, u);
                  }                
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
    ssize_t s, nbytes;
    char buf[BUF_LEN] __attribute__ ((aligned(8)));
    
    nbytes = read (itdata->inotify_fd, buf, BUF_LEN);
    if (nbytes < 0)
        log_error ("read failed");
    else if (nbytes == 0)
      {
        log_error ("read from inotify fd returned 0");
        return;
      }
    for (char *p = buf; p < buf + nbytes;)
      {
        struct inotify_event *event = (struct inotify_event *) p;
        if (event->len && event->mask & itdata->inotify_mask &&
            !(event->mask & IN_ISDIR)) /* Ignore all directories */
          {
            /* We add 2 bytes for the delimiter and nul-terminator */
            int path_len = itdata->dir_strlen + strlen (event->name) + 2;
            itdata->path = malloc (path_len);
            if (itdata->path == NULL)
              log_error ("malloc for file path failed");
            else
              {
                s = snprintf (itdata->path, path_len, "%s/%s", itdata->dir,
                              event->name);

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
                handle_state_file (itdata, event->name, itdata->content);
                free (itdata->content);
                itdata->content = NULL;
              }
            s = unlink (itdata->path);
            if (s < 0)
                log_error ("unlink failed");
            free (itdata->path);
            itdata->path = NULL;
          }
        p += sizeof (struct inotify_event) + event->len;
      }
}

/* Helper function for when a new state file is created */
static void
handle_state_file (struct internal_t_data *itdata, const char *filename,
                   const char *content)
{
    ssize_t s;

    if (strcmp (filename, "record") == 0 && content != NULL)
      {
        if (strcmp (content, "false") == 0)
          {
            if (atomic_compare_exchange_weak (itdata->is_recording, (_Bool[])
                { true }, false))
              {
                s = clock_gettime (CLOCK_REALTIME, &itdata->end);
                double elapsed = (itdata->end.tv_sec - itdata->start.tv_sec) *
                                  1E9 + itdata->end.tv_nsec -
                                  itdata->start.tv_nsec;
                printf ("recorded video of length %lf seconds\n",
                        elapsed / 1E9);               
              }
          }
        else if (strcmp (content, "true") == 0)
          {
            printf ("started recording (is recording: %s)\n",
                    atomic_load (itdata->is_recording) ? "true" : "false");
            s = clock_gettime (CLOCK_REALTIME, &itdata->start);
          }
        else
            printf ("record state changed to %s\n", content);
      }
}

/* Helper function to create picam hooks on record event */
static void
handle_record_event (struct internal_t_data *itdata, const uint64_t u)
{
    ssize_t s;  

    switch (u % 2)
      {
        case 1: // start recording if u is not divisible by 2
            s = touch (itdata->picam_start_hook);
            break;
        case 0: // stop recording otherwise
            s = touch (itdata->picam_stop_hook);
            if (s == 0 && !itdata->watch_state_enabled)
              {
                atomic_store (itdata->is_recording, false);
              }
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
    ssize_t s;  

    s = inotify_init ();
    if (s < 0)
      {
        log_error ("inotify_init failed");
        return 1;
      }
    itdata->inotify_fd = (int) s;

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
    itdata->wd = inotify_add_watch (itdata->inotify_fd, itdata->dir,
                                    itdata->inotify_mask);

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