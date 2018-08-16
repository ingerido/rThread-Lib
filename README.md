# rThread-Lib

rThread is a light-weight hybrid thread library with M:N mapping between user-level thread and kernel-level thread

This directory contains the following files:

  README.md:        this file

  Makefile:         to compile rThread.a, test_yield, and test_condvar, 
                    just type "make".  Make sure you read this Makefile 
                    to understand how the various libraries
                    get linked into the various executables.

  rthread.h:        header file of rThread Library

  rthread.c:        source file of rThread Library

  test_yield.c:     test yield function(rthread_yield) of rThread Library

  test_condvar.c:   test condition variable and mutex of rThread Library
