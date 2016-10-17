#include <time.h>
#include <pthread.h>
#include "rthread.h"

#include <sched.h>

int a[0xFFFF][4];

void rth(void* uc, void* arg) {
	char *t_name = (char *) arg;
	fprintf(stdout, "User Level Thread \"%s\" start !\n", t_name);	
	int i = 0;
	for (i = 0; i < 0xFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d \n", t_name, (int)syscall(SYS_gettid));
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	rthread_yield(uc);
	sleep(1);
	fprintf(stdout, "User Level Thread \"%s\" resume !\n", t_name);	
	for (i = 0; i < 0xFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d \n", t_name, (int)syscall(SYS_gettid));
	}
	fprintf(stdout, "User Level Thread \"%s\" finish !\n", t_name);	
}

void tst(void* uc, void* arg) {
	char *t_name = (char *) arg;
	fprintf(stdout, "User Level Thread \"%s\" start !\n", t_name);	
	int i = 0;
	for (i = 0; i < 0xFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d \n", t_name, (int)syscall(SYS_gettid));
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	sleep(1);
	//rthread_exit(uc, &i);
	fprintf(stdout, "User Level Thread \"%s\" resume !\n", t_name);	
	for (i = 0; i < 0xFFFF; i++) {
		if (i == 0)
			fprintf(stdout, "User Level Thread \"%s\" is running in tid = %d \n", t_name, (int)syscall(SYS_gettid));
	}		
	fprintf(stdout, "User Level Thread \"%s\" finish !\n", t_name);
	//rthread_exit(uc, &i);
}

void cal(void* uc, void* arg) {
	char *t_name = (char *) arg;
	int row = atoi(t_name) - 1;
	int sum = 0;
	int i = 0, j = 0, k = 0;
	fprintf(stdout, "User Level Thread \"%s\" start in tid = %d !\n", t_name, (int)syscall(SYS_gettid));
	while (i < 0x7FFF) {
		sum += a[i][row];
		i++;
		//sleep(0.1);
		/* Just do some thing to make it runs longer */
		while (j < 0xFFFFFFF) {
			j += 2;
			j -= 1;
		}
	}
	fprintf(stdout, "User Level Thread \"%s\" pause !\n", t_name);
	//sleep(1);
	fprintf(stdout, "User Level Thread \"%s\" resume in tid = %d !\n", t_name, (int)syscall(SYS_gettid));	
	while (i < 0xFFFF) {
		sum += a[i][row];
		i++;
		//sleep(0.1);
		/* Just do some thing to make it runs longer */
		while (k < 0xFFFFFFF) {
			k += 2;
			k -= 1;
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

	rthread_create(&u1, USER_LEVEL, &cal, "1");
	rthread_create(&u2, USER_LEVEL, &cal, "2");
	rthread_create(&u3, USER_LEVEL, &cal, "3");
	rthread_create(&u4, USER_LEVEL, &cal, "4");

	//rthread_create(&k1, KERNEL_LEVEL, &k_thread_exec_func, "1");
	//rthread_create(&k2, KERNEL_LEVEL, &k_thread_exec_func, "2");

	//pthread_create(&k1, NULL, &k_thread_exec_func, "1");
	//pthread_create(&k2, NULL, &k_thread_exec_func, "2");

	printf("k1 = %d\n", k1);
	printf("k2 = %d\n", k2);	

	//rthread_create(&k1, KERNEL_LEVEL, &tst, "1");
	//rthread_create(&k2, KERNEL_LEVEL, &tst, "2");

	//pthread_create(&k1, NULL, &tst, "1");
	//pthread_create(&k2, NULL, &tst, "2");

	printf("Main Thread Starts !\n");

	rthread_schedule();

	//sleep(5);
	//rthread_join(u1, &tmp);
	rthread_join(u1, NULL);
	rthread_join(u2, NULL);
	rthread_join(u3, NULL);
	rthread_join(u4, NULL);

	//rthread_create(&tid, USER_LEVEL, &rth, "5");
	//rthread_create(&tid, USER_LEVEL, &rth, "6");
	//rthread_create(&tid, USER_LEVEL, &rth, "7");
	//rthread_create(&tid, USER_LEVEL, &rth, "8");

	//sleep(5);

	printf("k1 = %d\n", k1);
	printf("k2 = %d\n", k2);
	printf("Main Thread Ends !\n");

	return 0;
}

