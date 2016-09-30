#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <string.h>
#include "logging.c"
#include "support.h"
#include "cdata.h"
#include "cthread.h"

int cidentify(char *name, int size)
{
	strncpy(name, "Lucas Leal (206438)\nLuis Mollmann (206440)", size);
	return 0;
}

#define THR_ERROR -1
#define THR_SUCCESS 0
#define THR_MAXTICKET 255
#define THR_STACKSZ 50*1024

struct join_pair {
	TCB_t *waiting;
	TCB_t *waited;
};

FILA2 apts_q;
PFILA2 papts_q;

FILA2 join_q;
PFILA2 pjoin_q;

FILA2 threads_q;
PFILA2 pthreads_q;

FILA2 valid_thr_q;
PFILA2 pvalid_thr_q;

TCB_t *getTCB(int tid);

static void terminate(void);

static int executing_now;
static ucontext_t terminate_context;
static char terminate_stack[THR_STACKSZ];

/***** utils */

/* logs a TCB */
static void printTCB(TCB_t thr)
{
	const char *state[5] =
	    { "creation", "apt", "executing", "blocked", "ended" };
	char info[1024];
	sprintf(info, "tid: %d - state: %s - ticket: %d ",
		thr.tid, state[thr.state], thr.ticket);
	loginfo((const char *)info);
}

static void printApts()
{
	TCB_t *tcb_p;

	loginfo("apts queue:");
	FirstFila2(papts_q);
	while ((tcb_p = (TCB_t *) GetAtIteratorFila2(papts_q)) != NULL) {
		printTCB(*tcb_p);
		NextFila2(papts_q);
	}
}

static void printJoin()
{
	struct join_pair *pair;

	loginfo("join queue:");
	FirstFila2(pjoin_q);
	while ((pair = (struct join_pair *)GetAtIteratorFila2(pjoin_q)) != NULL) {
		floginfo("%d, who is waiting for %d",
			 pair->waiting->tid, pair->waited->tid);
		NextFila2(pjoin_q);
	}
}

static int error(const char *msg)
{
	logerror(msg);
	logerror("system state:");
	logerror("excuting now is:");
	printTCB(*getTCB(executing_now));
	printApts();
	printJoin();

	return THR_ERROR;
}

/***** catalogging */

/* temporary TCB 'tree' */

static int initFila(PFILA2 fila)
{
	if (CreateFila2(fila) != 0) {
		return error("initFila() failed");
	} else {
		return THR_SUCCESS;
	}
}

static int initContext(ucontext_t * context, char *stack, int stack_size)
{
	if (getcontext(context) == -1) {
		return error("failed in initContext()");
	}

	context->uc_stack.ss_sp = stack;
	context->uc_stack.ss_size = stack_size;
	return THR_SUCCESS;
}

/* initializes the whole thread system. shouldn't be called more than
   once */
static int initAll(void)
{
	logdebug("initializing everything");

	logdebug("creating apt queue");
	papts_q = &apts_q;
	if (initFila(papts_q) == THR_ERROR)
		return THR_ERROR;

	logdebug("creating join queue");
	pjoin_q = &join_q;
	if (initFila(pjoin_q) == THR_ERROR)
		return THR_ERROR;

	logdebug("creating threads queue");
	pthreads_q = &threads_q;
	if (initFila(pthreads_q) == THR_ERROR)
		return THR_ERROR;

	logdebug("creating valid_threads queue");
	pvalid_thr_q = &valid_thr_q;
	if (initFila(pvalid_thr_q) == THR_ERROR)
		return THR_ERROR;

	logdebug("setting main to executing: ");
	executing_now = 0;

	logdebug("making terminate context");
	if (initContext(&terminate_context, terminate_stack, THR_STACKSZ) ==
	    THR_ERROR)
		return THR_ERROR;
	makecontext(&terminate_context, terminate, 0);

	return THR_SUCCESS;
}

/* add tcb to threads queue */
static void addToThreads(TCB_t * thr)
{
	flogdebug("adding %d to threads", thr->tid);
	AppendFila2(pthreads_q, thr);
}

/* remove tcb from threads queue */
static void removeFromThreads(int tid)
{
	flogdebug("removing %d from threads", tid);
	FirstFila2(pthreads_q);

	TCB_t *curr;		/* current */
	while ((curr = (TCB_t *) GetAtIteratorFila2(pthreads_q)) != NULL) {
		if (curr->tid == tid) {
			DeleteAtIteratorFila2(pthreads_q);
			break;
		} else {
			NextFila2(pthreads_q);
		}
	}
}

/* search for tid in threads queue */
static int is_valid_thread(int tid)
{
	TCB_t *it;
	FirstFila2(pthreads_q);
	while ((it = (TCB_t *) GetAtIteratorFila2(pthreads_q)) != NULL) {
		if (it->tid == tid) {
			return THR_SUCCESS;
		} else {
			NextFila2(pthreads_q);
		}
	}
	return THR_ERROR;
}

/* return a pointer to the TCB with that tid */
TCB_t *getTCB(int tid)
{
	TCB_t *it;
	FirstFila2(pthreads_q);
	while ((it = (TCB_t *) GetAtIteratorFila2(pthreads_q)) != NULL) {
		if (it->tid == tid) {
			return it;
		} else {
			NextFila2(pthreads_q);
		}
	}

	flogerror("Tried to access invalid thread %d", tid);

	return NULL;
}

/* put tcb in catalog */
static void keepTCB(TCB_t * tcb)
{
	flogdebug("keeping TCB %d", tcb->tid);

	if (is_valid_thread(tcb->tid) == THR_SUCCESS) {
		flogwarning("replacing TCB %d", tcb->tid);
		free(tcb->context.uc_stack.ss_sp);
	}

	addToThreads(tcb);
}

static void removeTCB(TCB_t * tcb)
{
	flogdebug("removing TCB %d", tcb->tid);

	if (is_valid_thread(tcb->tid) == THR_ERROR) {
		flogwarning("TCB %d is invalid");
	} else {
		flogdebug("freeing TCB %d stack at %p", tcb->tid,
			  tcb->context.uc_stack.ss_sp);
		free(tcb->context.uc_stack.ss_sp);
		removeFromThreads(tcb->tid);
	}
}

/* find and return the address of the TCB with ticket closest to the one given.
   if two tickets are equaly close, return the TCB with the smallest tid.
   should never return NULL, for at least the main thread exist. */
static TCB_t *getClosestTCB(int ticket)
{
	flogdebug("selecting thread by ticket. lucky number: %d", ticket);

	int closest = THR_MAXTICKET + 1;
	TCB_t *chosen = NULL;
	TCB_t *it;
	FirstFila2(papts_q);
	while ((it = (TCB_t *) GetAtIteratorFila2(papts_q)) != NULL) {
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

	flogdebug("closest was thread %d with ticket %d", chosen->tid,
		  chosen->ticket);

	return chosen;
}

static void addToApts(int tid)
{
	flogdebug("adding %d to apts", tid);
	TCB_t *thr = getTCB(tid);
	AppendFila2(papts_q, thr);
}

static int addToJoin(int waiting, int waited)
{
	flogdebug("adding (%d, %d) to join queue", waiting, waited);
	struct join_pair *pair =
	    (struct join_pair *)malloc(sizeof(struct join_pair));

	if (pair == NULL) {
		return error("malloc on addToJoin()");
	}

	pair->waiting = getTCB(waiting);
	pair->waiting->state = PROCST_BLOQ;
	pair->waited = getTCB(waited);
	AppendFila2(pjoin_q, pair);

	return THR_SUCCESS;
}

static void removeFromApts(int tid)
{
	flogdebug("removing %d from apts", tid);
	FirstFila2(papts_q);

	TCB_t *curr;		/* current */
	while ((curr = (TCB_t *) GetAtIteratorFila2(papts_q)) != NULL) {
		if (curr->tid == tid) {
			DeleteAtIteratorFila2(papts_q);
			break;
		} else {
			NextFila2(papts_q);
		}
	}
}

static void removeFromJoin(int tid)
{
	flogdebug("removing %d from join queue", tid);
	FirstFila2(pjoin_q);

	struct join_pair *pair;	/* current */
	while ((pair = (struct join_pair *)GetAtIteratorFila2(pjoin_q)) != NULL) {
		if (pair->waiting->tid == tid) {
			DeleteAtIteratorFila2(pjoin_q);
			free(pair);
			break;
		} else {
			NextFila2(pjoin_q);
		}
	}
}

static int findWaitingFor(int tid)
{
	FirstFila2(pjoin_q);

	struct join_pair *pair;	/* current */
	while ((pair = (struct join_pair *)GetAtIteratorFila2(pjoin_q)) != NULL) {
		if (pair->waited->tid == tid) {
			return pair->waiting->tid;
		} else {
			NextFila2(pjoin_q);
		}
	}

	return -1;
}

/***** TCB manipulation */

/* return initialized TCB, insert it to the TCB "catalog" */
static TCB_t *TCB_init(int tid)
{
	flogdebug("initializing TCB %d", tid);
	TCB_t *thr = (TCB_t *) malloc(sizeof(TCB_t));
	int ss_size = THR_STACKSZ;
	char *stack = (char *)malloc(ss_size);

	if (thr == NULL || stack == NULL) {
		error("malloc error on TCB_init()");
		return NULL;
	}

	thr->tid = tid;
	thr->state = PROCST_APTO;
	thr->ticket = Random2() % THR_MAXTICKET;
	flogdebug("initializing context at address %p", &(thr->context));
	if (getcontext(&(thr->context)) == -1)
		logerror("error initializing context");
	flogdebug("stack pointer is %p and stack size is %d", stack, ss_size);
	if (initContext(&(thr->context), stack, ss_size) == THR_ERROR)
		return NULL;

	keepTCB(thr);		/* insert in catalog */

	return thr;
}

static void run_thread(void *(*start) (void *), void *arg)
{
	logdebug("in call start");
	flogdebug("thread %d is executing", executing_now);
	start(arg);
	flogdebug("thread %d finished executing");
}

static void dispatcher(void)
{
	loginfo("in dispatcher");
	printApts();
	printJoin();

	int lucky = Random2() % THR_MAXTICKET;
	TCB_t *tcb = getClosestTCB(lucky);
	removeFromApts(tcb->tid);
	executing_now = tcb->tid;
	floginfo("dispatching %d", tcb->tid);
	flogdebug("setting context at address %p", &(tcb->context));
	setcontext(&(tcb->context));
}

static void releasePossibleWaiting(int tid)
{
	int waiting;
	if ((waiting = findWaitingFor(tid)) >= 0) {
		removeFromJoin(waiting);
		addToApts(waiting);
		floginfo("%d finished waiting for %d", waiting, tid);
	}
}

static void terminate(void)
{
	floginfo("thread %d terminated", executing_now);
	releasePossibleWaiting(executing_now);
	removeTCB(getTCB(executing_now));
	dispatcher();
}

/***** interface */

/* create thread */
int ccreate(void *(*start) (void *), void *arg)
{
	static int last_tid = -1;

	/* check for main */
	if (last_tid == -1) {
		initAll();
		floginfo("creating thread %d.", last_tid + 1);

		TCB_t *main_t;

		main_t = TCB_init(++last_tid);
		main_t->state = PROCST_EXEC;
		main_t->context.uc_link = NULL;

		floginfo("created thread %d.", main_t->tid);
		printTCB(*main_t);
	}

	floginfo("creating thread %d.", last_tid + 1);

	TCB_t *thr;

	thr = TCB_init(++last_tid);
	makecontext(&(thr->context), (void (*)(void))run_thread, 2, start, arg);
	addToApts(thr->tid);

	floginfo("created thread %d.", thr->tid);
	printTCB(*thr);

	return thr->tid;
}

int cyield(void)
{
	int been_here = 0;
	TCB_t *caller;
	caller = getTCB(executing_now);
	floginfo("thread %d is yielding control", caller->tid);
	if (getcontext(&(caller->context)) == -1) {
		return error("error getting context in cyield()");
	}

	if (!been_here) {
		been_here = 1;
		caller->state = PROCST_APTO;
		addToApts(caller->tid);
		dispatcher();
	}

	return THR_SUCCESS;
}

int cjoin(int tid)
{
	int been_here = 0;

	/* check if someone else is not waiting */
	if (findWaitingFor(tid) >= 0 || is_valid_thread(tid)) {
		return THR_ERROR;
	}

	TCB_t *waiting;
	waiting = getTCB(executing_now);
	floginfo("thread %d will wait for thread %d", waiting->tid, tid);
	if (getcontext(&(waiting->context)) == -1) {
		return error("error getting context in cjoin()");
	}

	if (!been_here) {
		been_here = 1;

		if (addToJoin(waiting->tid, tid) == THR_ERROR)
			return THR_ERROR;

		dispatcher();
	}

	return THR_SUCCESS;
}

int csem_init(csem_t * sem, int count)
{
	sem->fila = (PFILA2) malloc(sizeof(FILA2));
	if (sem->fila == NULL) {
		return error("malloc'ing semaphor queue");
	}
	sem->count = count;
	CreateFila2(sem->fila);

	return THR_SUCCESS;
}

int cwait(csem_t * sem)
{
	sem->count--;

	int been_here = 0;
	TCB_t *caller;
	caller = getTCB(executing_now);
	if (getcontext(&(caller->context)) == -1) {
		return error("error getting context in cwait()");
	}

	if (!been_here && sem->count < 0) {
		floginfo("%d is waiting for resource", executing_now);
		AppendFila2(sem->fila, getTCB(executing_now));
		been_here = 1;
		dispatcher();
	}

	return THR_SUCCESS;
}

int csignal(csem_t * sem)
{
	sem->count++;

	if (sem->count >= 0) {
		floginfo("%d released a resource", executing_now);
		FirstFila2(sem->fila);
		TCB_t *first;
		if ((first = (TCB_t *) GetAtIteratorFila2(sem->fila)) != NULL) {
			DeleteAtIteratorFila2(sem->fila);
			addToApts(first->tid);
		} else {
			loginfo("Nobody waits for this resource");
			return THR_ERROR;
		}
	}

	return THR_SUCCESS;
}
