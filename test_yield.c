#include "time.h"
#include "rthread.h"

rthread_mutex_t   mutex;
rthread_cond_t    condvar;

int count = 0;

void thread_one() {
	count++;
	printf("I'm User Level Thread 1\n");
	printf("yielding to User Level Thread 2\n");
	rthread_yield();
	printf("User Level Thread 1 live again!\n");
	printf("User Level Thread 1 finished\n");
	count--;
}

void thread_two() {
	count++;
	printf("I'm User Level Thread 2\n");
	printf("yielding to User Level Thread 1\n");
	rthread_yield();
	printf("User Level Thread 2 live again!\n");
	printf("User Level Thread 2 finished\n");
	count--;
}

void rth(void* arg) {
	count++;
	char *t_name = (char *) arg;
	fprintf(stdout, "User Level Thread \"%s\" start !\n", t_name);	
	int i = 0;
	for (i = 0; i < 0xFFFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running... in tid = %d \n", t_name, getpid());
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	rthread_yield();
	sleep(1);
	fprintf(stdout, "User Level Thread \"%s\" resume !\n", t_name);	
	for (i = 0; i < 0xFFFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running... in tid = %d \n", t_name, getpid());		
	}		
	fprintf(stdout, "User Level Thread \"%s\" finish !\n", t_name);	
	count--;
}

int main() {

	rthread_t tid;

	rthread_init(NULL, NULL);
	rthread_mutex_init(&mutex, NULL);
	rthread_cond_init(&condvar, NULL);

	rthread_create(&tid, USER_LEVEL, &rth, "1");
	rthread_create(&tid, USER_LEVEL, &rth, "2");
	rthread_create(&tid, USER_LEVEL, &rth, "3");

	rthread_create(&tid, KERNEL_LEVEL, &k_thread_exec_func, "1");

   while(1) {
      sleep(2);
      if (count == 0)
         break;
    }

	return 0;
}

