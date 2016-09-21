ucontext_t m;
int been_here;

static void say_hey(void) {
    been_here = 1;
    loginfo("hey");
}

(...)

//  char cstack[1024];
    char *cstack = (char *)malloc(1024);
    TCB_t *thr;
    thr = TCB_init(++last_tid);
    thr->context.uc_stack.ss_sp = cstack;
    thr->context.uc_stack.ss_size = 1024;

    been_here = 0;
    getcontext(&m);

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
}
