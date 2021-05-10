#include "spinlock.h"
#define MAX_BSEM 128

struct binarySemaphore
{
    int value;  //locked -> value==1 , otherwise 0 
    struct spinlock lock; // used in order to lock it
    int descriptor;     // Sem-id
    int try_to_delete // == 1 -> cannot be used, someone tries to free it

};
struct  binarySemaphore Bsems[MAX_BSEM];
