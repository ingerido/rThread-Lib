CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

all:: test_condvar test_yield

test_condvar: rthread.a src/test_condvar.o
	$(CC) -g -pthread -o test_condvar src/test_condvar.o -L./ -lrthread

test_yield: rthread.a src/test_yield.o
	$(CC) -g -pthread -o test_yield src/test_yield.o -L./ -lrthread

src/test_condvar.o: rthread.a
	$(CC) $(CFLAGS) -o src/test_condvar.o src/test_condvar.c

src/test_yield.o: rthread.a
	$(CC) $(CFLAGS) -o src/test_yield.o src/test_yield.c 

rthread.a: src/rthread.o
	$(AR) librthread.a src/rthread.o
	$(RANLIB) librthread.a

src/rthread.o: src/rthread.h
	$(CC) $(CFLAGS) -o src/rthread.o src/rthread.c

clean:
	rm -rf test_condvar test_yield rThread_err_log src/*.o *.a
