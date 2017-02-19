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

	_log_debug ("eventid: %d\n", fgev->id);
	for (i = 0; i < fgev->length; i++)
	  {
	  	_log_debug ("%d\n", fgev->payload[i]);
	  }

	if (!ansev->writeback)
		return 0;

	ansev->id = 5;
	ansev->writeback = 0;
	ansev->length = 0;
	return 1;
}