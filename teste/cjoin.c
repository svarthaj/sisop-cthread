#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#include "cthread.h"

void *thr1(void *arg) {
	int i;
	if (cjoin(10) < 0) {
		puts("1 won't wait for 10");
	}
	for (i = 0; i < 2; i++) {
		printf("counter in 1: i = %d\n", 1 - i);
		cyield();
	}
	puts("1 is finished");

	return NULL;
}

void *thr2(void *arg) {
	int i;
	if (cjoin(1) < 0) {
		puts("2 won't wait for 1");
	}
	for (i = 0; i < 4; i++) {
		printf("counter in 2: i = %d\n", 3 - i);
		cyield();
	}
	puts("2 is finished");

	return NULL;
}

void *thr3(void *wait_for) {
	puts("3 will wait for 2");
	cjoin(*(int *)wait_for);
	puts("3 will run now");

	puts("3 is finished");
	return NULL;
}

int main(int argc, char** argv) {
    int tid1 = ccreate(thr1, NULL);
    int tid2 = ccreate(thr2, NULL);
    int tid3 = ccreate(thr3, &tid2);
	cjoin(tid1);
	cjoin(tid3);
	puts("came back from cjoin()'s");
    return 0;
}
