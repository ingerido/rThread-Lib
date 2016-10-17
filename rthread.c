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
*/

#include "rthread.h"

/* user level thread Queue Definition */
static Queue _thread_queue;

/* current user level Thread context (for user level thread only !) */
static ucontext_t *current_uthread_context;

/* kernel thread context */
static ucontext_t context_main;

/* number of active kernel threads */
static int _kernel_thread_num = 0;

/* number of active threads */
static int _user_thread_num = 0;

/* global spinlock for critical section _queue */
static uint _spinlock = 0;

/* global semaphore for user level thread */
static sem_t sem_thread[THREAD_MAX];

/* timer and signal for user level thread scheduling */
static struct sigaction schedHandle;

static struct itimerval timeQuantum;

static sigset_t sigProcMask;

/* For debugging */
static int t = 0;

/*********************************************************
				Queue ADT Implementation
**********************************************************/

int enQueue(Queue *q, void *element) 
{
	if (q->head == (q->rear + 1) % q->size) {
		ERR_LOG("queue full ...");
		return -1;
	} else {
		q->queue[q->rear] = element;
		q->rear = (q->rear + 1) % q->size;
		return 0;
	}		
}

int deQueue(Queue *q, void **element) 
{
	if (q->head == q->rear) {
		ERR_LOG("queue empty ...");
		return -1;
	} else {
		*element = q->queue[q->head];
		q->head = (q->head + 1) % q->size;
		return 0;
	}
}

int isQueueEmpty(Queue q) 
{
	if (q.head == q.rear)
		return -1;
	else
		return 0;
}

/*********************************************************
           rThread Operation Implementation
**********************************************************/

/* initial rThread User level Package */
void rthread_init(uint size) 
{
	/* Initialize log file */
	FILE * fp;
	fp = fopen ("rThread_log", "w");
	dup2(fileno(fp), STDERR_FILENO);

	/* Initialize _thread_queue */
	_thread_queue.size = size + 1;
	_thread_queue.queue = (void**)malloc(_thread_queue.size*sizeof(void*));
	assert (_thread_queue.queue);
	memset(_thread_queue.queue, 0, _thread_queue.size*sizeof(void*));
	_thread_queue.head = 0;
	_thread_queue.rear = 0;

}

/* create a new thread according to thread level */
int rthread_create(rthread_t *tid,
                   threadLevel level,
                   void (*start_func)(void*, void*),
                   void *arg) 
{
	if (USER_LEVEL == level) {
		if (_user_thread_num == THREAD_MAX) {
			/* exceed ceiling limit of user lever threads */
			ERR_LOG("User level threads limit exceeded!");
			return -1;
		}

		/* create a TCB for the new thread */
		_tcb* newThread = (_tcb*)malloc(sizeof(_tcb));

		/* allocate space for the newly created thread on stack */
		newThread->stack = (void*)malloc(_THREAD_STACK);
		if (NULL == newThread->stack) {
			ERR_LOG("Failed to allocate space for stack!");
			return -1;
		}

		/* set thread id and level */
		newThread->tid = _user_thread_num++;
		*tid = newThread->tid;

		/* Initialize sem_thread */
		sem_init(&sem_thread[newThread->tid], 0, 0);

		/* using uContext to create a context for this user level thread */
		if (-1 == getcontext(&newThread->context)) {
			ERR_LOG("Failed to get uesr context!");
			return -1;
		}

		/* set the context to a newly allocated stack */
		newThread->context.uc_link = &context_main;
		newThread->context.uc_stack.ss_sp = newThread->stack;
		newThread->context.uc_stack.ss_size = _THREAD_STACK;
		newThread->context.uc_stack.ss_flags = 0;	

		/* setting the context. The context calls a wrapper function, and then calls start_func */
		makecontext(&newThread->context, (void (*)(void))&u_thread_exec_func, 3, start_func, arg, newThread);

		/* add newly created user level thread to the user level thread run queue */	
		while(__sync_lock_test_and_set(&_spinlock, 1));
		enQueue(&_thread_queue, (void*)&newThread->context);	
		__sync_lock_release(&_spinlock);

	} else if (KERNEL_LEVEL == level) {
		uint core_num = sysconf( _SC_NPROCESSORS_ONLN );

		/* allocate space for the newly created thread on stack */
		void* stack = (void*)malloc(_THREAD_STACK);
		if (NULL == stack) {
			ERR_LOG("Failed to allocate space for stack!");
			return -1;
		}

		/* invoke the clone system call to create a light weight process (kernel thread) */
		*tid = clone((int (*)(void*))start_func, (char*) stack + _THREAD_STACK,
			SIGCHLD | CLONE_SIGHAND | CLONE_VM | CLONE_PTRACE, arg);
		if (-1 == *tid) {
			ERR_LOG("Failed to invoke Clone System Call!");
			free(stack);
			return -1;
		}
	}

	return 0;
}

/* give CPU pocession to other user level threads voluntarily */
int rthread_yield(void *uc) 
{
	_tcb* current_tcb = GET_TCB(uc, _tcb, context);
	while(__sync_lock_test_and_set(&_spinlock, 1));

	if (RUNNING == current_tcb->status) {
		#ifdef _DEBUG_
		printf("yielding... !\n");
		printf("****** status = %d ******!\n", current_tcb->status);
		#endif

		current_tcb->status = SUSPENDED;
		enQueue(&_thread_queue, uc);
		__sync_lock_release(&_spinlock);
		
		swapcontext((ucontext_t*)uc, &context_main);
	} else
		__sync_lock_release(&_spinlock);
	return 0;
}

/* wait for thread termination */
int rthread_join(rthread_t thread, void **value_ptr) 
{
	/* do P() in thread semaphore until the certain user level thread is done */
	sem_wait(&sem_thread[thread]);
	return 0;
}

/* terminate a thread */
void rthread_exit(void *tcb) 
{
	_tcb* current_tcb = (_tcb*)tcb;
	current_tcb->status = TERMINATED;
	fprintf(stdout, "User Level Thread tid = %d terminates!\n", current_tcb->tid);
	/* When this thread finished, delete TCB and yield CPU control */
	free(current_tcb->stack);
	free(current_tcb);
	setcontext(&context_main);	
}

/* schedule the user level threads */
static void schedule(int sig, siginfo_t *si, void *uc)
{
	/* Note: calling printf() from a signal handler is not
	  strictly correct, since printf() is not async-signal-safe;
	  see signal(7) */

	//sigprocmask(SIG_BLOCK, &sigProcMask, NULL);

	#ifdef _DEBUG_
	printf("times up !\n");
	t++;
	#endif

	while(__sync_lock_test_and_set(&_spinlock, 1));
	enQueue(&_thread_queue, (void*)current_uthread_context);
	__sync_lock_release(&_spinlock);
	
	swapcontext(current_uthread_context, &context_main);
	//sigprocmask(SIG_UNBLOCK, &sigProcMask, NULL);
}

int rthread_schedule() 
{
	/* Set Signal Mask */
	sigemptyset(&sigProcMask);
	sigaddset(&sigProcMask, SIGPROF);

    /* Set Signal Handler to Call Scheduler */
    memset(&schedHandle, 0, sizeof(schedHandle));
	schedHandle.sa_flags = SA_SIGINFO;
    schedHandle.sa_handler = &schedule;
    sigaction(SIGPROF, &schedHandle, NULL);

    timeQuantum.it_value.tv_sec = 0;
    timeQuantum.it_value.tv_usec = 500000;
    timeQuantum.it_interval.tv_sec = 0;
    timeQuantum.it_interval.tv_usec = 500000;
	setitimer(ITIMER_PROF, &timeQuantum, NULL);

	/*  grab and run a user level thread from 
		the user level thread queue, until no available 
        user level thread  */
	ucontext_t *next_thread_uc = NULL;
	while (1) {
		//sigprocmask(SIG_BLOCK, &sigProcMask, NULL);
		while(__sync_lock_test_and_set(&_spinlock, 1));
		
		if (0 != deQueue(&_thread_queue, (void**)&next_thread_uc)) {
			ERR_LOG("Failed to grab a user level thread from the queue!");
			__sync_lock_release(&_spinlock);
			//sigprocmask(SIG_UNBLOCK, &sigProcMask, NULL);
			break;
		}
		__sync_lock_release(&_spinlock);
		
		current_uthread_context = (ucontext_t*)next_thread_uc;
		swapcontext(&context_main, (ucontext_t*)next_thread_uc);
		//sigprocmask(SIG_UNBLOCK, &sigProcMask, NULL);
	}
	return 0;
}

/* start user level thread wrapper function */
void u_thread_exec_func(void (*thread_func)(void*, void*), void *arg, _tcb *newThread) 
{
	_tcb *u_thread = newThread;
	rthread_t current_id = u_thread->tid;

	u_thread->status = RUNNING;
	thread_func((void*)&u_thread->context, arg);
	u_thread->status = FINISHED;
	/* When this thread finished, delete TCB and yield CPU control */
	#ifdef _DEBUG_
	printf(" t = %d\n", t);
	#endif
	free(u_thread->stack);
	free(u_thread);
	_user_thread_num--;
	/* do V() in thread semaphore implies current user level thread is done */
	sem_post(&sem_thread[current_id]);
}

/* run kernel level thread function */
void k_thread_exec_func(void *arg, void *reserved) 
{
	char *t_name = (char*) arg;
	ucontext_t *run_thread_uc = NULL;
	fprintf(stdout, "I'm Kernel Level Thread \"%s\"  tid = %d \n", t_name, (int)syscall(SYS_gettid));
	
	/*  grab and run a user level thread from 
		the user level thread queue, until no available 
        user level thread  */
	while (1) {
TAS:	while(__sync_lock_test_and_set(&_spinlock, 1));

		if (0 != deQueue(&_thread_queue, (void**)&run_thread_uc)) {
			__sync_lock_release(&_spinlock);
			//goto TAS;
			exit(0);
		}
		__sync_lock_release(&_spinlock);

		swapcontext(&context_main, (ucontext_t*)run_thread_uc);
	}
}

/*********************************************************
				Mutual Exclusive Lock
**********************************************************/

/* initial the mutex lock */
int rthread_mutex_init(rthread_mutex_t *mutex) 
{
	mutex->owner = NULL;
	mutex->lock = 0;

	mutex->wait_list.size = 16;
	mutex->wait_list.queue = (void**)malloc(mutex->wait_list.size*sizeof(void*));
	assert (mutex->wait_list.queue);
	memset(mutex->wait_list.queue, 0, mutex->wait_list.size*sizeof(void*));
	mutex->wait_list.head = 0;
	mutex->wait_list.rear = 0;

	return 0;
}

/* aquire the mutex lock */
int rthread_mutex_lock(rthread_mutex_t *mutex, void *arg) 
{
	/* Use "test-and-set" atomic operation to aquire the mutex lock */
	while (__sync_lock_test_and_set(&mutex->lock, 1)) {
		enQueue(&mutex->wait_list, arg);
		swapcontext((ucontext_t*)arg, &context_main);
	}
	mutex->owner = GET_TCB(arg, _tcb, context);
	return 0;
}

/* release the mutex lock */
int rthread_mutex_unlock(rthread_mutex_t *mutex) 
{
	void *next_thread_context = NULL;
	if (0 != deQueue(&mutex->wait_list, &next_thread_context)) {
		__sync_lock_release(&mutex->lock);
		mutex->owner = NULL;
		return 0;
	}
	while(__sync_lock_test_and_set(&_spinlock, 1));
	enQueue(&_thread_queue, next_thread_context);	
	__sync_lock_release(&_spinlock);
	__sync_lock_release(&mutex->lock);
	mutex->owner = NULL;
	return 0;
}

/*********************************************************
					Condition Variable
**********************************************************/
/* initial condition variable */
int rthread_cond_init(rthread_cond_t *condvar) 
{
	condvar->wait_list.size = THREAD_MAX;
	condvar->wait_list.queue = (void**)malloc(condvar->wait_list.size*sizeof(void*));
	assert (condvar->wait_list.queue);
	memset(condvar->wait_list.queue, 0, condvar->wait_list.size*sizeof(void*));
	condvar->wait_list.head = 0;
	condvar->wait_list.rear = 0;

	return 0;
}

/* wake up all threads on waiting list of condition variable */
int rthread_cond_broadcast(rthread_cond_t *condvar) 
{
	void *next_thread_context = NULL;
	while (0 != deQueue(&condvar->wait_list, &next_thread_context)) {
		while(__sync_lock_test_and_set(&_spinlock, 1));
		enQueue(&_thread_queue, next_thread_context);	
		__sync_lock_release(&_spinlock);
	}
	return 0;
}

/* wake up a thread on waiting list of condition variable */
int rthread_cond_signal(rthread_cond_t *condvar) 
{
	void *next_thread_context = NULL;
	if (0 != deQueue(&condvar->wait_list, &next_thread_context)) {
		return 0;
	}
	while(__sync_lock_test_and_set(&_spinlock, 1));
	enQueue(&_thread_queue, next_thread_context);	
	__sync_lock_release(&_spinlock);
	return 0;
}

/* current thread go to sleep until other thread wakes it up */
int rthread_cond_wait(rthread_cond_t* condvar, rthread_mutex_t *mutex, void *arg) 
{
	void *current_thread_context = arg;
	enQueue(&condvar->wait_list, current_thread_context);
	rthread_mutex_unlock(mutex);
	swapcontext((ucontext_t*)current_thread_context, &context_main);
	rthread_mutex_lock(mutex, current_thread_context);
	return 0;
}

/* -------------------------- end of rthread.c -------------------------- */
