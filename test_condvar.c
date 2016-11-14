#include "time.h"
#include "rthread.h"

rthread_mutex_t   mutex;
rthread_cond_t    condvar;

int cnt = 0;

void mutex_cond_demo(void* arg) {
	cnt++;
	char *t_name = (char *) arg;
	int count = 0;

	fprintf(stdout, "\nThread \"%s\" has been born!\n\n", t_name);

	// grab the mutex lock
	rthread_mutex_lock(&mutex);			

	// spin 5 times
	while(count++ < 5) {
		fprintf(stdout, "Thread \"%s\" says hello %d!\n", t_name, count);
	
		rthread_cond_signal(&condvar);

		rthread_cond_wait(&condvar, &mutex);
		sleep(1);
	}

	// release the mutex lock
	rthread_mutex_unlock(&mutex);

	fprintf(stdout, "Thread %s is dying!\n", t_name);
	rthread_cond_signal(&condvar);
	cnt--;
}

void test_mutex(void* arg) {
	char *t_name = (char *) arg;
	fprintf(stdout, "\nThread \"%s\" has been born in tid = %d!\n", t_name, (int)syscall(SYS_gettid));
	
	sleep(2);
	rthread_mutex_lock(&mutex);
	fprintf(stdout, "\nThread \"%s\" get the lock in tid = %d!\n", t_name, (int)syscall(SYS_gettid));
	cnt++;
	rthread_yield();
	rthread_mutex_unlock(&mutex);
	fprintf(stdout, "\nThread \"%s\" release the lock in tid = %d!\n", t_name, (int)syscall(SYS_gettid));
}

int main() {

	rthread_t ut_1, ut_2;
	rthread_t k1, k2;

	rthread_init(16);
	rthread_mutex_init(&mutex);
	rthread_cond_init(&condvar);

	//rthread_create(&ut_1, USER_LEVEL, &test_mutex, "1");
	//rthread_create(&ut_2, USER_LEVEL, &test_mutex, "2");
	rthread_create(&ut_1, USER_LEVEL, &mutex_cond_demo, "1");
	rthread_create(&ut_2, USER_LEVEL, &mutex_cond_demo, "2");

	rthread_create(&k1, KERNEL_LEVEL, &k_thread_exec_func, "1");
	//rthread_create(&k2, KERNEL_LEVEL, &k_thread_exec_func, "2");

	rthread_join(ut_1, NULL);
	rthread_join(ut_2, NULL);

	rthread_cond_destory(&condvar);
	rthread_mutex_destory(&mutex);

	printf("cnt = %d\n", cnt);

	return 0;
}

