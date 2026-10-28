/* Locked XMS belongs to the resident monitor. Guest code never calls this API. */
#include "backing.h"
static k32 entry;
static unsigned call(unsigned function,unsigned *bx,unsigned *dx,unsigned si) {
    unsigned a=function<<8,b=*bx,d=*dx,c=0;
    __asm__ volatile("pushw %%ds; pushw %%es; lcallw *%5; popw %%es; popw %%ds"
        :"+a"(a),"+b"(b),"+d"(d),"+S"(si),"+c"(c):"m"(entry):"edi","cc","memory");
    *bx=b&65535;*dx=d&65535;return a&65535;
}
int backing_init(void) {
    unsigned a=0x4300,b; k16 seg;
    __asm__ volatile("int $0x2f":"+a"(a)::"ebx","ecx","edx","cc","memory");
    if((a&255)!=128)return 0;
    a=0x4310;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"edx","cc","memory");
    entry=(k32)seg<<16|(b&65535);return entry!=0;
}
int backing_a20(int on){unsigned b=0,d=0;return entry&&call(on?5:6,&b,&d,0)==1;}
void backing_free(K386Backing *p) {
    if(p->handle){unsigned b=0,d=p->handle;call(13,&b,&d,0);d=p->handle;call(10,&b,&d,0);}
    *p=(K386Backing){0};
}
int backing_new(K386Backing *p) {
    unsigned b=0,d=(K386_BACKING+4095+1023)/1024;
    *p=(K386Backing){0};if(call(9,&b,&d,0)!=1)return 0;
    k16 handle=d;p->handle=handle;
    if(call(12,&b,&d,0)!=1){unsigned h=handle;call(10,&b,&h,0);*p=(K386Backing){0};return 0;}
    k32 raw=(k32)d<<16|b;p->physical=(raw+4095)&~4095u;p->offset=p->physical-raw;
    if(raw<0x100000||p->physical>0x1000000-K386_BACKING){backing_free(p);return 0;}
    return 1;
}
static int move(k16 from,k32 at,k16 to,k32 target,unsigned n) {
    struct __attribute__((packed)) {k32 n;k16 from;k32 at;k16 to;k32 target;} m={n,from,at,to,target};
    unsigned b=0,d=0;return call(11,&b,&d,(unsigned)&m)==1;
}
static k32 local(const void *p){k16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));return (k32)ds<<16|((unsigned)p&65535);}
int backing_read(const K386Backing *p,k32 at,void *out,unsigned n) {
    if(!p->handle||at>K386_BACKING||n>K386_BACKING-at)return 0;
    unsigned even=n&~1u;
    if(even&&!move(p->handle,p->offset+at,0,local(out),even))return 0;
    if(n&1){k8 tail[2];if(!move(p->handle,p->offset+at+even,0,local(tail),2))return 0;((k8 *)out)[even]=tail[0];}
    return 1;
}
int backing_write(const K386Backing *p,k32 at,const void *in,unsigned n) {
    if(!p->handle||at>K386_BACKING||n>K386_BACKING-at)return 0;
    unsigned even=n&~1u;
    if(even&&!move(0,local(in),p->handle,p->offset+at,even))return 0;
    if(n&1){k8 tail[2];if(!move(p->handle,p->offset+at+even,0,local(tail),2))return 0;tail[0]=((const k8 *)in)[even];if(!move(0,local(tail),p->handle,p->offset+at+even,2))return 0;}
    return 1;
}
int backing_copy(const K386Backing *p,k32 to,k32 from,unsigned count){return to<=K386_BACKING&&from<=K386_BACKING&&count<=K386_BACKING-to&&count<=K386_BACKING-from&&!(count&1)&&move(p->handle,p->offset+from,p->handle,p->offset+to,count);}
int backing_zero(const K386Backing *p,k32 at,k32 count) {
    static const k8 zero[1024]={0};
    while(count){unsigned n=count>sizeof zero?sizeof zero:count;if(!backing_write(p,at,zero,n))return 0;at+=n;count-=n;}
    return 1;
}

/* Application blocks stay unlocked and are reachable only through the broker. */
#include "xmem.h"
int xmem_new(k16 kb,k16 *handle){unsigned b=0,d=kb;if(call(9,&b,&d,0)!=1)return 0;*handle=d;return 1;}
int xmem_free(k16 handle){unsigned b=0,d=handle;return call(10,&b,&d,0)==1;}
int xmem_read(k16 handle,k32 at,void *out,unsigned n){
    unsigned even=n&~1u;if(even&&!move(handle,at,0,local(out),even))return 0;
    if(n&1){k8 tail[2];k32 pos=at+even;if(!move(handle,pos&~1u,0,local(tail),2))return 0;((k8 *)out)[even]=tail[pos&1];}
    return 1;
}
int xmem_write(k16 handle,k32 at,const void *in,unsigned n){
    unsigned even=n&~1u;if(even&&!move(0,local(in),handle,at,even))return 0;
    if(n&1){k8 tail[2];k32 pos=at+even;if(!move(handle,pos&~1u,0,local(tail),2))return 0;tail[pos&1]=((const k8 *)in)[even];if(!move(0,local(tail),handle,pos&~1u,2))return 0;}
    return 1;
}
