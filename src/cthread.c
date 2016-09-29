#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "logging.c"
#include "support.h"
#include "cdata.h"
#include "cthread.h"

#undef LOGLEVEL
#define LOGLEVEL 4

#define MAXTHREADS 50
#define MAXTICKET 255
#define THR_STACKSZ 50*1024

FILA2 apts_q;
PFILA2 papts_q;
TCB_t *getTCB(int tid);

static void terminated(void);

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
		printTCB(*tcb_p);
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
static ucontext_t terminated_context;
static char terminated_stack[THR_STACKSZ];

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

	loginfo("making terminated context");
	getcontext(&terminated_context);
	terminated_context.uc_stack.ss_sp = terminated_stack;
	terminated_context.uc_stack.ss_size = THR_STACKSZ;
	makecontext(&terminated_context, terminated, 0);
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

static void removeTCB(TCB_t *tcb) {
	floginfo("removing TCB %d", tcb->tid);

	if (valid_threads[tcb->tid] == 0) {
		flogdebug("TCB %d is invalid");
	} else {
		valid_threads[tcb->tid] = 0;
		flogdebug("freeing TCB %d stack at %p", tcb->tid, tcb->context.uc_stack.ss_sp);
		free(tcb->context.uc_stack.ss_sp);
		free(threads[tcb->tid]);
	}
}

static TCB_t *getNextTCB() {
	FirstFila2(papts_q);
	return (TCB_t *)GetAtIteratorFila2(papts_q);
}

/* find and return the address of the TCB with ticket closest to the one given.
   if two tickets are equaly close, return the TCB with the smallest tid.
   should never return NULL, for at least the main thread exist. */
static TCB_t *getClosestTCB(int ticket) {
	floginfo("selecting thread by ticket. lucky number: %d", ticket);

	int closest = MAXTICKET + 1;
	TCB_t *chosen = NULL;
	TCB_t *it;
	FirstFila2(papts_q);
	while ((it = (TCB_t *)GetAtIteratorFila2(papts_q)) != NULL) {
		int curr_dst = abs(it->ticket - ticket);
		if (curr_dst < closest) {
			flogdebug("closest is now ticket %d", it->ticket);
			chosen = it;
			closest = curr_dst;
		} else if (curr_dst == closest) {
			chosen = (chosen->tid < it->tid) ? chosen : it;
		}

		NextFila2(papts_q);
    }

	floginfo("closest was thread %d with ticket %d", chosen->tid, chosen->ticket);

    return chosen;
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
    int ss_size = THR_STACKSZ;
    char *stack = (char *)malloc(ss_size);

    thr->tid = tid;
    thr->state = PROCST_APTO;
    thr->ticket = Random2()%MAXTICKET;
    flogdebug("initializing context at address %p", &(thr->context));
    if (getcontext(&(thr->context)) == -1) logerror("error initializing context");
    flogdebug("stack pointer is %p and stack size is %d", stack, ss_size);
    thr->context.uc_stack.ss_sp = stack;
    thr->context.uc_stack.ss_size = ss_size;

    keepTCB(thr); /* insert in catalog */

    return thr;
}

static void run_thread(void *(*start)(void *), void *arg) {
    loginfo("in call start");
    flogdebug("thread %d is executing", executing_now);
    start(arg);
	flogdebug("thread %d finished executing");
	terminated();
}

static void dispatcher(void) {
	int lucky = Random2()%MAXTICKET;
    //TCB_t *tcb = getClosestTCB(lucky);
	TCB_t *tcb = getNextTCB();
    removeFromApts(tcb->tid);
    executing_now = tcb->tid;
    floginfo("dispatching %d", tcb->tid);
    flogdebug("setting context at address %p", &(tcb->context));
    setcontext(&(tcb->context));
}

static void terminated(void) {
	floginfo("thread %d terminated", executing_now);
	removeTCB(getTCB(executing_now));
	dispatcher();
}




/***** interface */

/* create thread */
int ccreate (void* (*start)(void*), void *arg) {
    static int last_tid = -1;

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
    makecontext(&(thr->context), (void (*)(void))run_thread, 2, start, arg);
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
    FILA2 bloq;
    PFILA2 pBloq = &bloq; // ponteiro para fila de TCB bloqueadas

    if(CreateFila2(pBloq) != 0) {
        ("Failed to create pBloq");
        return -1;
    }

    sem->count = count;
    sem->fila = pBloq;

    return 0;
}

int cwait(csem_t *sem) {
    if(sem->count <= 0) {
        // como acessar a TCB que chamou o cwait?
        // altera status da TCB=3
        // adiciona TCB a sem->fila
    }
    sem->count--;
}

int csignal(csem_t *sem) {
    sem->count++;

    // acessa o primeiro da fila de bloqueados
    FirstFila2( sem->fila );
    TCB_t  *unblocked_tcb;

    if( unblocked_tcb = (TCB_t*)GetAtIteratorFila2( sem->fila )){
        DeleteAtIteratorFila2( sem->fila ); // remove primeiro bloqueado
        unblocked_tcb->state = 1; // muda status para apto
        AppendFila2( papts_q, unblocked_tcb ); // acrescenta em aptos

        return 0;
    }

    return -1;
}
