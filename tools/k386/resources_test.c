#include <assert.h>
#include <stdio.h>
#include "resources.h"
int main(void){
    resource_init();
    unsigned hits[2]={0},last=2,streak=0;
    assert(resource_priority(0,K386_HIGH)==0&&resource_priority(1,K386_LOW)==0);
    for(int n=0;n<1000;n++){
        int id=resource_pick(3);assert(id>=0&&id<2);hits[id]++;
        streak=(unsigned)id==last?streak+1:1;last=id;assert(streak<=4);
    }
    assert(hits[0]==800&&hits[1]==200);
    for(int n=0;n<1000;n++)assert(resource_pick(2)==1);
    assert(resource_pick(0)==-1);
    assert(resource_priority(0,3)==87&&resource_priority(2,0)==87);
    assert(resource_priority(0,K386_NORMAL)==0&&resource_priority(1,K386_NORMAL)==0);
    hits[0]=hits[1]=0;for(int n=0;n<100;n++)hits[resource_pick(3)]++;
    assert(hits[0]==50&&hits[1]==50);
    assert(resource_budget(0,60001)==87&&resource_budget(2,0)==87);
    assert(resource_budget(0,1000)==0);resource_begin(0);
    resource_charge(0,K386_ARTIC_SECOND-1);assert(!resource_exhausted(0));
    resource_charge(0,1);assert(resource_exhausted(0));
    assert(resource_ms(k386_resources.usage[0].cpu_ticks)==1000);
    assert(!resource_exhausted(1));
    assert(resource_budget(0,2000)==0&&!resource_exhausted(0));
    resource_charge(0,~(k32)0);assert(k386_resources.usage[0].cpu_ticks==~(k32)0&&resource_exhausted(0));
    assert(resource_budget(0,0)==0&&!resource_exhausted(0));
    unsigned generation=k386_resources.usage[0].generation;
    resource_begin(0);assert(k386_resources.usage[0].generation==generation+1&&k386_resources.usage[0].cpu_ticks==0);
    K386Frame frame={.cs=0x8000,.eip=0x123};
    for(unsigned n=0;n<25;n++)resource_event(n%2,K386_EVENT_FAULT,K386_REASON_FAULT,&frame,0xe0000,14);
    assert(k386_resources.count==16&&k386_resources.next==9&&k386_resources.sequence==25);
    K386Event *e=&k386_resources.event[8];
    assert(e->sequence==25&&e->session==0&&e->generation==generation+1&&e->fault==14&&e->cr2==0xe0000&&e->cs==0x8000&&e->eip==0x123);
    assert(k386_resources.usage[0].last_fault==14);
    resource_begin(1);assert(k386_resources.usage[0].generation!=k386_resources.usage[1].generation);
    puts("K386 resource policy: fairness, no starvation/idle credits, quotas, saturation, generations and journal PASS");
}
