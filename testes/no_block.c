#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#include "cthread.h"

void *echo(void *str) {
    printf("entered %s\n", (const char *)str);
	cyield();
    printf("exiting %s\n", (const char *)str);

	return NULL;
}

int main(int argc, char** argv) {
    ccreate(echo, "1");
    cyield();
	puts("came back from first cyield()");
	cyield();
	puts("came back from second cyeld()");
	cyield();
	puts("came back from third cyeld()");
    return 0;
}
