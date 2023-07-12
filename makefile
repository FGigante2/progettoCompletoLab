CC=gcc
CFLAGS=-std=gnu99 -Wall -Wno-unused-variable -Wno-unused-result -g -O -pthread
LDLIBS=-lm -lrt -pthread

all: archivio client1 client2

archivio: archivio.o funzioni.o 
	gcc $(CFLAGS) funzioni.o archivio.o $(LDLIBS) -o archivio
	
client1: xclient1.o
	gcc $(CFLAGS) client1.o $(LDLIBS) -o client1
	
client2: xclient2.o
	gcc $(CFLAGS) client2.o $(LDLIBS) -o client2
	
archivio.o: archivio.c funzioni.h
	gcc $(CFLAGS) -c archivio.c
	
funzioni.o: funzioni.c funzioni.h
	gcc $(CFLAGS) -c funzioni.c
	
client1.o: client1.c
	gcc $(CFLAGS) -c client1.c

client2.o: client2.c
	gcc $(CFLAGS) -c client2.c
