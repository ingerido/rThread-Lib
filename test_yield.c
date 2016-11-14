#include <time.h>
#include <pthread.h>
#include "rthread.h"

#include <sched.h>

int a[0xFFFF][4];

void test_yield(void* arg) {
	char *t_name = (char *) arg;
	fprintf(stdout, "User Level Thread \"%s\" start !\n", t_name);	
	int i = 0;
	for (i = 0; i < 0xFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d \n", t_name, (int)syscall(SYS_gettid));
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	rthread_yield();
	sleep(1);
	fprintf(stdout, "User Level Thread \"%s\" resume !\n", t_name);	
	for (i = 0; i < 0xFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d \n", t_name, (int)syscall(SYS_gettid));
	}
	fprintf(stdout, "User Level Thread \"%s\" finish !\n", t_name);	
}

void test_roundrobin(void* arg) {
	char *t_name = (char *) arg;
	fprintf(stdout, "User Level Thread \"%s\" start !\n", t_name);	
	int i = 0, j = 0, k = 0;
	for (i = 0; i < 0x7FFF; i++) {
		/* Just do some thing to make it runs longer */
		while (j < 0xFFFFFFF) {
			j += 2;
			j -= 1;
			if (j % 0xFFFFFF == 0)
				fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d !\n", t_name, (int)syscall(SYS_gettid));
		}
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	sleep(1);
	//rthread_exit(&i);
	fprintf(stdout, "User Level Thread \"%s\" resume !\n", t_name);	
	for (i = 0; i < 0xFFFF; i++) {
		/* Just do some thing to make it runs longer */
		while (k < 0xFFFFFFF) {
			k += 2;
			k -= 1;
			if (k % 0xFFFFFF == 0)
				fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d !\n", t_name, (int)syscall(SYS_gettid));
		}
	}		
	fprintf(stdout, "User Level Thread \"%s\" finish !\n", t_name);
	rthread_exit(&i);
}

void parallel_calculate(void* arg) {
	char *t_name = (char *) arg;
	int row = atoi(t_name) - 1;
	int sum = 0;
	int i = 0, j = 0, k = 0;
	fprintf(stdout, "User Level Thread \"%s\" start in tid = %d !\n", t_name, (int)syscall(SYS_gettid));
	while (i < 0x7FFF) {
		sum += a[i][row];
		i++;
		/* Just do some thing to make it runs longer */
		while (j < 0xFFFFFFF) {
			j += 2;
			j -= 1;
			if (j % 0xFFFFFF == 0)
				fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d !\n", t_name, (int)syscall(SYS_gettid));
		}
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	sleep(1);
	fprintf(stdout, "User Level Thread \"%s\" resume in tid = %d !\n", t_name, (int)syscall(SYS_gettid));	
	while (i < 0xFFFF) {
		sum += a[i][row];
		i++;
		/* Just do some thing to make it runs longer */
		while (k < 0xFFFFFFF) {
			k += 2;
			k -= 1;
			if (k % 0xFFFFFF == 0)
				fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d !\n", t_name, (int)syscall(SYS_gettid));
		}
	}
	fprintf(stdout, "User Level Thread \"%s\" finish ! sum[%d] = %d\n", t_name, row, sum);
}

int main() {
	rthread_t u1, u2, u3, u4;
	rthread_t k1, k2, k3, k4;

	int *tmp;

	for (int j = 0; j < 0xFFFF; j++)
		for (int i = 0; i < 4; i++)
			a[j][i] = i;

	rthread_init(16);

	rthread_create(&u1, USER_LEVEL, &parallel_calculate, "1");
	rthread_create(&u2, USER_LEVEL, &parallel_calculate, "2");
	rthread_create(&u3, USER_LEVEL, &parallel_calculate, "3");
	rthread_create(&u4, USER_LEVEL, &parallel_calculate, "4");

	fprintf(stdout, "Main Thread Starts !\n");

	rthread_create(&k1, KERNEL_LEVEL, &k_thread_exec_func, "1");
	rthread_create(&k2, KERNEL_LEVEL, &k_thread_exec_func, "2");	
	//rthread_create(&k3, KERNEL_LEVEL, &k_thread_exec_func, "3");
	//rthread_create(&k4, KERNEL_LEVEL, &k_thread_exec_func, "4");

	//rthread_schedule();

	//rthread_join(u1, &tmp);
	rthread_join(u1, NULL);
	rthread_join(u2, NULL);
	rthread_join(u3, NULL);
	rthread_join(u4, NULL);

	fprintf(stdout, "Main Thread Ends !\n");

	return 0;
}

