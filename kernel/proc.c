#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "signal.h"
#define MAX_BSEM 128
#define NUM_OF_THREADS 20
struct spinlock;

struct binarySemaphore
{
    int value;  //locked -> value==1 , otherwise 0 
    struct spinlock lock; // used in order to lock it
    struct thread* threads[NUM_OF_THREADS];
    struct thread* currentThread;
    int descriptor;     // Sem-id
    

};
struct  binarySemaphore Bsems[MAX_BSEM];

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S
//thread task 3.1-start
static void free_thread(struct thread *t);
int nexttid = 1;
struct spinlock tid_lock;
struct thread *
mythread(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct thread *t = c->thread;

  pop_off();
  return t;
}

//thread task 3.1 -end
// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  struct thread *t;
  for (p = proc; p < &proc[NPROC]; p++)
  {

    for (int i = 0; i < NTHREAD; i++)
    {
      t = &p->thread_tbl[i];
      char *pa = kalloc();
      if (pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int)(p - proc), (int)(t - p->thread_tbl));
      kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
  }
}

// initialize the proc table at boot time.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&tid_lock, "nexttid");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    threadinit(p);
  }
}
void threadinit(struct proc *p)
{
  struct thread *t;
  for (t = p->thread_tbl; t < &p->thread_tbl[NTHREAD]; t++)
  {
    initlock(&t->lock, "thread");
    t->kstack = KSTACK((int)(p - proc), (int)(t - p->thread_tbl));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}
int get_tid()
{
  int tid;

  acquire(&tid_lock);
  tid = nexttid;
  nexttid = nexttid + 1;
  release(&tid_lock);

  return tid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  struct thread *t;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:

  t = &p->thread_tbl[0];
  p->pid = allocpid();
  p->state = USED;
  p->killed = 0;
  t->state = t_USED;
  t->killed = 0;
  t->tid = get_tid();
  t->tparent=p;
  for (int i = 0; i < NUMOFSIGNALS; i++)
  {
    p->signalmasks[i] = 0;
  }
  struct trapframe *tf;
  if ((tf = (struct trapframe *)kalloc()) == 0)
  {
    free_thread(t);
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  struct thread *trd;
  p->trapframepointer = tf;
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    free_thread(t);
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  for (trd = p->thread_tbl; trd < &p->thread_tbl[NTHREAD]; trd++)
  {
    trd->trapframe = tf;
    tf++;
  }
  if ((p->user_trap_frame_backup = (struct trapframe *)kalloc()) == 0)
  {
    free_thread(t);
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  p->handlingSignal = 0;
  p->signalmask_origin = 0;
  // Allocate a trapframe page.
  // if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  // {
  //   freeproc(p);
  //   release(&p->lock);
  //   return 0;
  // }

  // An empty user page table.

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&t->context, 0, sizeof(t->context));

  //Ass2-2.1.2-1
  init_siganls_handlers_to_default(p);
  p->pendingsignals = 0;
  t->context.ra = (uint64)forkret;
  t->context.sp = t->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  struct thread *t;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  if (p->trapframepointer)
    kfree((void *)p->trapframepointer);
  if (p->user_trap_frame_backup)
    kfree((void *)p->user_trap_frame_backup);
  p->trapframepointer = 0;

  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  for (int i = 0; i < NTHREAD; i++)
  {
    t = &p->thread_tbl[i];
    free_thread(t);
  }
}
static void
free_thread(struct thread *t)
{
  t->trapframe = 0;
  t->killed = 0;
  t->chan = 0;
  t->xstate = 0;
  t->tparent = 0;
  t->state = t_UNUSED;
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframepointer), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;
  struct thread *t;
  p = allocproc();
  t = &p->thread_tbl[0];
  initproc = p;

  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  t->trapframe->epc = 0;     // user program counter
  t->trapframe->sp = PGSIZE; // user stack pointer

  //Ass2-2.1.2  just to be sure
  init_siganls_handlers_to_default(p);
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // p->state = RUNNABLE;
  t->state = t_RUNNABLE;
  release(&p->lock);
}


// ASS 2 4.1


void initsems(void){
    for(int i=0;i<MAX_BSEM;i++){
   { 
        initlock(&Bsems[i].lock,"BsemLock");
        acquire(&Bsems[i].lock);
        Bsems[i].descriptor=-1; // unusedBsem
        Bsems[i].value=1;
        release(&Bsems[i].lock);
   }
}
}

// END
void init_siganls_handlers_to_default(struct proc *p)
{

  for (int i = 0; i < NUMOFSIGNALS; i++)
  {
    p->signalhandlers[i] = (void *)SIG_DFL;
  }
}
// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *p = myproc();
  acquire(&p->lock);
  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0)
    {
      release(&p->lock);
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  release(&p->lock);
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct thread *t = mythread();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  np->pendingsignals = 0;
  // copy saved user registers.
  *(np->thread_tbl->trapframe) = *(t->trapframe);

  // Cause fork to return 0 in the child.
  np->thread_tbl->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));
  np->signalmask = p->signalmask;
  copy_signal_handlers(np, p);
  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  // acquire(&np->thread_tbl[0].lock);
  // np->state = RUNNABLE;
  np->thread_tbl[0].state = t_RUNNABLE;
  np->state = USED;
  // release(&np->thread_tbl[0].lock);
  release(&np->lock);

  return pid;
}

int sigret_proc(void)
{
  struct proc *p = myproc();
  struct thread *t = mythread();
  acquire(&p->lock);
  p->handlingSignal = 0;
  memmove(t->trapframe, p->user_trap_frame_backup, sizeof(struct trapframe)); //TODO : maybe move backup to thread
  p->signalmask = p->signalmask_origin;
  release(&p->lock);
  return 0;
}
int sigaction_proc(int signum, uint64 act, uint64 oldact)
{
  struct proc *p = myproc();
  if (p == 0 || signum > 33 || signum < 0)
    return -1;
  acquire(&p->lock);
  struct sigaction tmp_old;
  struct sigaction tmp_sig;
  if (oldact != 0)
  {
    tmp_old.sa_handler = p->signalhandlers[signum];
    tmp_old.sigmask = p->signalmasks[signum];
    if (copyout(p->pagetable, oldact, (char *)&tmp_old, sizeof(struct sigaction)) < 0)
    {
      release(&p->lock);
      return -1;
    }
  }

  if (act != 0)
  {
    if (signum == SIGKILL || signum == SIGSTOP)
    {
      release(&p->lock);
      return -1;
    }
    if (copyin(p->pagetable, (char *)&tmp_sig, act, sizeof(struct sigaction)) < 0)
    {
      release(&p->lock);
      return -1;
    }
    p->signalhandlers[signum] = tmp_sig.sa_handler;
    p->signalmasks[signum] = tmp_sig.sigmask;
  }
  release(&p->lock);
  return 0;
}
int sigprocmask_proc(int newmask)
{
  struct proc *p = myproc();

  uint old_mask = p->signalmask;
  p->signalmask = newmask;
  return old_mask;
}
void copy_signal_handlers(struct proc *np, struct proc *p)
{
  for (int i = 0; i < NUMOFSIGNALS; i++)
  {
    np->signalhandlers[i] = p->signalhandlers[i];
  }
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{

  struct proc *p = myproc();
  struct thread *t = mythread();
  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  killthreads(); //need to kill all other threads

  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;
  acquire(&wait_lock);
  // if (holding(&p->lock))
  //   release(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  acquire(&p->lock);

  t->state = TZOMBIE;
  p->xstate = status;
  p->state = ZOMBIE;
  t->xstate = status;
  release(&wait_lock);

  // Jump into the scheduler, never to return.
  // release(&p->lock);
  sched();
  panic("zombie exit");
}
void killthreads()
{

  struct proc *p = myproc();
  struct thread *t = mythread();
  struct thread *trd;

  if (t->killed)
  {
    wakeup(t);
    acquire(&t->lock);
    t->state = TZOMBIE;
    sched();
  }
  else
  {
    for (int i = 0; i < NTHREAD; i++)
    {
      trd = &p->thread_tbl[i];
      if (trd->tid != t->tid)
        if (trd->state != TZOMBIE && trd->state != t_UNUSED)
          trd->killed = 1;
    }
  }
  int got;
  for (;;)
  {
    got = 0;
    for (int i = 0; i < NTHREAD; i++)
    {
      trd = &p->thread_tbl[i];
      if (trd->tid != t->tid)
        if (trd->state != TZOMBIE && trd->state != t_UNUSED)
          got = 1;
    }
    if (got)
      yield();
    else
    {
      break;
    }
  }
}
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (np = proc; np < &proc[NPROC]; np++)
    {
      if (np->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if (np->state == ZOMBIE)
        {
          // Found one.
          pid = np->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                   sizeof(np->xstate)) < 0)
          {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || p->killed)
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct thread *t;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == USED)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        for (int i = 0; i < NTHREAD; i++)
        {
          t = &p->thread_tbl[i];
          if (t->state == t_RUNNABLE)
          {
            t->state = t_RUNNING;
            c->thread = t;
            swtch(&c->context, &t->context);
            c->thread = 0;
          }
          // Process is done running for now.
          // It should have changed its p->state before coming back.
        }
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct thread *t = mythread();
  struct proc *p = myproc();
  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1 && mycpu()->noff != 2)
    panic("sched locks");
  if (t->state == t_RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&t->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct thread *t = mythread();
  struct proc *p = myproc();
  acquire(&p->lock);
  t->state = t_RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct thread *t = mythread();
  struct proc *p = myproc();
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = t_SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;
  struct thread *t;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    for (int i = 0; i < NTHREAD; i++)
    {
      t = &p->thread_tbl[i];

      if (t->state == t_SLEEPING && t->chan == chan)
      {
        t->state = t_RUNNABLE;
      }
    }
    release(&p->lock);
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid, int signum)
{
  struct proc *p;
  //uint sign = 1 << signum; dont work with this
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if ((p->pid == pid) && (p->state == ZOMBIE || p->state == UNUSED))
    {
      release(&p->lock);
      return -1;
    }
    if (p->pid == pid)
    { //2.2.1 check if its the right place
      p->pendingsignals = p->pendingsignals | signum;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}
void kthread_exit_proc(int status)
{
  struct thread *curr_t = mythread();
  struct proc *p = curr_t->tparent;
  struct thread *t;
  int num_of_threads = 0;

  acquire(&p->lock);
  for (int i = 0; i < NTHREAD; i++)
  {
    t = &p->thread_tbl[i];
    if (t->tid != curr_t->tid)
      if ((t->state == t_SLEEPING) || (t->state == t_RUNNABLE) || (t->state == t_RUNNING) || (t->state == t_UNUSED))
        num_of_threads = 1;
  }
  release(&p->lock);
  if (num_of_threads)
  {
    acquire(&wait_lock);
    wakeup(curr_t);
    acquire(&p->lock);
    curr_t->state = TZOMBIE;
    release(&wait_lock);
    sched();
    panic("zombie exit");
  }
  else
  {
    exit(status);
  }
}
int kthread_join_proc(int tthread_id, int * status)
{

  struct proc *p = myproc();
  struct thread *t;
  if (tthread_id == mythread()->tid || tthread_id > nexttid || tthread_id < 1)
    return -1;
  acquire(&wait_lock);
  acquire(&p->lock);
  for (int i =0; i < NTHREAD; i++)
  {
    t = &p->thread_tbl[i];
    if (t->tid == tthread_id)
      break;
  }
  while (t->tid == tthread_id && t->state != TZOMBIE)
  {
    release(&p->lock);
    sleep(t, &wait_lock);
    acquire(&p->lock);
  }
  if (status != 0 && copyout(p->pagetable, (uint64)status, (char *)&t->xstate,
                             sizeof(t->xstate)) < 0)
  {
    release(&p->lock);
    release(&wait_lock);
    return -1;
  }
  free_thread(t);
  release(&wait_lock);
  release(&p->lock);
  return 0;
}


// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      // [SLEEPING] "sleep ",
      // [RUNNABLE] "runble",
      // [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
int kthread_create_proc(void (*start_func)(), void *stack)
{
  struct proc *p = myproc();
  struct thread *t;
  acquire(&p->lock);
  for (int i = 0; i < NTHREAD; i++)
  {
    t = &p->thread_tbl[i];
    if (t->state == t_UNUSED)
    {
      t->state = t_RUNNABLE;
      t->tid = get_tid();
      t->trapframe->epc = (uint64)start_func;
      t->trapframe->sp = (uint64)stack + MAX_STACK_SIZE - 16;
      memset(&t->context, 0, sizeof(t->context));
      t->context.ra = (uint64)forkret;
      t->context.sp = t->kstack + PGSIZE;
      t->tparent = p;
      release(&p->lock);
      return t->tid;
    }
  }
  release(&p->lock);
  return -1;
}
int kthread_id_proc(void)
{
  struct thread *t = mythread();
  int tid;

  acquire(&t->tparent->lock);
  tid = t->tid;
  release(&t->tparent->lock);

  if (tid)
    return tid;
  return -1;
}

void 
remove_current_thread(struct thread* threads[]){
    struct thread* this_thread= mythread();
    for(int i=0;i<NUM_OF_THREADS;i++){
        if(threads[i]==this_thread){
           threads[i]=0;
           return;
        }
    }
}
void place_thread_in_line(struct thread* threads[])
{
    struct thread* this_thread= mythread();
    for(int i=0;i<NUM_OF_THREADS;i++){
        if(threads[i]==0){
           threads[i]=this_thread;
           return;
        }
    }
    panic("No More space for threads in Bsem");

}
int bsem_alloc(void){

    int descriptor=-1;
    for(int i=0;i<MAX_BSEM;i++){
        acquire(&Bsems[i].lock);
        if(Bsems[i].value==-1){
            Bsems[i].value=1;
            Bsems[i].descriptor=i;
            descriptor=i;
        }
        release(&Bsems[i].lock);
        break;
    }
       return descriptor;
}

void 
bsem_free(int descriptor){
        acquire(&Bsems[descriptor].lock);
        if(Bsems[descriptor].descriptor==-1 || Bsems[descriptor].value==0)// not even used || in use right now
        {
            release(&Bsems[descriptor].lock);
            return;
        }
        
        Bsems[descriptor].descriptor=-1; // unusedBsem
        release(&Bsems[descriptor].lock);
}
void 
bsem_down(int descriptor){
    for(;;){
        acquire(&Bsems[descriptor].lock);
            if(Bsems[descriptor].descriptor==-1)// not even used
            {
            release(&Bsems[descriptor].lock);
                return;
            }
        
        if(Bsems[descriptor].value==1)
        {
            Bsems[descriptor].currentThread=mythread();
            remove_current_thread(Bsems[descriptor].threads);
            Bsems[descriptor].value=0;
            release(&Bsems[descriptor].lock);
            break;
        }
        else{ 
            place_thread_in_line(Bsems[descriptor].threads);
            sleep(&Bsems[descriptor],&Bsems[descriptor].lock);
        }
}
}


void 
bsem_up(int descriptor){
        acquire(&Bsems[descriptor].lock);
        if(Bsems[descriptor].descriptor==-1){
            release(&Bsems[descriptor].lock);
            return;

        }
        Bsems[descriptor].currentThread=0;
        Bsems[descriptor].value=1;    
        wakeup(&Bsems[descriptor]);
        release(&Bsems[descriptor].lock);
}