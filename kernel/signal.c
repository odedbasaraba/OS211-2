
// //#include "signal.h"


// extern void *sigretBEG(void);
// extern void *sigretEND(void);

// int sigkill_handler()
// {
//     struct proc *p = myproc();
//     p->killed = 1;
//     if (p->state == SLEEPING)
//     {
//         // Wake process from sleep().
//         p->state = RUNNABLE;
//     }
//     return 0;
// }
// //handle the cont signal and shut down the cont bit that we handled 2.3.0
// int sigcont_handler()
// {
//     struct proc *p = myproc();
//     if (!p->freeze)
//         return -1;
//     p->freeze = 0;
//     p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
//     return 0;
// }
// //handle the stop signal and shut down the stop bit that we handled 2.3.0

// int sigstop_handler()
// {
//     struct proc *p = myproc();
//     if (p->freeze)
//         return -1;
//     else if (p->state == RUNNABLE || p->state == RUNNING)
//     {
//         p->freeze = 1;
//         for (;;)
//         {

//             if (p->pendingsignals & (1 << SIGCONT))

//                 acquire(&p->lock);
//             if (!(p->pendingsignals & 1 << SIGCONT))
//             {
//                 release(&p->lock);
//                 yield();
//             }

//             else
//             {
//                 p->freeze = 0;
//                 p->pendingsignals = p->pendingsignals ^ (1 << SIGSTOP);
//                 p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
//                 release(&p->lock);
//                 break;
//             }
//         }
//     }
//     return 0;
// }
// void signalhandler_user(int i)
// {
//     struct proc *p = myproc();
//     struct sigaction tmp_action;
//     copyin(p->pagetable, &tmp_action, p->signalhandlers[i], sizeof(struct sigaction));
//     p->signalmask_origin = p->signalmask;
//     p->signalmask = tmp_action.sigmask;
//     p->handlingSignal = 1;


//     uint64 new_sp;
//     new_sp = p->trapframe->sp - sizeof(struct trapframe);
//     //p->user_trap_frame_backup->sp=new_sp;
//     copyout(p->pagetable, new_sp, p->trapframe->sp, sizeof(uint64));
//     p->trapframe->epc = (uint64)tmp_action.sa_handler;
//     uint64 funcsize = sigretEND - sigretBEG;
//     p->trapframe->sp -= funcsize;
//     while(p->trapframe->sp--%4!=0);
//     copyout(p->pagetable, p->trapframe->sp, (void *)&sigretBEG, funcsize);
//     p->trapframe->a0 = i;
//     p->trapframe->ra = p->trapframe->sp;
//     p->pendingsignals = p->pendingsignals ^ (1 << (i - 1)); //i-1 maybe
// }

// void signal_handler()
// {
//     struct proc *p = myproc();
//     if (p->handlingSignal)
//         return;
//     for (int i = 0; i < NUMOFSIGNALS; i++)
//     {
//         if ((p->pendingsignals & 1 << i) & !(p->signalmask & 1 << i))
//         {
//             if (p->signalhandlers[i] == (void *)SIG_IGN)
//                 continue;
//             else if (p->signalhandlers[i] == (void *)SIG_DFL)
//                 sigkill_handler();
//             else if (i == SIGKILL)
//                 sigkill_handler();
//             else if (i == SIGCONT)
//                 sigcont_handler();
//             else if (i == SIGSTOP)
//                 sigstop_handler();
//             else
//                 //backup
//                 signalhandler_user(i);
//         }
//         else
//         {
//             return;
//         }
//     }
// }
