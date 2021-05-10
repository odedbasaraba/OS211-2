#include "binarySemaphore.h"
#include "defs.h"
#include "proc.h"

extern struct  binarySemaphore Bsems[MAX_BSEM];

void initsems(void){
    for(int i=0;i<MAX_BSEM;i++){
   { 
        initlock(&Bsems[i].lock,"BsemLock");
        accquire(&Bsems[i].lock);
        Bsems[i].descriptor=-1; // unusedBsem
        Bsems[i].value=1;
        release(&Bsems[i].lock);
   }
}
}

int bsem_alloc(void){

    int descriptor=-1;
    for(int i=0;i<MAX_BSEM;i++){
        accquire(&Bsems[i].lock);
        if(Bsems[i].value==-1){
            Bsems[i].value=1;
            Bsems[i].descriptor=i;
            Bsems[i].try_to_delete=0;
            descriptor=i;
        }
        release(&Bsems[i].lock);
        break;
    }
       return descriptor;
}
void 
bsem_free(int descriptor){
    int descriptor=-1;
        accquire(&Bsems[descriptor].lock);
        if(Bsems[descriptor].descriptor==-1 || Bsems[descriptor].try_to_delete==1 || Bsems[descriptor].value==0)// not even used
        {
            release(&Bsems[descriptor].lock);
            return;
        }
        
       if(someone_sleeps_on(descriptor)){
           return;
       }

       accquire(&Bsems[descriptor].lock);
        Bsems[descriptor].descriptor=-1; // unusedBsem
        Bsems[descriptor].try_to_delete=0;
        release(&Bsems[descriptor].lock);
}
void 
bsem_down(int descriptor){
    struct proc *p = myproc();

    accquire(&p->lock);
            p->sleeps_on=descriptor;
    for(;;){
        accquire(&Bsems[descriptor].lock);
            if(Bsems[descriptor].descriptor==-1 || Bsems[descriptor].try_to_delete==1)// not even used
            {
            release(&Bsems[descriptor].lock);
                return;
            }
        
        if(Bsems[descriptor].value==1)
        {
            Bsems[descriptor].value=0;
            release(&Bsems[descriptor].lock);

            break;
        }
        else 
            accquire(&p->lock);
            p->sleeps_on=descriptor;
            p->state=SLEEPING;
            release(&p->lock);
            release(&Bsems[descriptor].lock);
            yield();
        }
}
void 
bsem_up(int descriptor){
    int descriptor=-1;
    for(int i=0;i<MAX_BSEM;i++){
        accquire(&Bsems[i].lock);
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