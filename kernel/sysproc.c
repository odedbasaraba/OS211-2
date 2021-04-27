#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}
//Ass2-2.1.3
uint64
sys_sigprocmask(void)
{
  int new_mask;
  argint(0, &new_mask);
  return ((uint64)sigprocmask(new_mask));
 
}
uint64
sys_sigaction(void)
{
  struct proc *p=myproc();
  int signalnumber;
    uint64 act;
  uint64 oldact;

  if((argint(0, &signalnumber) < 0) || (argaddr(1, &act)<0) ||  (argaddr(2, &oldact)< 0 ))
    return -1;
    
  return sigaction(signalnumber,act,oldact);
}
uint64
sys_sigret(void)
{
  return 0;
}
uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;
  int signum;
  if((argint(0, &pid) < 0) ||(argint(0, &pid) < 0))
    return -1;
  return kill(pid,signum);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
