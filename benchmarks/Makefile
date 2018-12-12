CC=gcc
CFLAGS=-lpthread -lm -Wall
OBJS=wof_benchmark.c
BINS=wof_bench

${BINS}: ${OBJS}
	${CC} ${OBJS} ${CFLAGS} -o $@ -O1 -g

clean: 
	-rm -f *.o ${BINS}
