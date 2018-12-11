CC=gcc
CFLAGS=-lpthread -lm -Wall
OBJS=wof_benchmark.c
BINS=wof_test

${BINS}: ${OBJS}
	${CC} ${OBJS} ${CFLAGS} -o $@ -O1 -g

clean: 
	-rm -f *.o ${BINS}
