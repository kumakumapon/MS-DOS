#include "tasks.h"
Task desktop_tasks[TASK_SLOTS];
static unsigned short next_id=1,cursor;
unsigned task_start(unsigned slot) {
    if(slot>=TASK_SLOTS||!next_id)return 0;
    Task *t=&desktop_tasks[slot];
    *t=(Task){0};t->id=next_id++;t->state=TASK_WAITING;
    return t->id;
}
int task_post(unsigned slot,unsigned kind,unsigned value) {
    if(slot>=TASK_SLOTS)return 0;
    Task *t=&desktop_tasks[slot];
    if(!t->id||t->state==TASK_EXITED||t->state==TASK_FAULTED||t->state==TASK_STOPPED||t->count==TASK_QUEUE)return 0;
    unsigned index=(t->head+t->count)%TASK_QUEUE;
    t->events[index]=(TaskEvent){kind,value};t->count++;t->state=TASK_RUNNING;
    return 1;
}
void task_stop(unsigned slot) {
    if(slot<TASK_SLOTS&&desktop_tasks[slot].id&&desktop_tasks[slot].state!=TASK_EXITED)
        desktop_tasks[slot].state=TASK_STOPPED;
}
void task_resume(unsigned slot) {
    if(slot<TASK_SLOTS&&desktop_tasks[slot].state==TASK_STOPPED)
        desktop_tasks[slot].state=desktop_tasks[slot].count?TASK_RUNNING:TASK_WAITING;
}
void task_exit(unsigned slot) {
    if(slot<TASK_SLOTS){desktop_tasks[slot].state=TASK_EXITED;desktop_tasks[slot].head=desktop_tasks[slot].count=0;}
}
int task_next(unsigned *slot,TaskEvent *event) {
    for(unsigned n=0;n<TASK_SLOTS;n++) {
        unsigned at=cursor++%TASK_SLOTS;Task *t=&desktop_tasks[at];
        if(t->state!=TASK_RUNNING||!t->count)continue;
        *slot=at;*event=t->events[t->head];t->head=(t->head+1)%TASK_QUEUE;t->count--;t->dispatched++;
        return 1;
    }
    return 0;
}
void task_complete(unsigned slot,int error) {
    if(slot>=TASK_SLOTS)return;
    Task *t=&desktop_tasks[slot];
    if(error){t->fault=error;t->state=TASK_FAULTED;t->count=0;}
    else if(t->state==TASK_RUNNING&&!t->count)t->state=TASK_WAITING;
}
const char *task_state(unsigned slot) {
    static const char *states[]={"Ended","Running","Waiting","Stopped","Fault"};
    return slot<TASK_SLOTS&&desktop_tasks[slot].state<=TASK_FAULTED?states[desktop_tasks[slot].state]:"Invalid";
}
