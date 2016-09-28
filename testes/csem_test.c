#include <stdio.h>
#include "support.h"
#include "cthread.h"

int main(int argc, char** argv) {
    csem_t sem1;

    csem_init(&sem1, 3);

    cwait(&sem1);
    cwait(&sem1);
    cwait(&sem1);

    csignal(&sem1);

    return 0;

}
