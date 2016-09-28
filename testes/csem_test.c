#include <stdio.h>
#include "support.h"
#include "cthread.h"

int main(int argc, char** argv) {
    char info[1024];
    csem_t sem1;
    csem_t sem2;

    csem_init(&sem1, 3);
    csem_init(&sem2, 10);

    cwait(&sem1);
    sprintf(info, "sem count: %d - sem fila at: %p", sem1.count, sem1.fila);
    loginfo(info);
   
    cwait(&sem2);
    sprintf(info, "sem count: %d - sem fila at: %p", sem2.count, sem2.fila);
    loginfo(info);

    cwait(&sem1);
    sprintf(info, "sem count: %d", sem1.count);
    loginfo(info);
    
    cwait(&sem1);
    sprintf(info, "sem count: %d", sem1.count);
    loginfo(info);
    
    cwait(&sem1);
    sprintf(info, "sem count: %d", sem1.count);
    loginfo(info);
    
    csignal(&sem1);

    return 0;

}
