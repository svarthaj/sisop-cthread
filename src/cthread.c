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
FILA2 wait_q; // queue for thread being waited for other threads
PFILA2 pwait_q; 
FILA2 waitpair;
PFILA2 pwaitpair;
FILA2 cjoinbloq_q;
PFILA2 pcjoinbloq_q;
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

    loginfo("creating wait queue");
    pwait_q = &wait_q;
    CreateFila2(pwait_q);

    loginfo("creating waitpair queue");
    pwaitpair = &waitpair;
    CreateFila2(pwaitpair);
    
    loginfo("creating cjoinbloq queue");
    pcjoinbloq_q = &cjoinbloq_q;
    CreateFila2(pcjoinbloq_q);
    
    loginfo("setting main to executing: ");
    executing_now = 0;
}

/* return a pointer to the TCB with that tid */
TCB_t *getTCB(int tid) {
    if (tid > MAXTHREADS || !valid_threads[tid]) {
        flogerror("Tried to access invalid thread %d", tid);
        return NULL;
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
   return NULL, for at least the main thread exist. */
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

static void addToQueue(int tid, PFILA2 pqueue) {
    floginfo("adding %d to queue", tid);
    AppendFila2(pqueue, getTCB(tid));
}

static void removeFromQueue(int tid, PFILA2 pqueue) {
    floginfo("removing %d from queue", tid);
    FirstFila2(pqueue);

    TCB_t *curr; /* current */
    while ((curr = (TCB_t *)GetAtIteratorFila2(pqueue)) != NULL) {
        if (curr->tid == tid) {
            DeleteAtIteratorFila2(pqueue);
            break;
        } else {
            NextFila2(pqueue);
        }
    }
}

static int searchQueue(int tid, PFILA2 pqueue) {
    floginfo("searching %d in queue", tid);
    FirstFila2(pqueue);

    TCB_t *curr;
    while ((curr = (TCB_t *)GetAtIteratorFila2(pqueue)) != NULL ) {
        if(curr->tid == tid) {
            floginfo("%d found", tid);
            return 0;
        } else {
            NextFila2(pqueue);
        }
    }
    
    flogerror("%d was not found", tid);
    return -1;
}

static int removeFromPairQueue(int tid, PFILA2 pairq) {
    floginfo("removing %d from pair queue", tid);
    FirstFila2(pairq);

    WAITPAIR *curr; /* current */
    while ((curr = (WAITPAIR *)GetAtIteratorFila2(pairq)) != NULL) {
        if (curr->target == tid) {
            DeleteAtIteratorFila2(pairq);
            break;
        } else {
            NextFila2(pairq);
        }
    }
}

WAITPAIR *getWaitPair(int tid, PFILA2 pairq) {
    floginfo("fetching wait pair with target %d", tid);
    FirstFila2(pairq);

    WAITPAIR *curr; /* current */
    while ((curr = (WAITPAIR *)GetAtIteratorFila2(pairq)) != NULL) {
        if (curr->target == tid) {
            return curr;
        } else {
            NextFila2(pairq);
        }
    }
    
    return NULL;
}

/***** TCB manipulation */

/* return initialized TCB, insert it to the TCB "catalog" */
static TCB_t *TCB_init(int tid) {
    floginfo("initializing TCB %d", tid);
    TCB_t *thr = (TCB_t *)malloc(sizeof(TCB_t));
    int ss_size = SIGSTKSZ;
    char stack[ss_size];

    thr->tid = tid;
    thr->state = PROCST_APTO;
    thr->ticket = tid;
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
}

static void dispatcher(void) {
    int lucky = Random2()%MAXTHREADS; // THIS NEEDS TO BE CHANGED
    floginfo("closest to %d will be selected for dispatch", lucky);
    TCB_t *tcb = getClosestTCB(lucky);
    removeFromApts(tcb->tid);
    executing_now = tcb->tid;
    floginfo("dispatching %d", tcb->tid);
    flogdebug("setting context at address %p", &(tcb->context));
    setcontext(&(tcb->context));
}

static void terminate(int tid) {
    /* does a bucnh of stuff 
     *
     * will be linked from makecontext to uc_link during tcb creation
     * tid is the terminating thread tid
     *
     * TODO: remove tid entry from threads and valid_threads
     */   
    
    WAITPAIR *curr;
    // search waitpair queue for thread that is terminating
    if( (curr = getWaitPair(tid, pwaitpair)) != NULL ) {    
        int caller_tid;
        TCB_t *caller;
        
        caller_tid = curr->caller;
        // remove entry from waitpair queue
        removeFromPairQueue(tid, pwaitpair);
        // remove thread that is waiting from wait queue, cjoinbloq
        removeFromQueue(caller_tid, pcjoinbloq_q);
        removeFromQueue(caller_tid, pwait_q);
        caller = getTCB(caller_tid);
        caller->state=1; // set to executing
        // set context to this thread
        setcontext(&(caller->context));
        } else {
            // if terminating thread is not being waited, call dispatcher
            dispatcher();
        }

}

static void say_hey(void) {
    loginfo("hey");
}




/***** interface */

/* create thread */
int ccreate (void* (*start)(void*), void *arg) {
    static int last_tid = -1;

    /* check for main */
    TCB_t *main_t;
    if (last_tid == -1) {
        initAll();
        floginfo("creating thread %d.", last_tid + 1);

        int tid = ++last_tid;
        floginfo("initializing TCB %d", tid);
        main_t = (TCB_t *)malloc(sizeof(TCB_t));
        int ss_size = SIGSTKSZ;
        //char stack[ss_size];

        main_t->tid = tid;
        main_t->state = PROCST_APTO;
        main_t->ticket = tid;
        flogdebug("initializing context at address %p", &(main_t->context));
        if (getcontext(&(main_t->context)) == -1) logerror("error initializing context");
        main_t->context.uc_stack.ss_sp = (char*)malloc(SIGSTKSZ);
        main_t->context.uc_stack.ss_size = SIGSTKSZ;
        flogdebug("stack pointer is %p and stack size is %d", main_t->context.uc_stack.ss_sp, main_t->context.uc_stack.ss_size);
        
        main_t->state = PROCST_EXEC;
        main_t->context.uc_link = NULL;

        keepTCB(main_t); /* insert in catalog */
        
        floginfo("created thread %d.", main_t->tid);
        printTCB(*main_t);
    }

    floginfo("creating thread %d.", last_tid + 1);

    int tid = ++last_tid;
    floginfo("initializing TCB %d", tid);
    TCB_t *thr = (TCB_t *)malloc(sizeof(TCB_t));
    //int ss_size = SIGSTKSZ;
    //char stack[ss_size];

    thr->tid = tid;
    thr->state = PROCST_APTO;
    thr->ticket = tid;
    flogdebug("initializing context at address %p", &(thr->context));
    if (getcontext(&(thr->context)) == -1) logerror("error initializing context");
    thr->context.uc_stack.ss_sp = (char*)malloc(SIGSTKSZ);
    thr->context.uc_stack.ss_size = SIGSTKSZ;
    flogdebug("stack pointer is %p and stack size is %d", main_t->context.uc_stack.ss_sp, main_t->context.uc_stack.ss_size);

    loginfo("creating terminate context");
    ucontext_t term_context;
    //char term_stack[SIGSTKSZ];
    getcontext(&term_context); //get context template
    term_context.uc_link=&(main_t->context);
    term_context.uc_stack.ss_sp=(char*)malloc(SIGSTKSZ);
    term_context.uc_stack.ss_size=SIGSTKSZ; 
    makecontext(&(term_context), (void(*)(void))terminate, thr->tid);
    flogdebug("terminate context : uc_link points at main_context at %p - stack pointer is %p and stack size if %d", term_context.uc_link, term_context.uc_stack.ss_sp, term_context.uc_stack.ss_size);
    thr->context.uc_link = &term_context;
    
    keepTCB(thr); /* insert in catalog */
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
    TCB_t *target;
    
    // checks if tid is a valid_thread
    if( (target = getTCB( tid ))) {
        // checks if target_tcb is not finished (status!=4)
        if( target->state != 4 ) {
            // checks if tagret_tcb is being waited by anyone
            if( searchQueue(tid, pwait_q) == -1 ) {
                addToQueue(target->tid, pwait_q); // add target to waited_q
                TCB_t *caller;
                caller = getTCB(executing_now); // get caller tcb
                caller->state = 3; // change state to blocked
                addToQueue(caller->tid, pcjoinbloq_q); // add to cjoinbloq_q - not sure if needed
                
                /* set up waiting pair */
                WAITPAIR *wp = (WAITPAIR*)malloc(sizeof(WAITPAIR));
                wp->caller = caller->tid;
                wp->target = target->tid;
                AppendFila2( pwaitpair, wp); // add pair to queue
                
                int dispatched = 0;
                getcontext(&(caller->context));
                
                if(!dispatched) {
                    dispatched = 1;
                    dispatcher();
                }
    
                /* when this context is set again and target is finished */
                caller->state = 1; // change caller state to ready
                removeFromQueue(caller->tid, pcjoinbloq_q); 
                addToQueue(caller->tid, papts_q); 
                //removeFromQueue(target->tid, pwait_q); -> target is remove at terminate()

                return 0;

            } else {
                flogerror("%d is already being waited, can't call cjoin", tid);
                return -1;
            }       
        } else {
            flogerror("%d is finished, can't call cjoin", tid);
            return -1;
        }    
    } else {
        flogerror("%d is not a valid tcb, can't call cjoin", tid);
        return -1;
    }
}

int csem_init(csem_t *sem, int count) {
    //FILA2 bloq;
    PFILA2 pbloq = (PFILA2)malloc(sizeof(FILA2));

    if(CreateFila2(pbloq) != 0) {
        logerror("Fail to create pbloq\n");    
        return -1;
    }

    sem->count = count;
    sem->fila = pbloq;    
    floginfo("Succesfully created csem with count: %d\n", sem->count);

    return 0;
}

int cwait(csem_t *sem) { 
    if(sem->count <= 0) {
        // acessa TCB que chamou cwait
        TCB_t *waiting_tcb;
        waiting_tcb = getTCB( executing_now );
        // altera state da TCB=3
        waiting_tcb->state = 3;       
        // adiciona TCB a sem->fila
        AppendFila2( sem->fila, waiting_tcb );
    }
    sem->count--;
    
    return 0;
}

int csignal(csem_t *sem) {
    sem->count++;
    
    // acessa o primeiro da fila de bloqueados
    FirstFila2( sem->fila );
    TCB_t  *unblocked_tcb;
    
    if( (unblocked_tcb = (TCB_t*)GetAtIteratorFila2( sem->fila )) ){
        DeleteAtIteratorFila2( sem->fila ); // remove primeiro bloqueado
        unblocked_tcb->state = 1; // muda status para apto
        AppendFila2( papts_q, unblocked_tcb ); // acrescenta em aptos
        
        return 0;
    }

    return -1;
}

int cidentify(char *name, int size) {
    return -1;
}
