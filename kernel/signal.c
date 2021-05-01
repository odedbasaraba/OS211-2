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
//handle the cont signal and shut down the cont bit that we handled 2.3.0
int sigcont_handler()
{
    struct proc *p = myproc();
    if (!p->freeze)
        return -1;
    p->freeze = 0;
    acquire(&p->lock);
    p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
    release(&p->lock);

    return 0;
}
//handle the stop signal and shut down the stop bit that we handled 2.3.0

int sigstop_handler()
{
    struct proc *p = myproc();
    if (p->freeze)
        return -1;
    else if (p->state == RUNNABLE || p->state == RUNNING)
    {
        p->freeze = 1;
        for (;;)
        {
            acquire(&p->lock);
            if (!(p->pendingsignals & 1<<SIGCONT))
            {
                release(&p->lock);
                yield();
            }

            else
            {
                p->freeze=0;
                p->pendingsignals = p->pendingsignals ^ (1 << SIGSTOP);
                release(&p->lock);
                break;
            }
        }
    }
    return 0;
}