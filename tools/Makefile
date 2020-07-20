CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11
LDFLAGS=-g -ludev

all: bl convert calibrate
clean:
	rm -f bl convert *.o

bl: bl.c msg.o find_device.o
	$(CC) -o $@ $^ $(LDFLAGS)

convert: convert.c msg.o
	$(CC) -o $@ $^ $(LDFLAGS)

calibrate: calibrate.c msg.o find_device.o
	$(CC) -o $@ $^ $(LDFLAGS)
