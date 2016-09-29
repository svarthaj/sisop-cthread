#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#include "cthread.h"

#define RUNS 2

void *echo(void *str) {
	int i;
	for (i = 0; i < RUNS; i++) {
		printf("inside echo(), %s\n", (const char *)str);
		printf("i = %d\n", i);
		cyield();
	}

	printf("%s ended\n", (const char *)str);

	return NULL;
}

int main(int argc, char** argv) {
    ccreate(echo, "1");
	ccreate(echo, "2");
	ccreate(echo, "3");
	ccreate(echo, "4");

	int i;
	for (i = 0; i < 10*RUNS; i++) {
		puts("inside main()");
		printf("i = %d\n", i);
		cyield();
	}
	puts("end of main()");

	return 0;
}
