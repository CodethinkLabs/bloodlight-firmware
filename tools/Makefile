CFLAGS=-g -O2 -Wall -Wextra -pedantic --std=gnu11
LDFLAGS=-g

all: bl convert
clean:
	rm -f bl convert *.o

bl: bl.c common.o conversion.o
convert: convert.c common.o conversion.o
common.o: common.c
conversion.o: conversion.c
