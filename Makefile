CC = gcc
CFLAGS = -O3
LIBS = -lrt

all: readshm rudpd sock2shmd

readshm:
	${CC} ${CFLAGS} readshm.c -o readshm
      
rudpd:
	${CC} ${CFLAGS} rudpd.c -o rudpd ${LIBS}

sock2shmd:
	${CC} ${CFLAGS} sock2shmd.c -o sock2shmd

clean:
	rm -rf readshm rudpd sock2shmd

