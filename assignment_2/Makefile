CFLAGS=-Wall -Wpedantic -Werror

superserver: superserver.o
	gcc superserver.o ${CFLAGS} -o superserver

superserver.o: superserver.c
	gcc -c superserver.c ${CFLAGS} -o superserver.o

.PHONY: clean

clean:
	rm -rf *.o