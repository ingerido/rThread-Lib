# rThread-Lib
This is a thread library implemented by me in Linux providing both user-level threads and kernel level threads

This directory contains the following files:

  README.md:        this file

  Makefile:         to compile rThread.a, test_yield, and test_condvar, 
                    just type "make".  Make sure you read this Makefile 
                    to understand how the various libraries
                    get linked into the various executables.

  rthread.h:        header file of rThread Library ("rThread" is named with the first letter of my last name REN)

  rthread.c:        source file of rThread Library

  test_yield.c:     test yield function(rthread_yield) of rThread Library

  test_condvar.c:   test condition variable and mutex of rThread Library
