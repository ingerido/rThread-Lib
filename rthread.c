/* Copyright (C) 2015 -
 
   		The rThread package is a "POSIX pThread-like" library, including basic function of 
		thread in linux system. rThread library implements kernel level thread creation, 
		synchronization (mutex and condition variable) and destroy operations. In addtion,
   		rThread library also provide "pure" user level thread, which is totally scheduled 
		and managed in user mode and totally opaque to the linux kernel.

 * Author: Yujie REN 
 * Date: 09/26/2015 - 10/11/2015
*/

#include "rthread.h"

/*********************************************************
 - rThread User Level Run Queue Implementation
**********************************************************/

int enThreadQueue(_tcb* thd) {
	if (_queue.head == (_queue.rear + 1) % THREAD_QUEUE_MAX) {
		ERR_LOG("User level thread run queue is full!");
		return -1;
	} else {
		_queue.queue[_queue.rear] = thd;
		_queue.rear = (_queue.rear + 1) % THREAD_QUEUE_MAX;
		user_thread_num++;
		return 0;
	}		
}

int deThreadQueue(_tcb** thd) {
	if (_queue.head == _queue.rear) {
		ERR_LOG("User level thread run queue is empty!");
		return -1;
	} else {
		*thd = _queue.queue[_queue.head];
		//memcpy(thd, _queue.queue[_queue.head], sizeof(_tcb));
		_queue.head = (_queue.head + 1) % THREAD_QUEUE_MAX;
		user_thread_num--;
		return 0;
	}
}

bool isThreadQueueEmpty() {
	if (_queue.head == _queue.rear) {
		return TRUE;
	}
	return FALSE;
}

/*********************************************************
 - rThread Operation Implementation
**********************************************************/

/* initial rThread User level Package */
void rthread_init(void (*task_exec_func)(void*), void* arg) {
	/* initial the thread queue */
	int i = 0;
	for (i = 0; i < THREAD_QUEUE_MAX; i++) {
		_queue.queue[i] = NULL;
	}
	_queue.head = 0;
	_queue.rear = 0;

	// --- The following Part is to set the mode of thread creating ---
	/*
	threadMode mode;
	rthread_t *tid;
	printf("\n\n ----------- Please Select Thread Mode -----------\n\n");
	scanf("%d", &mode);
	if (ULONLY == mode) {
		tid = malloc(sizeof(rthread_t));
		if(rthread_create(&tid[i], USER_LEVEL, &task_exec_func, arg)) {
			fprintf(stderr, "rThread user level thread create error!\n");	
			return NULL;		
		}
	}
	if (KLMATCHCORES == mode) {
		uint core_num = sysconf( _SC_NPROCESSORS_ONLN );
		tid = malloc(core_num*sizeof(rthread_t));
		for (i = 0; i < core_num/2; i++) {
			if(rthread_create(&tid[i], USER_LEVEL, &task_exec_func, arg)) {
				fprintf(stderr, "rThread user level thread create error!\n");	
				return NULL;		
			}
		} 
		for (i = core_num/2; i <= core_num; i++) {
			if(rthread_create(&tid[i], KERNEL_LEVEL, &k_thread_exec_func, NULL)) {
				fprintf(stderr, "rThread user level thread create error!\n");	
				return NULL;		
			}
		} 
	}
	if (KLMATCHHYPER == mode) {
		uint core_num = sysconf( _SC_NPROCESSORS_ONLN );
		tid = malloc(core_num*2*sizeof(rthread_t));
		for (i = 0; i < core_num; i++) {
			if(rthread_create(&tid[i], USER_LEVEL, &task_exec_func, arg)) {
				fprintf(stderr, "rThread user level thread create error!\n");	
				return NULL;		
			}
		} 
		for (i = core_num; i <= core_num*2; i++) {
			if(rthread_create(&tid[i], KERNEL_LEVEL, &k_thread_exec_func, NULL)) {
				fprintf(stderr, "rThread user level thread create error!\n");	
				return NULL;		
			}
		} 
	}
	if (KLALWAYS == mode) {
		tid = malloc(2*sizeof(rthread_t));
		if(rthread_create(&tid[i], USER_LEVEL, &task_exec_func, arg)) {
			fprintf(stderr, "rThread user level thread create error!\n");	
			return NULL;		
		}
		if(rthread_create(&tid[i], KERNEL_LEVEL, &k_thread_exec_func, NULL)) {
			fprintf(stderr, "rThread user level thread create error!\n");	
			return NULL;		
		}
	}
	*/
}

/* create a new thread according to thread level */
int rthread_create(rthread_t* tid,
                   threadLevel level,
                   void (*start_func)(void*),
                   void* arg) {
	/* create a TCB for the new thread */
	_tcb* newThread = (_tcb*)malloc(sizeof(_tcb));

	/* allocate space for the newly created thread on stack */
	newThread->stack = malloc(_THREAD_STACK);
	if (NULL == newThread->stack) {
		ERR_LOG("Failed to allocate space for stack!");
		return -1;
	}
	if (USER_LEVEL == level) {
		if (user_thread_num == THREAD_MAX) {
			/* exceed ceiling limit of user lever threads */
			free(newThread->stack);
			free(newThread);
			ERR_LOG("User level threads limit exceeded!");
			return -1;
		}

		/* set thread id and level */
		newThread->tid = user_thread_num;
		newThread->level = USER_LEVEL;

		/* using uContext to create a context for this user level thread */
		if (-1 == getcontext(&newThread->context)) {
			ERR_LOG("Failed to get uesr context!");
			return -1;
		}

		/* set the context to a newly allocated stack */
		newThread->context.uc_link = 0;
		newThread->context.uc_stack.ss_sp = newThread->stack;
		newThread->context.uc_stack.ss_size = _THREAD_STACK;
		newThread->context.uc_stack.ss_flags = 0;	

		/* create the context. The context calls start_func */
		makecontext(&newThread->context, (void (*)(void))&u_thread_exec_func, 3, start_func, arg, newThread);

		/* add newly created user level thread to the user level thread run queue */
		enThreadQueue(newThread);

	} else if (KERNEL_LEVEL == level) {
		uint core_num = sysconf( _SC_NPROCESSORS_ONLN );

		/* invoke the clone system call to create a kernel level thread */
		newThread->tid = clone((int (*)(void*))start_func, (char*) newThread->stack + _THREAD_STACK,
			SIGCHLD | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM, arg);
		if (-1 == newThread->tid) {
			free(newThread->stack);
			ERR_LOG("Failed to invoke Clone System Call!");
			return -1;
		}
		newThread->level = KERNEL_LEVEL;
	}

	newThread->status = NOT_STARTED;
	*tid = newThread->tid;
	return 0;
}

/* give CPU pocession to other user level threads voluntarily */
int rthread_yield() {
	if (RUNNING == current_u_Thread->status) {
		while(__sync_lock_test_and_set(&_spinlock, 1));
		
		current_u_Thread->status = HALTING;
		enThreadQueue(current_u_Thread);
		__sync_lock_release(&_spinlock);
		
		swapcontext(&current_u_Thread->context, &context_main);
	} 
	return 0;
}

/* after a thread terminating, delete this thread */
void rthread_exit(void* tid) {
	int* id = (int*)tid;
	kill(&id, 0);
}

/* manage the user level threads */
int rthread_manage() {;
	/* grab and run a user level thread from 
		the user level thread queue, until no available 
       user level thread */
	if (TRUE == isThreadQueueEmpty()) {
		ERR_LOG("User level thread run queue is empty!");
		return -1;
	}
	while (user_thread_num > 0) {
		while(__sync_lock_test_and_set(&_spinlock, 1));
		
		if (0 != deThreadQueue(&current_u_Thread)) {
			ERR_LOG("Failed to grab a user level thread from the queue!");
			return -1;
		}
		__sync_lock_release(&_spinlock);
		
		swapcontext(&context_main, &current_u_Thread->context);
	}	
	return 0;
}

/* start user level thread function */
void u_thread_exec_func(void (*thread_func)(void*), void* arg, _tcb* newThread) {
	_tcb* current_u_Thread = newThread;
	current_u_Thread->status = RUNNING;
	thread_func(arg);
	current_u_Thread->status = FINISHED;
	/* When this thread finished, delete TCB and yield CPU control */
	free(current_u_Thread->stack);
	free(current_u_Thread);
	setcontext(&context_main);
}

/* activation record for user level threads mounted in a kernel level thread */
int k_thread_exec_func(void* arg) {
	kernel_thread_num++;
	char *t_name = (char*) arg;
	fprintf(stdout, "I'm Kernel Level Thread \"%s\"  tid = %d \n", t_name, getpid());
	fprintf(stdout, "I'm waiting for running user level threads\n");
	//sleep(1);
	
	/* grab and run a user level thread from 
		the user level thread queue, until no available 
       user level thread */
	while (FALSE == isThreadQueueEmpty()) {
		while(__sync_lock_test_and_set(&_spinlock, 1));
		
		if (0 != deThreadQueue(&current_u_Thread)) {
			ERR_LOG("Failed to grab a user level thread from the queue!");
			return -1;
		}
		__sync_lock_release(&_spinlock);
		
		swapcontext(&context_main, &current_u_Thread->context);
	}	

	//sleep(1);
	fprintf(stdout, "All user level threads are finished, Kernel Level Thread \"%s\" quit\n", t_name);
	kernel_thread_num--;
	return 0;
}

/*********************************************************
 - Mutual Exclusive Lock
**********************************************************/

/* initial the mutex lock */
int rthread_mutex_init(rthread_mutex_t* mutex, void* arg) {
	mutex->owner_tid = 0;
	mutex->lock = 0;
	return 0;
}

/* aquire the mutex lock */
int rthread_mutex_lock(rthread_mutex_t* mutex) {
	/* Use "test-and-set" atomic operation to aquire the mutex lock */
	while (__sync_lock_test_and_set(&mutex->lock, 1));
	mutex->owner_tid = getpid();
	return 0;
}

/* release the mutex lock */
int rthread_mutex_unlock(rthread_mutex_t* mutex) {
	/* Use "test-and-set" atomic operation to release the mutex lock */
	__sync_lock_release(&mutex->lock);
	mutex->owner_tid = 0;
	return 0;
}

/*********************************************************
 - Condition Variable
**********************************************************/
/* initial condition variable */
int rthread_cond_init(rthread_cond_t* condvar, void* arg) {
	condvar->cond_seq = NULL;
	condvar->ready = NULL;
	rthread_mutex_init(&condvar->seq_mutex, NULL);
	return 0;
}

/* wake up all threads on waiting list */
int rthread_cond_broadcast(rthread_cond_t* condvar) {
	rthread_mutex_lock(&condvar->seq_mutex);
	if (NULL != condvar->cond_seq) {
		do {			
			while(__sync_lock_test_and_set(&_spinlock, 1));

			if (0 != enThreadQueue(condvar->cond_seq->u_wait_thread)) {
				ERR_LOG("Failed to grab a user level thread from the queue!");
				return -1;
			}
			__sync_lock_release(&_spinlock);

			condvar->cond_seq = condvar->cond_seq->next;
		} while (NULL != condvar->cond_seq);
	}
	rthread_mutex_unlock(&condvar->seq_mutex);
	return 0;
}

/* wake up a thread on waiting list */
int rthread_cond_signal(rthread_cond_t* condvar) {
	rthread_mutex_lock(&condvar->seq_mutex);
	if (NULL != condvar->cond_seq) {
		if (condvar->ready != NULL)
			current_u_Thread = condvar->ready->u_wait_thread;
		condvar->ready = condvar->cond_seq;
		condvar->cond_seq = condvar->cond_seq->next;
	} else {
		rthread_mutex_unlock(&condvar->seq_mutex);
		return -1;
	}
	rthread_mutex_unlock(&condvar->seq_mutex);
	return 0;
}

/* current thread go to sleep until other thread wakes me up*/
int rthread_cond_wait(rthread_cond_t* condvar, rthread_mutex_t* mutex) {
	/* release the mutex lock, and yield CPU resource to other threads */
	rthread_cond_seq* this;
	rthread_mutex_lock(&condvar->seq_mutex);
	this = (rthread_cond_seq*)malloc(sizeof(rthread_cond_seq));
	memset(this, 0, sizeof(rthread_cond_seq));

	this->u_wait_thread = current_u_Thread;

	this->next = condvar->cond_seq;
	condvar->cond_seq = this;

	rthread_mutex_unlock(&condvar->seq_mutex);	
	
	rthread_mutex_unlock(mutex);
	if (condvar->ready == NULL) {
		swapcontext(&current_u_Thread->context, &context_main);
	} else {
		swapcontext(&current_u_Thread->context, &condvar->ready->u_wait_thread->context);
	}

	return 0;
}

/***********************  end of rthread.c  **************************/
