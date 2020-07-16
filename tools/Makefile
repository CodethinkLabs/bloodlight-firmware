CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11 -ludev
LDFLAGS=-g

all: bl convert calibrate
clean:
	rm -f bl convert *.o

bl: bl.c msg.o find_device.o
convert: convert.c msg.o
calibrate: calibrate.c msg.o
msg.o: msg.c
find_device.o: find_device.c
