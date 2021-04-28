#include "proc.h"
#include "defs.h"

int sigkill_handler()
{
    struct proc *p = myproc();
    p->killed = 1;
    if (p->state == SLEEPING)
    {
        // Wake process from sleep().
        p->state = RUNNABLE;
    }
    return 0;
}

int sigcont_handler()
{
    struct proc *p = myproc();
    if (!p->freeze)
        return -1;
    p->freeze = 0;
    return -1;
}

int sigstop_handler()
{
    struct proc *p = myproc();
    if (p->freeze)
        return -1;
    else if (p->state == RUNNABLE || p->state == RUNNING)
    {
        p->freeze = 1;
        yield();
    }
    return 0;
}