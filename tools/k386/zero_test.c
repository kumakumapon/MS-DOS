#include "backing.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static k8 memory[K386_BACKING];static unsigned calls,fail;
int backing_write(const K386Backing *b,k32 at,const void *in,unsigned n){assert(b->handle==1&&at<=sizeof memory&&n<=sizeof memory-at);if(++calls==fail)return 0;memcpy(memory+at,in,n);return 1;}
int backing_copy(const K386Backing *b,k32 to,k32 from,unsigned n){assert(b->handle==1&&n&&!(n&1)&&from<=sizeof memory&&n<=sizeof memory-from&&to<=sizeof memory&&n<=sizeof memory-to&&from+n<=to);if(++calls==fail)return 0;memcpy(memory+to,memory+from,n);return 1;}
int main(void){K386Backing b={.handle=1};
 for(unsigned start=0;start<9;start++)for(unsigned n=0;n<130;n++){memset(memory,0xa5,sizeof memory);calls=fail=0;assert(backing_zero(&b,start,n));for(unsigned i=0;i<start+n+2;i++)assert(memory[i]==(i>=start&&i<start+n?0:0xa5));}
 memset(memory,0xa5,sizeof memory);calls=0;assert(backing_zero(&b,0,sizeof memory));assert(calls<30);for(unsigned i=0;i<sizeof memory;i++)assert(!memory[i]);
 for(unsigned n=1;n<65;n++){memset(memory,0xa5,sizeof memory);assert(backing_zero(&b,sizeof memory-n,n));for(unsigned i=sizeof memory-n;i<sizeof memory;i++)assert(!memory[i]);assert(memory[sizeof memory-n-1]==0xa5);}
 calls=0;assert(!backing_zero(&b,sizeof memory-2,3)&&!calls);assert(!backing_zero(&b,~0u,1)&&!calls);
 memset(memory,0xa5,sizeof memory);calls=0;fail=1;assert(!backing_zero(&b,10,100));assert(memory[10]==0xa5);
 calls=0;fail=3;assert(!backing_zero(&b,10,100));assert(memory[9]==0xa5&&memory[110]==0xa5);
 puts("V86 zeroing: bounded disjoint XMS doubling, odd tails, preservation and I/O failures passed");
}
