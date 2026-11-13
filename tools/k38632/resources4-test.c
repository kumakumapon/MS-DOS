#include <assert.h>
#include "../k386/resources.h"
int main(void){
 resource_init();assert(resource_slots==2&&resource_priority(3,1)==87);
 resource_slots=4;unsigned hits[4]={0};
 for(unsigned i=0;i<4;i++){assert(!resource_priority(i,i==2?2:i&1?1:0));resource_begin(i);}
 for(unsigned n=0;n<900;n++){int id=resource_pick(15);assert(id>=0&&id<4);hits[id]++;}
 assert(hits[0]==100&&hits[1]==200&&hits[2]==400&&hits[3]==200);
 for(unsigned n=0;n<100;n++)assert(resource_pick(8)==3);
 assert(resource_pick(0)==-1);assert(!resource_budget(2,1));resource_charge(2,308);assert(resource_exhausted(2)&&!resource_exhausted(3));
 k32 old[4];for(unsigned i=0;i<4;i++)old[i]=resource_usage(i)->generation;
 resource_begin(3);for(unsigned i=0;i<4;i++)assert(resource_usage(3)->generation!=old[i]);
 for(unsigned i=0;i<3;i++)assert(resource_usage(i)->generation==old[i]);
 K386Frame f={.cs=0x1b,.eip=0x801234};resource_event(3,K386_EVENT_FAULT,K386_REASON_FAULT,&f,0x10000,14);
 assert(k386_resources.event[0].session==3&&k386_resources.event[0].generation==resource_usage(3)->generation&&resource_usage(3)->last_fault==14);
 assert(resource_usage(0)==&k386_resources.usage[0]&&resource_usage(1)==&k386_resources.usage[1]&&sizeof(K386Resources)==1084);
 return 0;
}
