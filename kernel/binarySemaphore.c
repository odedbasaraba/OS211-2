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
            descriptor=i;
        }
        release(&Bsems[i].lock);
        break;
    }
       return descriptor;
}

void 
bsem_free(int descriptor){
        accquire(&Bsems[descriptor].lock);
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
    struct proc *p = myproc();
    for(;;){
        accquire(&Bsems[descriptor].lock);
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
        else 
            place_thread_in_line(Bsems[descriptor].threads);
            sleep(&Bsems[descriptor],&Bsems[descriptor].lock);
        }
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
void 
bsem_up(int descriptor){
        accquire(&Bsems[descriptor].lock);
        if(Bsems[descriptor].descriptor==-1){
            release(&Bsems[descriptor].lock);
            return;

        }
        Bsems[descriptor].currentThread=0;
        Bsems[descriptor].value=1;    
        wakeup(&Bsems[descriptor]);
        release(&Bsems[descriptor].lock);
}