#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#include "cthread.h"

void *echo(void *str) {
    puts((const char *)str);
	return NULL;
}

int main(int argc, char** argv) {
    ccreate(echo, "hello?");
    cyield();
    return 0;
}
