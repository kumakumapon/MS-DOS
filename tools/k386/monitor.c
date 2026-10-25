#define K386_MONITOR
#include "runtime.h"
#include "resources.h"
#include "../window/hardware.h"
K386Mailbox k386_mailbox={{'K','3','8','6','A','B','I','1'},1,sizeof(K386Mailbox),1,0,0,0,0,0,{0},{{0}}};
K386Session sessions[2];
k32 monitor_base,page_directory[2],selected,saved_cr0,saved_cr3;
k8 transfer[4096];
static k16 tables;
static k16 ds_segment,owner_psp,indos_segment,indos_offset;
static void usage(unsigned id){
    K386Runtime *r=&runtime[id];K386Usage *u=&k386_resources.usage[id];
    unsigned heap=0,handles=0;
    for(unsigned i=0;i<r->arena.count;i++)if(r->arena.block[i].owner)heap+=r->arena.block[i].size+1;
    for(unsigned i=0;i<K386_HANDLES;i++)if(sessions[id].handles[i])handles++;
    u->memory_kb=r->backing.handle?(K386_BACKING+4095+1023)/1024:0;
    u->heap_kb=(heap+63)/64;u->handles=handles;u->depth=r->process.psp?r->depth+1:0;u->psp=r->process.psp;
    for(unsigned i=0;i<sizeof u->program;i++)u->program[i]=r->process.program[i];
}
static void event(unsigned id,unsigned type,unsigned reason){
    K386View *v=&k386_mailbox.view[id];usage(id);
    resource_event(id,type,reason,&sessions[id].frame,
        type==K386_EVENT_FAULT?v->cr2:0,type==K386_EVENT_FAULT?v->fault:0);
}
static void limit(unsigned id){
    K386View *v=&k386_mailbox.view[id];
    if((v->state==K386_RUNNING||v->state==K386_WAITING)&&resource_exhausted(id)){
        v->state=K386_STOPPED;v->flags++;runtime[id].clock=0;
        event(id,K386_EVENT_LIMIT,K386_REASON_LIMIT);
    }
}
static void step(unsigned id){
    K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    limit(id);
    if(v->state==K386_RUNNING||v->state==K386_WAITING)k386_clock(id);
    if(v->state!=K386_RUNNING)return;
    if(!backing_a20(1)){
        v->state=K386_FAULTED;v->fault=0x8001;
        event(id,K386_EVENT_FAULT,K386_REASON_FAULT);return;
    }
    k32 start=k386_clock_ticks();
    pm_run();backing_a20(0);if(s->request)k386_broker(id);
    resource_charge(id,(k386_clock_ticks()-start)&0xffffff);
    if(v->state==K386_FAULTED)event(id,K386_EVENT_FAULT,K386_REASON_FAULT);
    else if(v->state==K386_ENDED)event(id,K386_EVENT_EXIT,K386_REASON_EXIT);
    k386_reap(id);limit(id);usage(id);
}
void api_dispatch(void) {
    unsigned id=k386_mailbox.session;k386_mailbox.error=0;
    if(k386_mailbox.reserved||mem_get(indos_segment,indos_offset)){k386_mailbox.error=5;return;}
    k16 msw;unsigned char mask;
    __asm__ volatile("smsw %0; inb $2,%1":"=r"(msw),"=a"(mask));
    if((msw&1)||!(mask&1)){k386_mailbox.error=5;return;}

    if(id>=2){k386_mailbox.error=87;return;}
    k386_mailbox.reserved=1; /* Publish views only after the whole API returns. */
    unsigned caller,service=0x6200;
    __asm__ volatile("int $0x21":"+a"(service),"=b"(caller)::"cc","memory");
    service=0x5000;__asm__ volatile("int $0x21":"+a"(service):"b"((unsigned)owner_psp):"cc","memory");
    selected=id;K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    switch(k386_mailbox.command) {
    case K386_CREATE:
        k386_mailbox.error=k386_start(id,k386_mailbox.path);
        if(!k386_mailbox.error){resource_begin(id);event(id,K386_EVENT_START,K386_REASON_NONE);}
        usage(id);break;
    case K386_SCHEDULE: {
        /* DOS brokers (DIR/TYPE/EXEC) used to cost one entire desktop turn
           each. Reconsider all runnable sessions between bounded slices, so
           I/O-heavy shells progress without starving CPU loops or the GUI. */
        /* key=0 retains the original one-slice public API. The desktop
           explicitly requests up to eight slices; reject unbounded requests. */
        if(k386_mailbox.key>8){k386_mailbox.error=87;break;}
        unsigned slices=k386_mailbox.key?k386_mailbox.key:1;
        k32 start=k386_clock_ticks();
        for(unsigned batch=0;batch<slices;batch++) {
            unsigned runnable=0;
            for(unsigned i=0;i<K386_SESSIONS;i++){
                limit(i);unsigned state=k386_mailbox.view[i].state;
                if(state==K386_RUNNING||state==K386_WAITING)k386_clock(i);
                if(k386_mailbox.view[i].state==K386_RUNNING)runnable|=1u<<i;
            }
            int chosen=resource_pick(runnable);k386_mailbox.session=chosen<0?0xffff:chosen;
            if(chosen<0)break;
            selected=chosen;step(chosen);
            if(((k386_clock_ticks()-start)&0xffffff)>=3072)break;
        }break;
    }
    case K386_STEP:step(id);break;
    case K386_KEY:
        if(s->count==K386_KEY_CAPACITY){k386_mailbox.error=8;break;}
        s->keys[(s->head+s->count++)%K386_KEY_CAPACITY]=k386_mailbox.key;
        if(v->state==K386_WAITING)v->state=K386_RUNNING;
        break;
    case K386_STOP:if(v->state==K386_RUNNING||v->state==K386_WAITING){v->state=K386_STOPPED;runtime[id].clock=0;event(id,K386_EVENT_PAUSE,K386_REASON_PAUSE);}break;
    case K386_RESUME:
        if(v->state==K386_STOPPED){
            if(resource_exhausted(id)){k386_mailbox.error=5;break;}
            v->state=K386_RUNNING;runtime[id].clock=0;event(id,K386_EVENT_RESUME,K386_REASON_NONE);
        }break;
    case K386_END:event(id,K386_EVENT_END,K386_REASON_END);k386_release(id);v->state=K386_ENDED;usage(id);break;
    case K386_PRIORITY:
        k386_mailbox.error=resource_priority(id,k386_mailbox.key==0xffff?(k386_resources.usage[id].priority+1)%3:k386_mailbox.key);
        if(!k386_mailbox.error)event(id,K386_EVENT_PRIORITY,K386_REASON_NONE);
        break;
    case K386_BUDGET:
        if(k386_mailbox.key==0xffff){
            unsigned ms=resource_ms(k386_resources.usage[id].limit_ticks);
            k386_mailbox.key=!ms?1000:ms<5000?5000:ms<10000?10000:0;
        }
        k386_mailbox.error=resource_budget(id,k386_mailbox.key);
        if(!k386_mailbox.error){event(id,K386_EVENT_BUDGET,K386_REASON_NONE);limit(id);}break;
    case K386_INTERRUPT:
        if(v->state!=K386_RUNNING&&v->state!=K386_WAITING&&v->state!=K386_STOPPED){k386_mailbox.error=5;break;}
        event(id,K386_EVENT_INTERRUPT,K386_REASON_INTERRUPT);
        s->head=s->count=s->line_count=s->request=0;v->state=K386_ENDED;v->exit_code=0x103;
        k386_reap(id);limit(id);usage(id);v->flags++;break;
    case K386_DESCRIBE:k386_mailbox.error=resource_describe(id,k386_mailbox.key);break;
    default:k386_mailbox.error=1;
    }
    service=0x5000;__asm__ volatile("int $0x21":"+a"(service):"b"(caller):"cc","memory");
    k386_mailbox.reserved=0;
}
int monitor_init(void) {
    __asm__ volatile("movw %%ds,%0":"=r"(ds_segment));monitor_base=(k32)ds_segment*16;
    unsigned service=0x6200,psp;
    __asm__ volatile("int $0x21":"+a"(service),"=b"(psp)::"cc","memory");owner_psp=psp;
    if(monitor_base+0x10000>=0x80000)return 0;
    unsigned a=0x3400,b; k16 seg;
    __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"cc","memory");
    indos_segment=seg;indos_offset=b;
    if(!backing_init())return 0;
    resource_init();
    tables=mem_alloc(28672);if(!tables)return 0;
    k32 at=((k32)tables*16+4095)&~4095u;
    page_directory[0]=at;page_directory[1]=at+12288;
    for(int id=0;id<2;id++){runtime[id].process.vif=1;runtime[id].process.dta=0x80000080;for(int i=0;i<2000;i++)k386_mailbox.view[id].screen[i]=' ';}
    return 1;
}
_Static_assert(sizeof(K386Frame)==76,"interrupt frame ABI");
_Static_assert(sizeof(K386Session)==256,"assembly session stride");
