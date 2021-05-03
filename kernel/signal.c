#include "proc.h"
#include "defs.h"

extern void* sigretBEG(void);
extern void* sigretEND(void);

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
       p->pendingsignals=p->pendingsignals ^ (1<<SIGCONT);
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

            if (p->pendingsignals &(1<<SIGCONT)==0)

            acquire(&p->lock);
            if (!(p->pendingsignals & 1<<SIGCONT))
            {
                release(&p->lock);
                yield();
            }

            else
            {
                p->freeze=0;
                p->pendingsignals=p->pendingsignals ^ (1<<SIGSTOP);
                p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
                release(&p->lock);
                break;
                
            }
        }
    }
    return 0;
}
int
signalhandler_user(int i)
{
    struct proc * p=myproc();
    p->user_trap_frame_backup=p->trapframe;
     // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    p->trapframe->ra=sigretBEG;
    int funcsize= sigretEND - sigretBEG;
     struct sigaction * tmp_action=(struct sigaction *)p->signalhandlers[i];
     p->trapframe->epc=tmp_action->sa_handler;
       
  }

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
            return signalhandler_user(i);
        }
        else{
            return -1;
        }
    }
    
}





