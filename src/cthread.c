#include <ucontext.h>
#include "support.h"

#define ST_CREATION 0
#define ST_APT 1
#define ST_RUNNING 2
#define ST_BLOCKED 3
#define ST_ENDED 4

typedef struct s_TCB {
    int tid;
    int state;

    int ticket;
    ucontext_t context;
} TCB_t;

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
//int csem_init(csem_t *sem, int count);
//int cwait(csem_t *sem);
//int csignal(csem_t *sem);
//int cidentify (char *name, int size);
