CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

all:: test_condvar test_yield
	#strip test_condvar
	#strip test_yield

test_condvar: rthread.a test_condvar.o
	$(CC) -pthread -o test_condvar test_condvar.o -L./ -lrthread

test_yield: rthread.a test_yield.o
	$(CC) -pthread -o test_yield test_yield.o -L./ -lrthread

test_condvar.o: rthread.a
	$(CC) $(CFLAGS) test_condvar.c

test_yield.o: rthread.a
	$(CC) $(CFLAGS) test_yield.c 

rthread.a: rthread.o
	$(AR) librthread.a rthread.o
	$(RANLIB) librthread.a

rthread.o: rthread.h
	$(CC) $(CFLAGS) rthread.c

clean:
	rm -rf test_condvar test_yield rThread_err_log *.o *.a
