/* Copyright (C) 2015 - 2016
 
   		The rThread package is a light-weight thread library, including basic function of 
		thread in linux system. rThread library implements user level thread creation, 
		synchronization (mutex and condition variable) and destroy operations. rThread Library
		combines advantages of kernel level threads and user level threads; in rThread Library
		kernel threads runs in a loop grabs available user threads to run. And the user could
		modify the schedule strategies in user level threads.

 * Author: Yujie REN 
 * Date:   09/26/2015 - 10/11/2015
 * update: 10/10/2016 (Modify logic from tcb queue to queue ADT to improve code reusablility)
 * update: 10/12/2016 (Modify logic in thread creation, yield and eliminate runtime error)
 * update: 10/14/2016 (Modify logic in thread mutex and condition variable)
 * update: 10/15/2016 (Add logic in thread exit and thread join)
 * update: 10/17/2016 (Add Round Robin Scheduler for user level threads)
 * update: 11/06/2016 (Add rthread_mutex_destory() and rthread_cond_destory())
 * update: 11/13/2016 (Fix segmentation fault bugs and correct the logic in context switch)
 * update: 10/03/2017 (Modify logic from queue ADT to linked list ADT)
 * update: 10/06/2017 (Modify stack space for tcb by using struct hack)
*/

#ifndef RTHREAD_H
#define RTHREAD_H

#define _GNU_SOURCE

//#define _DEBUG_

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <assert.h>
#include <ucontext.h>
#include <sched.h>
#include <signal.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* MACRO Definition */
#define KERNEL_LEVEL 0
#define   USER_LEVEL 1

#define _THREAD_STACK 1024*32
#define U_THREAD_MAX 16
#define K_THREAD_MAX 4
#define K_CONTEXT_MASK 0b11
#define PRIORITY 16
#define TIME_QUANTUM 50000

#define ERR_LOG_FILE "rThread_err_log"

#define ERR_LOG(string) fprintf(stderr, "rThread: " string "\n")

#define GET_TCB(ll_ptr) \
				((_tcb*)((char*)(ll_ptr) - (unsigned long long)(&((_tcb*)0)->node)))

/* Typedef */
typedef uint threadMode;

typedef uint threadLevel;

typedef uint rthread_t;

/* Task Linked List */
typedef struct list_node {
	struct list_node *next, *prev;
} list_node;

/* rThread Status Definition */
typedef enum threadStatus {
	NOT_STARTED = 0,
	RUNNING,
	SUSPENDED,
	TERMINATED,
	FINISHED,
} threadStatus;

typedef enum schedPolicy {
	RR = 0,
	MLFQ,
} schedPolicy;

/* user_level Thread Control Block Definition */
typedef struct threadControlBlock {
	rthread_t   tid;			/* Thread ID            */
	threadStatus status;		/* Thread Status        */
	ucontext_t context;			/* Thread Contex        */
	uint       priority;        /* Thread Priority      */
	list_node  node;            /* Thread Node in Queue */
	char stack[1];				/* Thread Stack pointer */
} _tcb;

/* sig_semaphore Definition */
typedef struct sig_semaphore {
	sem_t semaphore;
	unsigned long *val;
} sig_semaphore;

/* mutex struct definition */
typedef struct rthread_mutex_t {
	_tcb *owner;
	uint lock;
	list_node wait_list;
} rthread_mutex_t;

/* condition variable struct definition */
typedef struct rthread_cond_t {
	list_node wait_list;
	rthread_mutex_t list_mutex;
} rthread_cond_t;

/*********************************************************
                    rThread Operation
**********************************************************/
/* initial rThread User level Package */
void rthread_init();

/* create a new thread according to thread level */
int rthread_create(rthread_t *tid,
                   threadLevel level,
                   void (*start_func)(void*),
                   void *arg);

/* give CPU pocession to other user level threads voluntarily */
int rthread_yield();

/* wait for thread termination */
int rthread_join(rthread_t thread, void **value_ptr);

/* terminate a thread */
void rthread_exit(void *retval);

/* schedule user level threads */
int rthread_schedule();

/* start user level thread wrapper function */
void u_thread_exec_func(void (*thread_func)(void*), void *arg, _tcb *newThread);

/* run kernel level thread function */
void k_thread_exec_func(void *arg);


/*********************************************************
				Mutual Exclusive Lock
**********************************************************/

/* initial the mutex lock */
int rthread_mutex_init(rthread_mutex_t *mutex);

/* aquire the mutex lock */
int rthread_mutex_lock(rthread_mutex_t *mutex);

/* release the mutex lock */
int rthread_mutex_unlock(rthread_mutex_t *mutex);

/* destory the mutex lock */
int rthread_mutex_destroy(rthread_mutex_t *mutex);

/*********************************************************
					Condition Variable
**********************************************************/

/* initial condition variable */
int rthread_cond_init(rthread_cond_t *condvar);

/* wake up all threads on waiting list */
int rthread_cond_broadcast(rthread_cond_t *condvar);

/* wake up a thread on waiting list */
int rthread_cond_signal(rthread_cond_t *condvar);

/* current thread go to sleep until other thread wakes it up*/
int rthread_cond_wait(rthread_cond_t *condvar, rthread_mutex_t *mutex);

/* destory condition variable */
int rthread_cond_destroy(rthread_cond_t *condvar);

#endif

/* -------------------------- end of rthread.h -------------------------- */
