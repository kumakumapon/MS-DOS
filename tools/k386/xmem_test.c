#include "xmem.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *store[64],guest[2][0xa0000],bounce[KX_CHUNK];
static unsigned sizes[64],next=1,writes,fail_new,fail_write,fail_free;
int xmem_new(k16 kb,k16 *h){if(fail_new)return 0;assert(next<64);*h=next++;sizes[*h]=kb*1024;store[*h]=malloc(sizes[*h]);assert(store[*h]);memset(store[*h],0xcd,sizes[*h]);return 1;}
int xmem_free(k16 h){assert(h&&store[h]);if(fail_free)return 0;free(store[h]);store[h]=0;return 1;}
int xmem_read(k16 h,k32 at,void *out,unsigned n){assert(h&&store[h]&&at<=sizes[h]&&n<=sizes[h]-at);memcpy(out,store[h]+at,n);return 1;}
int xmem_write(k16 h,k32 at,const void *in,unsigned n){assert(h&&store[h]&&at<=sizes[h]&&n<=sizes[h]-at);if(fail_write)return 0;writes+=n;memcpy(store[h]+at,in,n);return 1;}
int xmem_guest(unsigned id,k32 at,void *buffer,unsigned n,int write){assert(id<2&&at<sizeof guest[id]&&n<=sizeof guest[id]-at);if(write)memcpy(guest[id]+at,buffer,n);else memcpy(buffer,guest[id]+at,n);return 1;}
static K386Frame frame(unsigned op,k16 token){K386Frame f={0};f.ebx=op;f.edx=token;f.es=0x2000;f.edi=5;return f;}
static unsigned run(unsigned id,k16 owner,K386Frame *f){int pending=xmem_service(id,owner,f,bounce);assert(!pending);assert((f->eax!=0)==((f->eflags&1)!=0));return f->eax;}
static k16 alloc(unsigned id,k16 owner,unsigned kb){K386Frame f=frame(KX_ALLOC,0);f.ecx=kb;unsigned slices=0;do{writes=0;slices++;}while(xmem_service(id,owner,&f,bounce)&&writes<=KX_ZERO_SLICE&&slices<300);assert(!f.eax&&!(f.eflags&1)&&f.edx);assert(writes<=KX_ZERO_SLICE);return f.edx;}
static unsigned active(void){unsigned n=0;for(unsigned i=1;i<next;i++)n+=store[i]!=0;return n;}
int main(void){
    K386Frame f=frame(KX_QUERY,0);assert(!xmem_service(0,0x8000,&f,bounce)&&f.eax==0x389d&&f.ebx==KX_VERSION&&f.ecx==4096&&f.edx==4096);
    k16 a=alloc(0,0x8000,1024),b=alloc(1,0x8000,1024);assert(a!=b);
    KXBlock *block=&k386_xmem.blocks[0][0];assert(block->bytes==1024*1024&&block->initialized==block->bytes);
    for(unsigned i=0;i<block->bytes;i++)assert(store[block->handle][i]==0);
    for(unsigned i=0;i<5;i++)guest[0][0x20005+i]=0x31+i;
    f=frame(KX_WRITE,a);f.esi=65535;f.ecx=5;assert(!run(0,0x8000,&f));
    memset(guest[0]+0x20005,0,5);f.ebx=KX_READ;assert(!run(0,0x8000,&f));for(unsigned i=0;i<5;i++)assert(guest[0][0x20005+i]==0x31+i);
    f=frame(KX_READ,a);f.ecx=1;assert(run(1,0x8000,&f)==6);assert(run(0,0x7000,&f)==6);
    f=frame(KX_FREE,a);assert(run(1,0x8000,&f)==6&&run(0,0x7000,&f)==6);
    f=frame(KX_READ,a);f.ecx=1;f.esi=1024*1024;assert(run(0,0x8000,&f)==87);
    f.esi=0xffffffff;assert(run(0,0x8000,&f)==87);f.esi=0;f.ecx=4097;assert(run(0,0x8000,&f)==87);
    f.ecx=2;f.edi=65535;assert(run(0,0x8000,&f)==87);f.edi=0;f.es=0xa000;assert(run(0,0x8000,&f)==87);
    f.es=0x9fff;f.edi=15;assert(run(0,0x8000,&f)==87);f.ecx=1;assert(!run(0,0x8000,&f));
    f=frame(KX_READ,a);f.esi=1024*1024;assert(!run(0,0x8000,&f)); /* empty range at end */
    f=frame(KX_ALLOC,0);f.ecx=0;assert(run(0,0x8000,&f)==87);f.ecx=65537;assert(run(0,0x8000,&f)==87);
    f.ecx=4096;assert(run(0,0x8000,&f)==8); /* quota shared with parent */
    k16 child=alloc(0,0x7000,128);xmem_release(0,0x7000);f=frame(KX_READ,child);assert(run(0,0x7000,&f)==6);
    f=frame(KX_READ,a);f.ecx=1;assert(!run(0,0x8000,&f));
    fail_free=1;f=frame(KX_FREE,a);assert(run(0,0x8000,&f)==8&&active()==2);fail_free=0;assert(!run(0,0x8000,&f));assert(run(0,0x8000,&f)==6);
    k16 c=alloc(0,0x8000,1024);assert(c!=a);f=frame(KX_READ,a);assert(run(0,0x8000,&f)==6);
    xmem_release(0,0);xmem_release(1,0);assert(active()==0);
    for(unsigned i=0;i<KX_BLOCKS;i++)alloc(0,0x8000,1);
    f=frame(KX_ALLOC,0);f.ecx=1;assert(run(0,0x8000,&f)==8);xmem_release(0,0);
    k16 full=alloc(0,0x8000,4096);(void)full;unsigned count;assert(xmem_used(0,0,&count)==4096&&count==1);xmem_release(0,0);
    fail_new=1;f=frame(KX_ALLOC,0);f.ecx=1;assert(run(0,0x8000,&f)==8&&active()==0);fail_new=0;
    fail_write=1;assert(run(0,0x8000,&f)==8&&active()==0);fail_write=0;
    /* Cancel a not-yet-published allocation, as End/fault/replacement does. */
    f=frame(KX_ALLOC,0);f.ecx=1024;writes=0;assert(xmem_service(0,0x8000,&f,bounce)==1);assert(writes==KX_ZERO_SLICE&&k386_xmem.pending[0]);
    xmem_release(0,0);assert(!active()&&!k386_xmem.pending[0]);
    k386_xmem.serial=65535;f=frame(KX_ALLOC,0);f.ecx=1;assert(run(0,0x8000,&f)==8);assert(active()==0);
    puts("XMS: isolation, zeroing, bounded progress, quotas, ranges, stale handles, nested ownership and failure cleanup passed");
}
