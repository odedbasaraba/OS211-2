#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#include "signal.h"

extern char sigretBEG[];
extern char sigretEND[];

int sigkill_handler()
{
  struct proc *p = myproc();
  struct thread *t=mythread();
  acquire(&p->lock);
  p->killed = 1;
  if (t->state == t_SLEEPING)
  {
    // Wake process from sleep().
    t 
    ->state = t_RUNNABLE;
  }
  for (int i = 0; i < NTHREAD; i++)
  {
    t = &p->thread_tbl[i];
    acquire(&t->lock);
    if (t->state == t_SLEEPING)
      t->state = t_RUNNABLE;
    release(&t->lock);
  }
  release(&p->lock);
  return 0;
}
//handle the cont signal and shut down the cont bit that we handled 2.3.0
int sigcont_handler()
{
  struct proc *p = myproc();
  if (!p->freeze)
    return -1;
  p->freeze = 0;
  p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
  return 0;
}
//handle the stop signal and shut down the stop bit that we handled 2.3.0

int sigstop_handler()
{
  struct proc *p = myproc();
  struct thread *t = mythread();
  if (p->freeze)
    return -1;
  else if (t->state == t_RUNNABLE || t->state == t_RUNNING)
  {
    acquire(&p->lock);
    p->freeze = 1;
    release(&p->lock);
    for (;;)
    {
      acquire(&p->lock);
      if (!(p->pendingsignals & (1 << SIGCONT)))
      {
        release(&p->lock);
        yield();
      }

      else
      {
        p->freeze = 0;
        p->pendingsignals = p->pendingsignals ^ (1 << SIGSTOP);
        p->pendingsignals = p->pendingsignals ^ (1 << SIGCONT);
        release(&p->lock);
        break;
      }
    }
  }
  return 0;
}
void signalhandler_user(int i)
{

  struct proc *p = myproc();
  struct thread *t = mythread();
  struct sigaction tmp_action;
  acquire(&p->lock);
  copyin(p->pagetable, (void *)(&tmp_action.sa_handler), (uint64)p->signalhandlers[i], sizeof(void *));
  copyin(p->pagetable, (void *)(&tmp_action.sigmask), (uint64)p->signalmasks[i], sizeof(uint32));
  p->signalmask_origin = p->signalmask;
  p->signalmask = tmp_action.sigmask;
  p->handlingSignal = 1;
  uint64 new_sp;
  new_sp = t->trapframe->sp - sizeof(struct trapframe);
  //p->user_trap_frame_backup=(struct trapframe*)new_sp;
  //memmove(p->user_trap_frame_backup,p->trapframe,sizeof(struct trapframe));
  //p->user_trap_frame_backup->sp=new_sp;
  copyout(p->pagetable, new_sp, (void *)t->trapframe->sp, sizeof(uint64));
  t->trapframe->epc = (uint64)tmp_action.sa_handler;
  uint64 funcsize = sigretEND - sigretBEG;
  t->trapframe->sp -= funcsize;
  copyout(p->pagetable, t->trapframe->sp, (void *)&sigretBEG, funcsize);
  t->trapframe->a0 = i;
  t->trapframe->ra = t->trapframe->sp;
  p->pendingsignals = p->pendingsignals ^ (1 << i); //i-1 maybe
  release(&p->lock);
}

void signal_handler()
{

  struct proc *p = myproc();
  if (p->handlingSignal)
    return;
  for (int i = 0; i < NUMOFSIGNALS; i++)
  {
    if ((p->pendingsignals & 1 << i) & !(p->signalmask & 1 << i))
    {
      if (p->signalhandlers[i] == (void *)SIG_IGN)
        continue;
      else if (p->signalhandlers[i] == (void *)SIG_DFL)
        sigkill_handler();
      else if (i == SIGKILL)
        sigkill_handler();
      else if (i == SIGCONT)
        sigcont_handler();
      else if (i == SIGSTOP)
        sigstop_handler();
      else
        //backup
        signalhandler_user(i);
    }
    else
    {
      return;
    }
  }
}

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void)
{
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  struct thread *t = mythread();

  // save user program counter.
  t->trapframe->epc = r_sepc();

  if (r_scause() == 8)
  {
    // system call

    if (p->killed)
      exit(-1);
    // if (t->killed)
    // {
    //   wakeup(t);
    //   acquire(&t->lock);
    //   t->state = TZOMBIE;
    //   sched();
    // }
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    t->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  }
  else if ((which_dev = devintr()) != 0)
  {
    // ok
  }
  else
  {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if (p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
  struct proc *p = myproc();
  struct thread *t = mythread();
  memmove(p->user_trap_frame_backup, t->trapframe, sizeof(struct trapframe)); //copy

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  t->trapframe->kernel_satp = r_satp();         // kernel page table
  t->trapframe->kernel_sp = t->kstack + PGSIZE; // process's kernel stack
  t->trapframe->kernel_trap = (uint64)usertrap;
  t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()
  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);
  // set S Exception Program Counter to the saved user pc.
  w_sepc(t->trapframe->epc);
  signal_handler();
  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64, uint64))fn)(TRAPFRAME + (sizeof(struct trapframe) * (t - p->thread_tbl)), satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && mythread() != 0 && mythread()->state == t_RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
  uint64 scause = r_scause();

  if ((scause & 0x8000000000000000L) &&
      (scause & 0xff) == 9)
  {
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if (irq == UART0_IRQ)
    {
      uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000001L)
  {
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if (cpuid() == 0)
    {
      clockintr();
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  }
  else
  {
    return 0;
  }
}
