#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "support.h"
#include "cdata.h"
#include "cthread.h"

#define VERBOSE 1

/***** catalogging */

/* temporary TCB 'tree' */
static TCB_t threads[50];

TCB_t getTCB(int tid) {
    return threads[tid];
}

/* put 'tcb' in catalog */
static void keepTCB(TCB_t tcb) {
    threads[tcb.tid] = tcb;
}

/***** TCB manipulation and utils */

/* return initialized TCB, insert it to the TCB "catalog" */
static TCB_t TCB_init(int tid) {
    TCB_t thr;
    char *stack = malloc(1024*sizeof(char));

    thr.tid = tid;
    thr.state = PROCST_APTO;
    thr.ticket = tid;
    getcontext(&(thr.context));
    thr.context.uc_stack.ss_sp = stack;
    thr.context.uc_stack.ss_size = sizeof(stack);

    keepTCB(thr); /* insert in catalog */

    return thr;
}

static void printTCB(int tid) {
    if (VERBOSE) {
        TCB_t thr = getTCB(tid);
        const char *state[5] = {"creation", "apt", "executing", "blocked", "ended"};
        printf("tid: %d\n", thr.tid);
        printf("state: %s\n", state[thr.state]);
        printf("ticket: %d\n", thr.ticket);
    }
}

/***** interface */

int ccreate (void* (*start)(void*), void *arg) {
    static last_tid = -1;

    /* check for main */
    if (last_tid == -1) {
        TCB_t main;

        main = TCB_init(++last_tid);
        main.context.uc_link = NULL;

        printTCB(main.tid);
    }

    TCB_t thr;

    thr = TCB_init(++last_tid);
    //thr.context.uc_link = /* to dispatcher */
    printTCB(thr.tid);

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
