/* Bounded text snapshot for small real-mode clients; raw typed data is retained. */
#include "resources.h"
static void add(char *row,const char *s){unsigned at=0;while(row[at]&&at<71)at++;while(*s&&at<71)row[at++]=*s++;row[at]=0;}
static void number(char *row,k32 value){char digits[12];unsigned at=11;digits[at]=0;do{digits[--at]='0'+value%10;value/=10;}while(value);add(row,digits+at);}
static void hex(char *row,k32 value){char digits[9];for(unsigned i=0;i<8;i++)digits[i]="0123456789ABCDEF"[(value>>(28-i*4))&15];digits[8]=0;add(row,digits);}
int resource_describe(unsigned id,unsigned age){
    static const char *priorities[]={"Low","Normal","High"};
    static const char *reasons[]={"None","Exited","Fault","Paused","CPU limit","Interrupted","Ended"};
    static const char *events[]={"","Start","Exit","Fault","Pause","Resume","CPU limit","Interrupt","End","Priority","Budget"};
    K386Resources *r=&k386_resources;
    if(id>=K386_SESSIONS||(age&&age>=r->count))return 87;
    K386Usage *u=&r->usage[id];char *row=r->report[0];
    for(unsigned i=0;i<6;i++)r->report[i][0]=0;
    add(row,"ID ");number(row,u->generation);add(row,"  ");add(row,u->program);
    add(row,"  Priority: ");add(row,priorities[u->priority<=K386_HIGH?u->priority:K386_NORMAL]);
    row=r->report[1];add(row,"CPU: ");number(row,resource_ms(u->cpu_ticks));add(row," ms  Runs: ");number(row,u->runs);
    add(row,"  Limit: ");if(u->limit_ticks){number(row,resource_ms(u->limit_ticks));add(row," ms");}else add(row,"off");
    row=r->report[2];add(row,"XMS: ");number(row,u->memory_kb);add(row," KiB  Heap: ");number(row,u->heap_kb);
    add(row," KiB  Handles: ");number(row,u->handles);add(row,"  Depth: ");number(row,u->depth);
    row=r->report[3];add(row,"Reason: ");add(row,reasons[u->reason<=K386_REASON_END?u->reason:0]);
    if(u->last_fault){add(row,"  Fault: ");number(row,u->last_fault);add(row,"  IP: ");hex(row,u->last_eip);add(row,"  CR2: ");hex(row,u->last_cr2);}
    row=r->report[4];
    if(r->count){
        K386Event *e=&r->event[(r->next+K386_EVENTS-1-age)%K386_EVENTS];
        add(row,"Event #");number(row,e->sequence);add(row," DOS ");number(row,e->session+1);
        add(row," ID ");number(row,e->generation);add(row," ");add(row,events[e->type<=K386_EVENT_BUDGET?e->type:0]);
        if(e->fault){add(row," #");number(row,e->fault);add(row," IP ");hex(row,e->eip);}
    }else add(row,"No recorded events");
    add(r->report[5],"P priority / L limit / B break / [ ] history");return 0;
}
