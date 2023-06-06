CC = gcc
CFLAGS = -O3
LIBS = -lrt

all: readshm rudpd sock2shmd sendsocket

readshm:
	${CC} ${CFLAGS} readshm.c -o readshm
      
rudpd:
	${CC} ${CFLAGS} rudpd.c -o rudpd ${LIBS}

sock2shmd:
	${CC} ${CFLAGS} sock2shmd.c -o sock2shmd

sendsocket:
	${CC} ${CFLAGS} sendsocket.c -o sendsocket

ifeq ($(PREFIX),)
    PREFIX := /opt/burstt
endif

install: readshm rudpd sock2shmd
	install -d $(PREFIX)/bin
	install -m 755 readshm $(PREFIX)/bin
	install -m 755 rudpd $(PREFIX)/bin
	install -m 755 sock2shmd $(PREFIX)/bin
	install -m 755 sendsocket $(PREFIX)/bin

clean:
	rm -rf readshm rudpd sock2shmd sendsocket

