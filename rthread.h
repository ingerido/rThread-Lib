/* Copyright (C) 2015 -
 
   		The rThread package is a "POSIX pThread-like" library, including basic function of 
		thread in linux system. rThread library implements kernel level thread creation, 
		synchronization (mutex and condition variable) and destroy operations. In addtion,
   		rThread library also provide "pure" user level thread, which is totally scheduled 
		and managed in user mode and totally opaque to the linux kernel.

 * Author: Yujie REN 
 * Date: 09/26/2015 - 10/11/2015
*/

#ifndef RTHREAD_H
#define RTHREAD_H

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sched.h>
#include <signal.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>

/* MACRO Definition */
#define KERNEL_LEVEL 0
#define USER_LEVEL   1

#define ULONLY       0
#define KLMATCHCORES 1
#define KLMATCHHYPER 2
#define KLALWAYS     3

#define _THREAD_STACK 1024*32

#define THREAD_MAX 16
#define THREAD_QUEUE_MAX (THREAD_MAX+1)

#define ERR_LOG(string) fprintf(stderr, "rThread: " string "\n")

/* TYPEDEF */
typedef uint threadMode;

typedef uint threadLevel;

typedef unsigned long int rthread_t;

typedef enum bool {
	FALSE = 0,
	TRUE,
} bool;

/*********************************************************
 - rThread Status Definition
**********************************************************/
typedef enum threadStatus {
	NOT_STARTED = 0,
	RUNNING,
	HALTING,
	FINISHED,
} threadStatus;

/*********************************************************
 - rThread Control Block Implementation
**********************************************************/
typedef struct threadControlBlock{
	rthread_t   tid;			/* Thread ID            */
	threadLevel level;			/* Thread Level         */
	threadStatus status;		/* Thread Status        */
	ucontext_t context;			/* Thread Contex        */
	void* stack;				/* Thread Stack pointer */
} _tcb;

/*********************************************************
 - rThread User Level Run Queue Implementation
**********************************************************/
typedef struct threadQueue {
	_tcb* queue[THREAD_QUEUE_MAX];
	int head;
	int rear;
} threadQueue;

/* User level Thread Queue Definition */
static threadQueue _queue;

int enThreadQueue(_tcb* thd);

int deThreadQueue(_tcb** thd);

bool isThreadQueueEmpty();

/*********************************************************
 - Global Variable Definition
**********************************************************/
/* main thread context */
static ucontext_t context_main;

/* current running user level thread */
static _tcb *current_u_Thread = NULL;

/* number of active kernel threads */
static int kernel_thread_num = 0;

/* number of active threads */
static int user_thread_num = 0;

/* global spinlock for critical section _queue */
static uint _spinlock;

/*********************************************************
 - rThread Operation Implementation
**********************************************************/
/* initial rThread User level Package */
void rthread_init(void (*task_exec_func)(void*), void* arg);

/* create a new thread according to thread level */
int rthread_create(rthread_t* tid,
                   threadLevel level,
                   void (*start_func)(void*),
                   void* arg);

/* give CPU pocession to other user level threads voluntarily */
int rthread_yield();

/* after a thread terminating, delete this thread */
void rthread_exit(void* tid);

/* manage user level threads */
int rthread_manage();

/* start user level thread function */
static void u_thread_exec_func(void (*thread_func)(void*), void* arg, _tcb* newThread);

/* activation record for user level threads mounted in a kernel level thread */
int  k_thread_exec_func(void* arg);

/*********************************************************
 - Mutual Exclusive Lock
**********************************************************/
/* mutex struct definition */
typedef struct rthread_mutex_t {
	rthread_t owner_tid;
	uint lock;
} rthread_mutex_t;

/* global mutex variable */
//static rthread_mutex_t mutex;

/* initial the mutex lock */
int rthread_mutex_init(rthread_mutex_t* mutex, void* arg);

/* aquire the mutex lock */
int rthread_mutex_lock(rthread_mutex_t* mutex);

/* release the mutex lock */
int rthread_mutex_unlock(rthread_mutex_t* mutex);

/*********************************************************
 - Condition Variable
**********************************************************/
/* condition waiting sequence */
typedef struct rthread_cond_seq {
	_tcb* u_wait_thread;
	struct rthread_cond_seq* next;
} rthread_cond_seq;

/* condition variable struct definition */
typedef struct rthread_cond_t {
	//threadLevel thread_level;
	rthread_cond_seq* cond_seq;
	rthread_cond_seq* ready;
	rthread_mutex_t seq_mutex;
} rthread_cond_t;

/* global condition variable */
//static rthread_cond_t condvar;

/* initial condition variable */
int rthread_cond_init(rthread_cond_t* condvar, void* arg);

/* wake up all threads on waiting list */
int rthread_cond_broadcast(rthread_cond_t* condvar);

/* wake up a thread on waiting list */
int rthread_cond_signal(rthread_cond_t* condvar);

/* current thread go to sleep until other thread wakes me up*/
int rthread_cond_wait(rthread_cond_t* condvar, rthread_mutex_t* mutex);

#endif

/***********************  end of rthread.h  **************************/
