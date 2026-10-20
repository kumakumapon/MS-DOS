/* Pure policy and bounded accounting; no host services or guest-writable state. */
#include "resources.h"
K386Resources k386_resources={.signature={'K','3','8','6','S','Y','S','1'},.version=1,.size=sizeof(K386Resources)};
static int score[K386_SESSIONS];
static unsigned turn;
static k32 next_generation;
void resource_init(void){
    for(unsigned id=0;id<K386_SESSIONS;id++){
        k386_resources.usage[id]=(K386Usage){.priority=K386_NORMAL};score[id]=0;
    }
    turn=0;next_generation=0;
}
/* Smooth weighted round robin. Blocked tasks never accumulate credits. */
int resource_pick(unsigned runnable){
    int chosen=-1,total=0;
    for(unsigned n=0;n<K386_SESSIONS;n++){
        unsigned id=(turn+n)%K386_SESSIONS;
        if(!(runnable&(1u<<id))){score[id]=0;continue;}
        unsigned weight=1u<<k386_resources.usage[id].priority;
        total+=weight;score[id]+=weight;
        if(chosen<0||score[id]>score[chosen])chosen=id;
    }
    if(chosen>=0){score[chosen]-=total;turn=(chosen+1)%K386_SESSIONS;}
    return chosen;
}
int resource_priority(unsigned id,unsigned priority){
    if(id>=K386_SESSIONS||priority>K386_HIGH)return 87;
    k386_resources.usage[id].priority=priority;
    for(unsigned i=0;i<K386_SESSIONS;i++)score[i]=0;
    return 0;
}
int resource_budget(unsigned id,unsigned milliseconds){
    if(id>=K386_SESSIONS||milliseconds>60000)return 87;
    k386_resources.usage[id].limit_ticks=(k32)milliseconds*3072/10;
    return 0;
}
void resource_begin(unsigned id){
    K386Usage *u=&k386_resources.usage[id];
    k32 limit=u->limit_ticks;unsigned priority=u->priority;
    do{if(++next_generation==0)next_generation=1;}while(next_generation==k386_resources.usage[id^1].generation);
    *u=(K386Usage){.generation=next_generation,.limit_ticks=limit,.priority=priority};
    score[id]=0;
}
void resource_charge(unsigned id,k32 elapsed){
    K386Usage *u=&k386_resources.usage[id];
    if(elapsed>~(k32)0-u->cpu_ticks)u->cpu_ticks=~(k32)0;else u->cpu_ticks+=elapsed;
    if(u->runs!=~(k32)0)u->runs++;
}
int resource_exhausted(unsigned id){K386Usage *u=&k386_resources.usage[id];return u->limit_ticks&&u->cpu_ticks>=u->limit_ticks;}
void resource_event(unsigned id,unsigned type,unsigned reason,const K386Frame *f,k32 cr2,unsigned fault){
    K386Resources *r=&k386_resources;K386Usage *u=&r->usage[id];
    if(++r->sequence==0)r->sequence=1;
    K386Event *e=&r->event[r->next];
    *e=(K386Event){.sequence=r->sequence,.generation=u->generation,.cpu_ticks=u->cpu_ticks,
        .eip=f?f->eip:0,.cr2=cr2,.session=id,.type=type,.reason=reason,.fault=fault,
        .psp=u->psp,.cs=f?f->cs:0};
    r->next=(r->next+1)%K386_EVENTS;if(r->count<K386_EVENTS)r->count++;
    if(reason){u->reason=reason;u->last_eip=e->eip;u->last_cr2=cr2;u->last_fault=fault;u->last_cs=e->cs;}
}
_Static_assert(sizeof(K386Usage)==60,"resource usage ABI");
_Static_assert(sizeof(K386Event)==32,"resource journal ABI");
_Static_assert(sizeof(K386Resources)==1084,"resource discovery ABI");
