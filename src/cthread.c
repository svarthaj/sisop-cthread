#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "logging.c"
#include "support.h"
#include "cdata.h"
#include "cthread.h"

#undef LOGLEVEL
#define LOGLEVEL 5

#define MAXTHREADS 50

FILA2 apts_q;
PFILA2 papts_q;
TCB_t *getTCB(int tid);

/***** utils */

/* logs a TCB */
static void printTCB(TCB_t thr) {
    const char *state[5] = {"creation", "apt", "executing", "blocked", "ended"};
    char info[1024];
    sprintf(info, "tid: %d - state: %s - ticket: %d ",
            thr.tid, state[thr.state], thr.ticket);
    loginfo((const char *)info);
}

static void printApts() {
    TCB_t *tcb_p;

    loginfo("apts queue:");
    FirstFila2(papts_q);
    while((tcb_p = (TCB_t *)GetAtIteratorFila2(papts_q)) != NULL) {
        floginfo("- %d", tcb_p->tid);
        NextFila2(papts_q);
    }
}

static void printTCB_tid(int tid) {
    TCB_t *thr = getTCB(tid);
    printTCB(*thr);
}




/***** catalogging */

/* temporary TCB 'tree' */
static TCB_t *threads[MAXTHREADS];
static char valid_threads[MAXTHREADS];
static int executing_now;

/* initializes the whole thread system. shouldn't be called more than
   once */
void initAll(void) {
    loginfo("initializing everything");
    int i;
    for (i = 0; i < MAXTHREADS; i++) valid_threads[i] = 0;

    loginfo("creating apt queue");
    papts_q = &apts_q;
    CreateFila2(papts_q);

    loginfo("setting main to executing: ");
    executing_now = 0;
}

/* return a pointer to the TCB with that tid */
TCB_t *getTCB(int tid) {
    if (tid > MAXTHREADS || !valid_threads[tid]) {
        flogerror("Tried to access invalid thread %d", tid);
    }

    return threads[tid];
}

/* put tcb in catalog */
static void keepTCB(TCB_t *tcb) {
    floginfo("keeping TCB %d", tcb->tid);
    printTCB(*tcb);

    if (valid_threads[tcb->tid] == 1) {
        floginfo("replacing TCB %d", tcb->tid);
        free(tcb->context.uc_stack.ss_sp);
        free(threads[tcb->tid]);
    }

    valid_threads[tcb->tid] = 1;
    threads[tcb->tid] = tcb;
}

static void addToApts(int tid) {
    floginfo("adding %d to apts", tid);
    AppendFila2(papts_q, getTCB(tid));
    printApts();
}




/***** TCB manipulation */

/* return initialized TCB, insert it to the TCB "catalog" */
static TCB_t *TCB_init(int tid) {
    floginfo("initializing TCB %d", tid);
    TCB_t *thr = (TCB_t *)malloc(sizeof(TCB_t));
    char *stack = (char *)malloc(1024*sizeof(char));

    thr->tid = tid;
    thr->state = PROCST_APTO;
    thr->ticket = tid;
    getcontext(&(thr->context));
    thr->context.uc_stack.ss_sp = stack;
    thr->context.uc_stack.ss_size = sizeof(stack);

    keepTCB(thr); /* insert in catalog */

    return thr;
}




/***** interface */

/* create thread */
int ccreate (void* (*start)(void*), void *arg) {
    static last_tid = -1;

    /* check for main */
    if (last_tid == -1) {
        initAll();
        floginfo("creating thread %d.", last_tid + 1);

        TCB_t *main;

        main = TCB_init(++last_tid);
        main->state = PROCST_EXEC;
        main->context.uc_link = NULL;

        floginfo("created thread %d.", main->tid);
        printTCB(*main);
    }

    floginfo("creating thread %d.", last_tid + 1);

    TCB_t *thr;

    thr = TCB_init(++last_tid);
    //makecontext(&(thr->context), start, 1, arg);
    //thr.context.uc_link = /* to pre-dispatcher */
    addToApts(thr->tid);

    floginfo("created thread %d.", thr->tid);
    printTCB(*thr);
    return 0;
}

int cyield(void) {
    return -1;
}

int cjoin(int tid) {
    return -1;
}

int csem_init(csem_t *sem, int count) {
    return -1;
}

int cwait(csem_t *sem) {
    return -1;
}

int csignal(csem_t *sem) {
    return -1;
}

int cidentify (char *name, int size) {
    return -1;
}
