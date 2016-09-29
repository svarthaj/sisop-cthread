#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#include "cthread.h"

csem_t sem;

void *echo(void *str) {
	cwait(&sem);
	printf("%s: now I can run\n", (const char *)str);

	return NULL;
}

int main(int argc, char** argv) {
    ccreate(echo, "1");
	csem_init(&sem, 1);
	cwait(&sem);
    cyield();
	puts("came back from first cyield()");
	cyield();
	puts("came back from second cyeld()");
	csignal(&sem);
	cyield();
	puts("came back from third cyeld()");
    return 0;
}
