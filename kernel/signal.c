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
                p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
                release(&p->lock);
                break;
            }
        }
    }
    return 0;
}
int
signal_handler()
{   
    struct proc * p=myproc();
    for (int i = 0; i < NUMOFSIGNALS; i++)
    {
        if((p->pendingsignals&1<<i)& !(p->signalmask& 1<<i))
        {
            if (p->signalhandlers[i]==(void*)SIG_IGN)
            continue;
            else if(p->signalhandlers[i]==(void*)SIG_DFL)
                sigkill_handler();
            else if (i==SIGKILL)
                sigkill_handler();
            else if (i==SIGCONT)
                sigcont_handler();
            else if (i==SIGSTOP)
                sigstop_handler();
                else
                            //backup

            signalhandler_user();
            return 0;
        }
        else{
            return -1;
        }
    }
    
}