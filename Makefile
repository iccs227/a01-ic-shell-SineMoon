CC=g++
CFLAGS=-Wall -g 
BINARY=icsh

all: icsh

icsh: icsh.cpp
	$(CC) -o $(BINARY) $(CFLAGS) $<

.PHONY: clean

clean:
	rm -f $(BINARY)
