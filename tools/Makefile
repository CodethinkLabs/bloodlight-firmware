CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11
LDFLAGS=-g

all: bl convert
clean:
	rm -f bl convert *.o

bl: bl.c msg.o conversion.o
convert: convert.c msg.o conversion.o
msg.o: msg.c
conversion.o: conversion.c
