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
	rthread_cond_broadcast(&condvar);	
	//rthread_cond_signal(&condvar);
	cnt--;
}


int main() {

	rthread_t tid;

	rthread_init(NULL, NULL);
	rthread_mutex_init(&mutex, NULL);
	rthread_cond_init(&condvar, NULL);

	rthread_create(&tid, USER_LEVEL, &mutex_cond_demo, "1");
	rthread_create(&tid, USER_LEVEL, &mutex_cond_demo, "2");

	rthread_create(&tid, KERNEL_LEVEL, &k_thread_exec_func, "1");

   while(1) {
      sleep(2);
      if (cnt == 0)
         break;
    }

	return 0;
}

