CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11
LDFLAGS=-g -ludev

all: bl convert calibrate
clean:
	rm -f bl convert calibrate *.o

bl: bl.c msg.o device.o sig.o
	$(CC) -o $@ $^ $(LDFLAGS)

convert: convert.c msg.o sig.o
	$(CC) -o $@ $^ $(LDFLAGS)

calibrate: calibrate.c msg.o sig.o
	$(CC) -o $@ $^ $(LDFLAGS)
