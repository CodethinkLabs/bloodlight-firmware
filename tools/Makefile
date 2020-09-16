CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11
LDFLAGS=-g -ludev

all: bl convert calibrate normalize fft
clean:
	rm -f bl convert calibrate normalize fft *.o

bl: bl.c msg.o device.o sig.o util.o
	$(CC) -o $@ $^ $(LDFLAGS)

convert: convert.c msg.o sig.o fifo.o
	$(CC) -o $@ $^ $(LDFLAGS)

calibrate: calibrate.c msg.o sig.o
	$(CC) -o $@ $^ $(LDFLAGS)

normalize: normalize.c msg.o sig.o fifo.o util.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

fft: fft.c msg.o sig.o util.o fifo.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lfftw3 -lm
