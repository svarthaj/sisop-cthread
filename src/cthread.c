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

ucontext_t m;
int been_here;

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

    loginfo("setting maint to executing: ");
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

/* find and return the address of the TCB with tid closest to the one given.
   if two tids are equaly close, returns the smallest of them. should never
   return NULL, for at least the maint thread exist. */
static TCB_t *getClosestTCB(int tid) {
    int r = 0; /* radius */
    int i = tid;
    TCB_t *tcb = NULL;

    while (tcb == NULL) {
        int minor, major;
        /* watch for limits */
        minor = (i - r < 0) ? 0 : i - r;
        major = (i + r > MAXTHREADS) ? MAXTHREADS : i + r;

        /* smaller has preference */
        if (valid_threads[minor]) {
            tcb = getTCB(minor);
        } else if (valid_threads[major]) {
            tcb = getTCB(major);
        }

        r++;
    }

    return tcb;
}

static void addToApts(int tid) {
    floginfo("adding %d to apts", tid);
    AppendFila2(papts_q, getTCB(tid));
    printApts();
}

static void removeFromApts(int tid) {
    floginfo("removing %d from apts", tid);
    FirstFila2(papts_q);

    TCB_t *curr; /* current */
    while ((curr = (TCB_t *)GetAtIteratorFila2(papts_q)) != NULL) {
        if (curr->tid == tid) {
            DeleteAtIteratorFila2(papts_q);
            break;
        } else {
            NextFila2(papts_q);
        }
    }
}




/***** TCB manipulation */

/* return initialized TCB, insert it to the TCB "catalog" */
static TCB_t *TCB_init(int tid) {
    floginfo("initializing TCB %d", tid);
    TCB_t *thr = (TCB_t *)malloc(sizeof(TCB_t));
    int ss_size = 1234*sizeof(char);
    char *stack = (char *)malloc(ss_size);

    thr->tid = tid;
    thr->state = PROCST_APTO;
    thr->ticket = tid;
    flogdebug("initializing context at address %p", &(thr->context));
    if (getcontext(&(thr->context)) == -1) logerror("error initializing context");
    flogdebug("stack pointer is %p and stack size is %d", stack, ss_size);
	thr->context.uc_link = &m;
    thr->context.uc_stack.ss_sp = stack;
    thr->context.uc_stack.ss_size = ss_size;

    keepTCB(thr); /* insert in catalog */

    return thr;
}

static void run_thread(void *(*start)(void *), void *arg) {
    loginfo("in call start");
    flogdebug("thread %d is executing", executing_now);
    start(arg);
}

static void dispatcher(void) {
    int lucky = Random2()%MAXTHREADS;
    floginfo("closest to %d will be selected for dispatch", lucky);
    TCB_t *tcb = getClosestTCB(lucky);
    removeFromApts(tcb->tid);
    executing_now = tcb->tid;
    floginfo("dispatching %d", tcb->tid);
    flogdebug("setting context at address %p", &(tcb->context));
    setcontext(&(tcb->context));
}

static void say_hey(void) {
	been_here = 1;
    loginfo("hey");
}




/***** interface */

/* create thread */
int ccreate (void* (*start)(void*), void *arg) {
    static int last_tid = -1;

    /* check for maint */
    if (last_tid == -1) {
        initAll();
        floginfo("creating thread %d.", last_tid + 1);

        TCB_t *maint;

        maint = TCB_init(++last_tid);
        maint->state = PROCST_EXEC;
        maint->context.uc_link = NULL;

        floginfo("created thread %d.", maint->tid);
        printTCB(*maint);
    }

    floginfo("creating thread %d.", last_tid + 1);

//	char cstack[1024];
	char *cstack = (char *)malloc(1024);
    TCB_t *thr;
    thr = TCB_init(++last_tid);
	thr->context.uc_stack.ss_sp = cstack;
//	thr->context.uc_stack.ss_sp = malloc(1024);
	thr->context.uc_stack.ss_size = 1024;

//	ucontext_t c;
//	c.uc_link = &m;
//	c.uc_stack.ss_sp = cstack;
//	c.uc_stack.ss_size = sizeof(cstack);

	been_here = 0;
	getcontext(&m);

//	if (!been_here) {
//		logdebug("getting test context");
//		getcontext(&c);
//		logdebug("making test context");
//		makecontext(&c, say_hey, 0);
//		logdebug("setting test context");
//		setcontext(&c);
//	}
//	logdebug("came back from setcontext");

	if (!been_here) {
		//makecontext(&(thr->context), (void (*)(void))run_thread, 2, start, arg);
		logdebug("**prematurely setting context just for testing the segfault");
		logdebug("this shall be removed and left to the dispatcher later**");
		flogdebug("making context at address %p", &(thr->context));
		logdebug("calling: makecontext(&(thr->context), say_hey, 0);");
		makecontext(&(thr->context), say_hey, 0);
		flogdebug("setting context at address %p", &(thr->context));
		logdebug("calling: setcontext(&(thr->context))");
		setcontext(&(thr->context));
	}
	logdebug("came back from setcontext");

	exit(0);

    //thr->context.uc_link = NULL; /* to pre-dispatcher */
    addToApts(thr->tid);

    floginfo("created thread %d.", thr->tid);
    printTCB(*thr);
    return 0;
}

int cyield(void) {
    int been_here = 0;
    TCB_t *caller;
    caller = getTCB(executing_now);
    floginfo("thread %d is yielding control", caller->tid);
    getcontext(&(caller->context));

    if (!been_here) {
        been_here = 1;
        caller->state = PROCST_APTO;
        addToApts(caller->tid);

        dispatcher();
    }
    return 0;
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
