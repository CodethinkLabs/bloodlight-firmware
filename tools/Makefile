CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11 -I ..
LDFLAGS=-g -ludev

all: bl bpm convert calibrate fft normalize
clean:
	rm -f bl bpm convert calibrate fft normalize *.o

bl: bl.c msg.o device.o sig.o util.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

convert: convert.c msg.o sig.o fifo.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

calibrate: calibrate.c msg.o sig.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

normalize: normalize.c msg.o sig.o fifo.o util.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

fft: fft.c msg.o sig.o util.o fifo.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lfftw3 -lm

bpm: bpm.c msg.o sig.o util.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

