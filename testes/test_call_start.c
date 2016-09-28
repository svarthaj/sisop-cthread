#include <stdlib.h>
#include "support.h"
#include "cthread.h"

void *echo(void *str) {
    puts((const char *)str);
	cyield();
	puts("hello again");
	cyield();
	puts("hello yet again");
}

int main(int argc, char** argv) {
    ccreate(echo, "hello?");
    cyield();
	puts("came back from cyield()");
	cyield();
	puts("came back from the other cyield()");
    return 0;
}
