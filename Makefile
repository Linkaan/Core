CC := gcc
INCLUDE ?= -I.
CFLAGS := $(INCLUDE) -std=gnu11 -g -Wall -Wextra -D _GNU_SOURCE
LDFLAGS := -lwiringPi -lpthread
SOURCES := core.c motion_handler.c
HEADERS := motion_handler.h
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE := fagelmatare-core

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.0: %.c $(HEADERS)
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)
