#ifndef K386_RESOURCES_H
#define K386_RESOURCES_H
#include "abi.h"
enum { K386_LOW, K386_NORMAL, K386_HIGH };
enum { K386_REASON_NONE, K386_REASON_EXIT, K386_REASON_FAULT, K386_REASON_PAUSE,
       K386_REASON_LIMIT, K386_REASON_INTERRUPT, K386_REASON_END };
enum { K386_EVENT_START=1, K386_EVENT_EXIT, K386_EVENT_FAULT, K386_EVENT_PAUSE,
       K386_EVENT_RESUME, K386_EVENT_LIMIT, K386_EVENT_INTERRUPT, K386_EVENT_END,
       K386_EVENT_PRIORITY, K386_EVENT_BUDGET };
enum { K386_EVENTS=16, K386_ARTIC_SECOND=307200 };
typedef struct {
    k32 generation,cpu_ticks,runs,limit_ticks;
    k16 priority,reason,memory_kb,heap_kb,handles,depth,psp,reserved;
    k32 last_eip,last_cr2;
    k16 last_fault,last_cs;
    char program[16];
} K386Usage;
typedef struct {
    k32 sequence,generation,cpu_ticks,eip,cr2;
    k16 session,type,reason,fault,psp,cs;
} K386Event;
typedef struct {
    char signature[8];k16 version,size;
    k32 sequence;k16 next,count;
    K386Usage usage[K386_SESSIONS];
    K386Event event[K386_EVENTS];
    char report[6][72];
} K386Resources;
extern K386Resources k386_resources;
void resource_init(void);
int resource_pick(unsigned runnable);
int resource_priority(unsigned id,unsigned priority);
int resource_budget(unsigned id,unsigned milliseconds);
void resource_begin(unsigned id);
void resource_charge(unsigned id,k32 elapsed);
int resource_exhausted(unsigned id);
void resource_event(unsigned id,unsigned type,unsigned reason,const K386Frame *frame,k32 cr2,unsigned fault);
int resource_describe(unsigned id,unsigned age);
static inline k32 resource_ms(k32 ticks){return ticks/3072*10+(ticks%3072)*10/3072;}
#endif
