/*
 *  touch.c
 *    Modified from http://git.savannah.gnu.org/cgit/coreutils.git/tree/src/touch.c
 *  touch -- change modification and access times of files
 *  Copyright (C) 1987-2016 Free Software Foundation, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Written by Paul Rubin, Arnold Robbins, Jim Kingdon, David MacKenzie,
 *  and Randy Smith.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "touch.h"
#include "common.h"
#include "log.h"

/* Forward declarations used in this file. */
int fdutimensat(int, int, char const *, struct timespec const ts[2], int);

/*
 * Create file or update the time of file according to the options given.
 * Returns a negative number on fail
 */
int
touch (const char *file)
{
  int fd = -1;
  int open_errno = 0;
  struct stat info;
  struct timespec new_times[2];

  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  fd = open (file, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY, mode);

  /*
   * Don't save a copy of errno if it's EISDIR, since that would lead
   * touch to give a bogus diagnostic for e.g., `touch /' (assuming
   * we don't own / or have write access to it).  On Solaris 5.6,
   * and probably other systems, it is EINVAL.  On SunOS4, it's EPERM.
   */
  if (fd == -1 && errno != EISDIR && errno != EINVAL && errno != EPERM)
    open_errno = errno;

  if (stat (file, &info) < 0)
    {
    if (fd < 0)
      {
        log_error ("stat failed");
        if (open_errno)
          {
            errno = open_errno;
          }
        close (fd);
        return 1;
      }
    }
  else
    {
      /* keep atime unchanged */
      new_times[0] = info.st_atim;

      /* set mtime to current time */
      clock_gettime (CLOCK_REALTIME, &new_times[1]);
    }

  if (fdutimensat (fd, AT_FDCWD, file, new_times, 0))
    {
      if (open_errno)
        {
          errno = open_errno;
        }
      close (fd);
      return 1;
    }

  close (fd);
  return 0;
}

/* Set the access and modification time stamps of FD (a.k.a. FILE) to be
   TIMESPEC[0] and TIMESPEC[1], respectively; relative to directory DIR.
   FD must be either negative -- in which case it is ignored --
   or a file descriptor that is open on FILE.
   If FD is nonnegative, then FILE can be NULL, which means
   use just futimes (or equivalent) instead of utimes (or equivalent),
   and fail if on an old system without futimes (or equivalent).
   If TIMESPEC is null, set the time stamps to the current time.
   ATFLAG is passed to utimensat if FD is negative or futimens was
   unsupported, which can allow operation on FILE as a symlink.
   Return 0 on success, -1 (setting errno) on failure. */
int
fdutimensat (int fd, int dir, char const *file, struct timespec const ts[2],
             int atflag)
{
  int result = 1;

  if (0 <= fd)
    result = futimens (fd, ts);
  if (file && (fd < 0 || (result == -1 && errno == ENOSYS)))
    result = utimensat (dir, file, ts, atflag);
  if (result == 1)
    {
      errno = EBADF;
      result = -1;
    }

  return result;
}