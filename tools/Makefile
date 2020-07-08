CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11
LDFLAGS=-g

all: bl convert calibrate
clean:
	rm -f bl convert *.o

bl: bl.c msg.o
convert: convert.c msg.o
calibrate: calibrate.c msg.o
msg.o: msg.c
