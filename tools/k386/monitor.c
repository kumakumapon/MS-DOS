#define K386_MONITOR
#include "runtime.h"
#include "../window/hardware.h"
K386Mailbox k386_mailbox={{'K','3','8','6','A','B','I','1'},1,sizeof(K386Mailbox),1,0,0,0,0,0,{0},{{0}}};
K386Session sessions[2];
k32 monitor_base,page_directory[2],selected,saved_cr0,saved_cr3;
k8 transfer[4096];
static k8 pages[20480];
static k16 ds_segment,indos_segment,indos_offset;
void api_dispatch(void) {
    unsigned id=k386_mailbox.session;k386_mailbox.error=0;
    if(mem_get(indos_segment,indos_offset)){k386_mailbox.error=5;return;}
    k16 msw;unsigned char mask;
    __asm__ volatile("smsw %0; inb $2,%1":"=r"(msw),"=a"(mask));
    if((msw&1)||!(mask&1)){k386_mailbox.error=5;return;}

    if(id>=2){k386_mailbox.error=87;return;}
    unsigned caller,service=0x6200;
    __asm__ volatile("int $0x21":"+a"(service),"=b"(caller)::"cc","memory");
    service=0x5000;__asm__ volatile("int $0x21":"+a"(service):"b"((unsigned)ds_segment):"cc","memory");
    selected=id;K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    switch(k386_mailbox.command) {
    case K386_CREATE:k386_mailbox.error=k386_start(id,k386_mailbox.path);break;
    case K386_STEP:
        if(v->state!=K386_RUNNING)break;
        pm_run();if(s->request)k386_broker(id);k386_reap(id);
        break;
    case K386_KEY:
        if(s->count==K386_KEY_CAPACITY){k386_mailbox.error=8;break;}
        s->keys[(s->head+s->count++)%K386_KEY_CAPACITY]=k386_mailbox.key;
        if(v->state==K386_WAITING)v->state=K386_RUNNING;
        break;
    case K386_STOP:if(v->state==K386_RUNNING||v->state==K386_WAITING)v->state=K386_STOPPED;break;
    case K386_RESUME:if(v->state==K386_STOPPED)v->state=K386_RUNNING;break;
    case K386_END:k386_release(id);v->state=K386_ENDED;break;
    default:k386_mailbox.error=1;
    }
    service=0x5000;__asm__ volatile("int $0x21":"+a"(service):"b"(caller):"cc","memory");
}
int monitor_init(void) {
    __asm__ volatile("movw %%ds,%0":"=r"(ds_segment));monitor_base=(k32)ds_segment*16;
    if(monitor_base+0x10000>=0x80000)return 0;
    unsigned a=0x3400,b; k16 seg;
    __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"cc","memory");
    indos_segment=seg;indos_offset=b;
    k32 at=(monitor_base+(unsigned)pages+4095)&~4095u;
    page_directory[0]=at;page_directory[1]=at+8192;
    for(int id=0;id<2;id++){runtime[id].vif=1;runtime[id].dta=0x80000080;for(int i=0;i<2000;i++)k386_mailbox.view[id].screen[i]=' ';}
    return 1;
}
_Static_assert(sizeof(K386Frame)==76,"interrupt frame ABI");
_Static_assert(sizeof(K386Session)==240,"assembly session stride");
