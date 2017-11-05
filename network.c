/*
 *  network.c
 *    Implemenents callback function to handle events using fgevents library
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

#include <stdint.h>

#include "network.h"
#include "common.h"
#include "log.h"
#include "core.h"

static void
handle_sensor_event (struct thread_data *tdata, struct fgevent *fgev,
                     struct fgevent *ansev)
{
    ansev->id = FG_SENSOR_DATA;
    ansev->receiver = FG_DATALOGGER;
    ansev->writeback = 0;
    ansev->length = 10;

    ansev->payload = malloc (sizeof (int32_t) * ansev->length);

    memset (ansev->payload, 0, sizeof (int32_t) * ansev->length);

    read_cpu_temp (tdata);

    pthread_mutex_lock (&tdata->sensor_mutex);
    if (fgev->length > 0)
      {
        if (fgev->payload[0] == OUTTEMP)
          {
            tdata->sensor_data.outtemp = fgev->payload[1];
            ansev->payload[0] = OUTTEMP;
            ansev->payload[1] = fgev->payload[1];
          }
        if (fgev->payload[2] == INTEMP)
          {
            tdata->sensor_data.intemp = fgev->payload[3];
            ansev->payload[2] = INTEMP;
            ansev->payload[3] = fgev->payload[3];
          }
        if (fgev->payload[4] == PRESSURE)
          {
            tdata->sensor_data.pressure = fgev->payload[5];
            ansev->payload[4] = PRESSURE;
            ansev->payload[5] = fgev->payload[7];
          }
        if (fgev->payload[6] == HUMIDITY)
          {
            tdata->sensor_data.humidity = fgev->payload[7];
            ansev->payload[6] = HUMIDITY;
            ansev->payload[7] = fgev->payload[7];
          }
      }

    ansev->payload[8] = CPUTEMP;
    ansev->payload[9] = (int32_t) tdata->sensor_data.cputemp;
    pthread_mutex_unlock (&tdata->sensor_mutex);
}         

/* Returns 1 on should writeback; 0 if not*/
int
fg_handle_event (void *arg, struct fgevent *fgev, struct fgevent *ansev)
{
    int i;
    struct thread_data *tdata = arg;

    /* Handle error in fgevent */
    if (fgev == NULL)
      {
        log_error_en (tdata->etdata.save_errno, tdata->etdata.error);
        return 0;
      }

    switch (fgev->id)
      {
        case FG_SENSOR_DATA: /* TODO: consider creating a translation system for this event */
          handle_sensor_event (tdata, fgev, ansev);
          return 1;          
          break;
        default:
          _log_debug ("eventid: %d\n", fgev->id);
          break;                                                          
      }
      
    return 0;
}