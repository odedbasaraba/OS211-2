#include "spinlock.h"
#define MAX_BSEM 128
#define NUM_OF_THREADS 20
struct binarySemaphore
{
    int value;  //locked -> value==1 , otherwise 0 
    struct spinlock lock; // used in order to lock it
    struct thread* threads[NUM_OF_THREADS];
    struct thread* currentThread;
    int descriptor;     // Sem-id
    

};
struct  binarySemaphore Bsems[MAX_BSEM];
