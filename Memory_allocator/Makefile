CC=gcc
CFLAGS=-fPIC -shared
SO_FILES=memalloc.so

all:
	${CC} -o memalloc.so ${CFLAGS} memalloc.c

clean:
	-rm -f ${SO_FILES}
