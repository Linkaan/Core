#
# Makefile:
#	defines rules to build the fagelmatare core module. Setup for cross
#   compilation with crosstool-ng
##############################################################################
#  This file is part of Fågelmataren, an advanced bird feeder equipped with
#  many peripherals. See <https://github.com/Linkaan/Fagelmatare>
#  Copyright (C) 2015-2016 Linus Styrén
#
#  Fågelmataren is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the Licence, or
#  (at your option) any later version.
#
#  Fågelmataren is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public Licence for more details.
#
#  You should have received a copy of the GNU General Public Licence
#  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
##############################################################################

INCLUDE ?= -I.
LINKS ?= -L.
CFLAGS := $(INCLUDE) -std=gnu11 -g -Wall -Wextra -D _GNU_SOURCE
LDFLAGS := $(LINKS) -lwiringPi -lpthread
SOURCES := core.c motion.c picam_state.c timeout.c touch.c
HEADERS := common.h motion.h picam_state.h tiemout.h touch.h 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE := fagelmatare-core

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.0: %.c $(HEADERS)
	ifndef CC
	$(error CC not set, please invoke with CC set to path of arm-rpi-linux-gnueabihf-gcc)
	endif
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)
