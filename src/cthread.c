#include <ucontext.h>
#include "support.h"
#include "cdata.h"

/* temporary TCB 'tree' */
static TCB_t threads[50];

TCB_t *getTCB(int tid) {
    return threads[tid];
}

int ccreate (void* (*start)(void*), void *arg) {
    static last_tid = -1;

    if (last_tid == -1) {
        TCB_t main = (TCB_t *)malloc(sizeof(TCB_t));

        main.tid = last_tid + 1;
        last_tid++;

        main.state = ST_RUNNING;

        main.ticket = main.tid;

        getcontext(&(main.context));
        main.context->uclink = NULL;

        threads[main.tid] = main;
    }
}

//int cyield(void);
//int cjoin(int tid);
int csem_init(csem_t *sem, int count) {
    FILA2 bloq;
    PFILA2 *pBloq = &bloq; // ponteiro para fila de TCB bloqueadas
    
    if(CreateFila2(pBloq) != 0) {
        puts("Failed to create pBloq");    
        return -1;
    }

    sem->count = count;
    sem->fila = pBloq;    

    return 0;
}

int cwait(csem_t *sem) { 
    if(sem->count <= 0) {
        // altera status da TCB=3
        // adiciona TCB a sem->fila
    }
    sem->count--;
}

int csignal(csem_t *sem) {
    sem->count++;
    sem->fila[0].status = 1;
    // MOVE sem->fila[0] para lista de aptos
}
//int cidentify (char *name, int size);
