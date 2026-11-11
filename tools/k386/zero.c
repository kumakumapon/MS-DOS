#include "backing.h"
int backing_zero(const K386Backing *p,k32 at,k32 count) {
    /* Seed a small zero block, then duplicate only initialized XMS bytes.
       No shared transfer-buffer alias and no 1 KiB resident zero table. */
    k8 zero[32]={0};if(at>K386_BACKING||count>K386_BACKING-at)return 0;
    unsigned first=count>sizeof zero?sizeof zero:count;
    if(first&&!backing_write(p,at,zero,first))return 0;
    k32 done=first;
    while(done<count){unsigned n=count-done;if(n>done)n=done;n&=~1u;
        if(!n){if(!backing_write(p,at+done,zero,1))return 0;done++;}
        else{if(!backing_copy(p,at+done,at,n))return 0;done+=n;}
    }
    return 1;
}

