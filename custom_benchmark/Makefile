CC=gcc
CFLAGS=-fopenmp -lm -Wall
OBJS=test.c
BINS=a

a: ${OBJS}
	${CC} ${OBJS} ${CFLAGS} -o $@ -O0

clean: 
	-rm -f *.o ${BINS}
