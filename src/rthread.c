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
*/

#include "rthread.h"

/* user level thread Queue Definition */
static list_node _thread_queue[PRIORITY];

/* current user level Thread context (for user level thread only !) */
static list_node *cur_node[K_THREAD_MAX];

/* kernel thread context */
static ucontext_t context_main[K_THREAD_MAX];

/* number of active kernel threads */
static int _kernel_thread_num = 0;

/* number of active threads */
static int _user_thread_num = 0;

/* global spinlock for critical section _queue */
static uint _spinlock = 0;

/* global semaphore for user level thread */
static sig_semaphore sigsem_thread[U_THREAD_MAX];

/* timer and signal for user level thread scheduling */
static struct sigaction schedHandle;

static struct itimerval timeQuantum;

static struct itimerval zero_timer = { 0 };

static int sched = 0;

/* For debugging */
static int t = 0;

/*********************************************************
				Linked List Operations
**********************************************************/

static inline int isQueueEmpty(list_node *q) {
	if ((q->prev == q) && (q->next == q))
		return -1;
	else
		return 0;
}

static inline void enQueue(list_node *q, list_node *node) {
	node->next = q;
	q->prev->next = node;
	node->prev = q->prev;
	q->prev = node;
}

static inline int deQueue(list_node *q, list_node **node) {
	if (-1 == isQueueEmpty(q))
		return -1;
	*node = q->next;
	q->next = q->next->next;
	q->next->prev = q;

	(*node)->next = NULL;
	(*node)->prev = NULL;

	return 0;
}

/*********************************************************
           rThread Operation Implementation
**********************************************************/

/* initial rThread User level Package */
void rthread_init() 
{
	/* Initialize error log file */
	FILE *fp;
	fp = freopen(ERR_LOG_FILE, "w", stderr);
	dup2(fileno(fp), fileno(stderr));
	
	/* Initialize time quantum */
	timeQuantum.it_value.tv_sec = 0;
	timeQuantum.it_value.tv_usec = TIME_QUANTUM;
	timeQuantum.it_interval.tv_sec = 0;
	timeQuantum.it_interval.tv_usec = TIME_QUANTUM;
}

/* create a new thread according to thread level */
int rthread_create(rthread_t *tid,
                   threadLevel level,
                   void (*start_func)(void*),
                   void *arg) 
{
	if (USER_LEVEL == level) {
		if (_user_thread_num == U_THREAD_MAX) {
			/* exceed ceiling limit of user lever threads */
			ERR_LOG("User level threads limit exceeded!");
			return -1;
		}

		/* create a TCB for the new thread */
		_tcb* newThread = (_tcb*)malloc(sizeof(_tcb) + 1 + _THREAD_STACK);

		/* allocate space for the newly created thread on stack */
		/*newThread->stack = (void*)malloc(_THREAD_STACK);
		if (NULL == newThread->stack) {
			ERR_LOG("Failed to allocate space for stack!");
			return -1;
		}*/

		if (0 == _user_thread_num) {
			/* Initialize time quantum */
			timeQuantum.it_value.tv_sec = 0;
			timeQuantum.it_value.tv_usec = TIME_QUANTUM;
			timeQuantum.it_interval.tv_sec = 0;
			timeQuantum.it_interval.tv_usec = TIME_QUANTUM;

			_thread_queue->prev = _thread_queue; 
			_thread_queue->next = _thread_queue;
		}

		/* set thread id and level */
		newThread->tid = _user_thread_num++;
		*tid = newThread->tid;

		// set initial priority to be the highest 
		newThread->priority = 0;

		// set node in thread run queue
		newThread->node.next = NULL;
		newThread->node.prev = NULL;

		/* Initialize sigsem_thread */
		sigsem_thread[newThread->tid].val = NULL;
		sem_init(&(sigsem_thread[newThread->tid].semaphore), 0, 0);

		/* using uContext to create a context for this user level thread */
		if (-1 == getcontext(&newThread->context)) {
			ERR_LOG("Failed to get uesr context!");
			return -1;
		}

		/* set the context to a newly allocated stack */
		newThread->context.uc_link = &context_main[0];
		newThread->context.uc_stack.ss_sp = newThread->stack;
		newThread->context.uc_stack.ss_size = _THREAD_STACK;
		newThread->context.uc_stack.ss_flags = 0;	

		/* setting the context. The context calls a wrapper function, and then calls start_func */
		makecontext(&newThread->context, (void (*)(void))&u_thread_exec_func, 3, start_func, arg, newThread);

		/* add newly created user level thread to the user level thread run queue */	
		while(__sync_lock_test_and_set(&_spinlock, 1));

		enQueue(_thread_queue + newThread->priority, &newThread->node);	

		//enQueue(&_thread_queue, (void*)&newThread->context);

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
int rthread_yield() 
{
	uint k_tid = (uint)syscall(SYS_gettid);
	_tcb* cur_tcb = GET_TCB(cur_node[k_tid & K_CONTEXT_MASK]);
	// _tcb* current_tcb = GET_TCB(current_uthread_context[k_tid & K_CONTEXT_MASK], _tcb, context);
	while(__sync_lock_test_and_set(&_spinlock, 1));

	if (RUNNING == cur_tcb->status) {
		#ifdef _DEBUG_
			fprintf(stdout, "yielding... !\n");
			fprintf(stdout, "****** status = %d ******!\n", current_tcb->status);
		#endif

		cur_tcb->status = SUSPENDED;
		// enQueue(&_thread_queue, current_uthread_context[k_tid & K_CONTEXT_MASK]);
		enQueue(_thread_queue + cur_tcb->priority, cur_node[k_tid & K_CONTEXT_MASK]);
		__sync_lock_release(&_spinlock);
		
		// swapcontext(current_uthread_context[k_tid & K_CONTEXT_MASK], &context_main[k_tid & K_CONTEXT_MASK]);
		swapcontext(&(cur_tcb->context), &context_main[k_tid & K_CONTEXT_MASK]);
	} else
		__sync_lock_release(&_spinlock);
	return 0;
}

/* wait for thread termination */
int rthread_join(rthread_t thread, void **value_ptr) 
{
	/* do P() in thread semaphore until the certain user level thread is done */
	sem_wait(&(sigsem_thread[thread].semaphore));
	/* get the value's location passed to rthread_exit */
	if (value_ptr && sigsem_thread[thread].val)
		memcpy((unsigned long*)*value_ptr, sigsem_thread[thread].val, sizeof(unsigned long));
	return 0;
}

/* terminate a thread */
void rthread_exit(void *retval) 
{
	uint k_tid = (uint)syscall(SYS_gettid);
	// _tcb* current_tcb = GET_TCB(current_uthread_context[k_tid & K_CONTEXT_MASK], _tcb, context);
	_tcb* cur_tcb = GET_TCB(cur_node[k_tid & K_CONTEXT_MASK]);
	rthread_t current_id = cur_tcb->tid;

	cur_tcb->status = TERMINATED;
	#ifdef _DEBUG_
		fprintf(stdout, "User Level Thread tid = %d terminates!\n", current_tcb->tid);
	#endif
	/* When this thread finished, delete TCB and yield CPU control */
	_user_thread_num--;
	
	sigsem_thread[current_id].val = (unsigned long*)malloc(sizeof(unsigned long));
	memcpy(sigsem_thread[current_id].val, retval, sizeof(unsigned long));

	while(__sync_lock_test_and_set(&_spinlock, 1));
	// enQueue(&_thread_queue, current_uthread_context[k_tid & K_CONTEXT_MASK]);	
	enQueue(_thread_queue + cur_tcb->priority, cur_node[k_tid & K_CONTEXT_MASK]);	
	__sync_lock_release(&_spinlock);

	// swapcontext(current_uthread_context[k_tid & K_CONTEXT_MASK], &context_main[k_tid & K_CONTEXT_MASK]);
	swapcontext(&(cur_tcb->context), &context_main[k_tid & K_CONTEXT_MASK]);
}

/* schedule the user level threads */
static void schedule()
{
	/* Note: calling printf() from a signal handler is not
	  strictly correct, since printf() is not async-signal-safe;
	  see signal(7) */

	uint k_tid = (uint)syscall(SYS_gettid);
	
	_tcb* cur_tcb = GET_TCB(cur_node[k_tid & K_CONTEXT_MASK]);

	#ifdef _DEBUG_
		fprintf(stdout, "times up !\n");
		t++;
	#endif

	while(__sync_lock_test_and_set(&_spinlock, 1));

	cur_tcb->status = SUSPENDED;

	if (MLFQ == sched && cur_tcb->priority < PRIORITY - 1)
		++cur_tcb->priority;

	// enQueue(&_thread_queue, (void*)current_uthread_context[k_tid & K_CONTEXT_MASK]);
	enQueue(_thread_queue + cur_tcb->priority, cur_node[k_tid & K_CONTEXT_MASK]);

	__sync_lock_release(&_spinlock);
	
	// swapcontext(current_uthread_context[k_tid & K_CONTEXT_MASK], &context_main[k_tid & K_CONTEXT_MASK]);
	swapcontext(&(cur_tcb->context), &context_main[k_tid & K_CONTEXT_MASK]);
}

int rthread_schedule() 
{
	/* Set Signal Handler to Call Scheduler */
	memset(&schedHandle, 0, sizeof(schedHandle));
	schedHandle.sa_flags = SA_SIGINFO;
	schedHandle.sa_handler = &schedule;
	sigaction(SIGPROF, &schedHandle, NULL);

	setitimer(ITIMER_PROF, &timeQuantum, NULL);

	uint k_tid = (uint)syscall(SYS_gettid);

	/* thread TCB and context */
	// ucontext_t *next_thread_uc = NULL;
	// _tcb* current_tcb = NULL;

	list_node *run_node = NULL;
	_tcb* run_tcb = NULL;

	/*  grab and run a user level thread from 
		the user level thread queue, until no available 
        user level thread  */

	while (1) {
		while(__sync_lock_test_and_set(&_spinlock, 1));
		
		// if (0 != deQueue(&_thread_queue, (void**)&next_thread_uc)) {
		if (0 != deQueue(_thread_queue, &run_node)) {
			ERR_LOG("Failed to grab a user level thread from the queue!");
			__sync_lock_release(&_spinlock);

			setitimer(ITIMER_PROF, &zero_timer, &timeQuantum);

			break;
		}
		__sync_lock_release(&_spinlock);

		//current_tcb = GET_TCB((ucontext_t*)next_thread_uc, _tcb, context);
		run_tcb = GET_TCB(run_node);		

		/* current user thread is already terminated or finished by rthread_exit() */
		if (TERMINATED == run_tcb->status || FINISHED == run_tcb->status) {
			/* do V() in thread semaphore implies current user level thread is done */
			sem_post(&(sigsem_thread[run_tcb->tid].semaphore));
			// free(current_tcb->stack);
			free(run_tcb);
			_user_thread_num--;
			continue;
		}

		run_tcb->status = RUNNING;
		cur_node[k_tid & K_CONTEXT_MASK] = run_node;
		// swapcontext(&context_main[k_tid & K_CONTEXT_MASK], (ucontext_t*)next_thread_uc);
		swapcontext(&context_main[k_tid & K_CONTEXT_MASK], &(run_tcb->context));
	}
	return 0;
}

/* start user level thread wrapper function */
void u_thread_exec_func(void (*thread_func)(void*), void *arg, _tcb *newThread) 
{
	uint k_tid = 0;

	_tcb *u_thread = newThread;
	rthread_t current_id = u_thread->tid;

	u_thread->status = RUNNING;
	thread_func(arg);
	u_thread->status = FINISHED;

	k_tid = (uint)syscall(SYS_gettid);

	u_thread->context.uc_link = &context_main[k_tid & K_CONTEXT_MASK];

	/* When this thread finished, delete TCB and yield CPU control */
	#ifdef _DEBUG_
		fprintf(stdout, " t = %d\n", t);
	#endif

	while(__sync_lock_test_and_set(&_spinlock, 1));
	// enQueue(&_thread_queue, current_uthread_context[k_tid & K_CONTEXT_MASK]);
	enQueue(_thread_queue + u_thread->priority, cur_node[k_tid & K_CONTEXT_MASK]);

	__sync_lock_release(&_spinlock);

	// swapcontext(current_uthread_context[k_tid & K_CONTEXT_MASK], &context_main[k_tid & K_CONTEXT_MASK]);
	swapcontext(&u_thread->context, &context_main[k_tid & K_CONTEXT_MASK]);

}

/* run kernel level thread function */
void k_thread_exec_func(void *arg) 
{
	uint k_tid = (uint)syscall(SYS_gettid);

	char *t_name = (char*) arg;

	//ucontext_t *next_thread_uc = NULL;
	//_tcb* current_tcb = NULL;

	list_node *run_node = NULL;
	_tcb* run_tcb = NULL;	

	fprintf(stdout, "I'm Kernel Level Thread \"%s\"  tid = %d \n", t_name, k_tid);

	/* Set Signal Handler to Call Scheduler */
	memset(&schedHandle, 0, sizeof(schedHandle));
	schedHandle.sa_flags = SA_SIGINFO;
	schedHandle.sa_handler = &schedule;
	sigaction(SIGPROF, &schedHandle, NULL);

	setitimer(ITIMER_PROF, &timeQuantum, NULL);

	/*  grab and run a user level thread from 
		the user level thread queue, until no available 
        user level thread  */
	while (1) {
TAS:	while(__sync_lock_test_and_set(&_spinlock, 1));

		// if (0 != deQueue(&_thread_queue, (void**)&next_thread_uc)) {
		if (0 != deQueue(_thread_queue, &run_node)) {
			__sync_lock_release(&_spinlock);

			setitimer(ITIMER_PROF, &zero_timer, &timeQuantum);
			//goto TAS;
			//exit(0);
			return;
		}
		__sync_lock_release(&_spinlock);

		// current_tcb = GET_TCB((ucontext_t*)next_thread_uc, _tcb, context);
		run_tcb = GET_TCB(run_node);		

		/* current user thread is already terminated or finished by rthread_exit() */
		if (TERMINATED == run_tcb->status || FINISHED == run_tcb->status) {
			/* do V() in thread semaphore implies current user level thread is done */
			sem_post(&(sigsem_thread[run_tcb->tid].semaphore));
			// free(current_tcb->stack);
			free(run_tcb);
			_user_thread_num--;
			continue;
		}

		run_tcb->status = RUNNING;
		// current_uthread_context[k_tid & K_CONTEXT_MASK] = (ucontext_t*)next_thread_uc;
		// swapcontext(&context_main[k_tid & K_CONTEXT_MASK], (ucontext_t*)next_thread_uc);
		cur_node[k_tid & K_CONTEXT_MASK] = run_node;
		swapcontext(&context_main[k_tid & K_CONTEXT_MASK], &(run_tcb->context));	
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

	(&(mutex->wait_list))->prev = &(mutex->wait_list); 
	(&(mutex->wait_list))->next = &(mutex->wait_list);

	return 0;
}

/* aquire the mutex lock */
int rthread_mutex_lock(rthread_mutex_t *mutex) 
{
	uint k_tid = (uint)syscall(SYS_gettid);
	/* Use "test-and-set" atomic operation to aquire the mutex lock */
	/*while (__sync_lock_test_and_set(&mutex->lock, 1)) {
		enQueue(&mutex->wait_list, current_uthread_context[k_tid & K_CONTEXT_MASK]);
		swapcontext(current_uthread_context[k_tid & K_CONTEXT_MASK], &context_main[k_tid & K_CONTEXT_MASK]);
	}
	mutex->owner = GET_TCB(current_uthread_context[k_tid & K_CONTEXT_MASK], _tcb, context);*/

	while (__sync_lock_test_and_set(&mutex->lock, 1)) {
		enQueue(&mutex->wait_list, cur_node[k_tid & K_CONTEXT_MASK]);
		swapcontext(&(GET_TCB(cur_node[k_tid & K_CONTEXT_MASK])->context), &context_main[k_tid & K_CONTEXT_MASK]);
	}
	mutex->owner = GET_TCB(cur_node[k_tid & K_CONTEXT_MASK]);

	return 0;
}

/* release the mutex lock */
int rthread_mutex_unlock(rthread_mutex_t *mutex) 
{
	/*void *next_thread_context = NULL;
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
	return 0;*/
	list_node *next_node = NULL;
	_tcb* cur_tcb = NULL;
	if (0 != deQueue(&(mutex->wait_list), &next_node)) {
		__sync_lock_release(&mutex->lock);
		mutex->owner = NULL;
		return 0;
	}
	cur_tcb = GET_TCB(next_node);
	cur_tcb->priority = 0;
	while(__sync_lock_test_and_set(&_spinlock, 1));
	enQueue(_thread_queue + cur_tcb->priority, next_node);	
	__sync_lock_release(&_spinlock);
	__sync_lock_release(&mutex->lock);
	mutex->owner = NULL;
	return 0;
}

/* destory the mutex lock */
int rthread_mutex_destroy(rthread_mutex_t *mutex)
{
	/*if (0 == mutex->lock){
		free(mutex->wait_list.queue);
		return 0;
	}
	return -1;*/
	return 0;
}

/*********************************************************
					Condition Variable
**********************************************************/
/* initial condition variable */
int rthread_cond_init(rthread_cond_t *condvar) 
{
	/*condvar->wait_list.size = U_THREAD_MAX;
	condvar->wait_list.queue = (void**)malloc(condvar->wait_list.size*sizeof(void*));
	assert (condvar->wait_list.queue);

	memset(condvar->wait_list.queue, 0, condvar->wait_list.size*sizeof(void*));
	condvar->wait_list.head = 0;
	condvar->wait_list.rear = 0;*/

	(&(condvar->wait_list))->prev = &(condvar->wait_list); 
	(&(condvar->wait_list))->next = &(condvar->wait_list);

	return 0;
}

/* wake up all threads on waiting list of condition variable */
int rthread_cond_broadcast(rthread_cond_t *condvar) 
{
	/*void *next_thread_context = NULL;
	while (0 != deQueue(&condvar->wait_list, &next_thread_context)) {
		while(__sync_lock_test_and_set(&_spinlock, 1));
		enQueue(&_thread_queue, next_thread_context);	
		__sync_lock_release(&_spinlock);
	}*/

	list_node *next_node = NULL;
	_tcb* cur_tcb = NULL;
	while (0 != deQueue(&(condvar->wait_list), &next_node)) {
		cur_tcb = GET_TCB(next_node);
		cur_tcb->priority = 0;
		while(__sync_lock_test_and_set(&_spinlock, 1));
		enQueue(_thread_queue + cur_tcb->priority, next_node);	
		__sync_lock_release(&_spinlock);
	}
	return 0;
}

/* wake up a thread on waiting list of condition variable */
int rthread_cond_signal(rthread_cond_t *condvar) 
{
	/*void *next_thread_context = NULL;
	if (0 != deQueue(&condvar->wait_list, &next_thread_context)) {
		return 0;
	}

	while(__sync_lock_test_and_set(&_spinlock, 1));
	enQueue(&_thread_queue, next_thread_context);	
	__sync_lock_release(&_spinlock);*/

	list_node *next_node = NULL;
	_tcb* cur_tcb = NULL;
	if (0 != deQueue(&(condvar->wait_list), &next_node)) {
		return 0;
	}

	cur_tcb = GET_TCB(next_node);
	cur_tcb->priority = 0;
	while(__sync_lock_test_and_set(&_spinlock, 1));
	enQueue(_thread_queue + cur_tcb->priority, next_node);	
	__sync_lock_release(&_spinlock);

	return 0;
}

/* current thread go to sleep until other thread wakes it up */
int rthread_cond_wait(rthread_cond_t* condvar, rthread_mutex_t *mutex) 
{
	/*uint k_tid = (uint)syscall(SYS_gettid);

	ucontext_t *current_thread_context = current_uthread_context[k_tid & K_CONTEXT_MASK];
	enQueue(&condvar->wait_list, current_thread_context);

	rthread_mutex_unlock(mutex);
	swapcontext((ucontext_t*)current_thread_context, &context_main[k_tid & K_CONTEXT_MASK]);
	rthread_mutex_lock(mutex);*/

	uint k_tid = (uint)syscall(SYS_gettid);

	list_node *node = cur_node[k_tid & K_CONTEXT_MASK];
	enQueue(&condvar->wait_list, node);

	rthread_mutex_unlock(mutex);
	swapcontext(&(GET_TCB(cur_node[k_tid & K_CONTEXT_MASK])->context), &context_main[k_tid & K_CONTEXT_MASK]);
	rthread_mutex_lock(mutex);

	return 0;
}

/* destory condition variable */
int rthread_cond_destroy(rthread_cond_t *condvar)
{
	//free(condvar->wait_list.queue);
	return 0;	
}

/* -------------------------- end of rthread.c -------------------------- */
