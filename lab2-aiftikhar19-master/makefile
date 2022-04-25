CC=gcc
CFLAGS = -c -O3 -g -Wall
LFLAGS = -O3 -g -Wall
all: ftpc.o
	${CC} ${LFLAGS} ftpc.o -o ftpc
ftpc.o:ftpc.c
	${CC} ${CFLAGS} ftpc.c
clean:
	rm -f ftpc *.o *~ recvd*
