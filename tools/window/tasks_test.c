#include <assert.h>
#include "tasks.h"
int main(void) {
    unsigned a=task_start(0),b=task_start(1),slot;TaskEvent e;
    assert(a&&b&&a!=b);
    for(unsigned n=0;n<TASK_QUEUE;n++)assert(task_post(0,TASK_KEY,n));
    assert(!task_post(0,TASK_KEY,99));assert(task_post(1,TASK_KEY,123));
    assert(task_next(&slot,&e)&&slot==0&&e.value==0);task_complete(slot,0);
    assert(task_next(&slot,&e)&&slot==1&&e.value==123);task_complete(slot,0);
    assert(desktop_tasks[1].state==TASK_WAITING);
    task_stop(0);assert(!task_next(&slot,&e));assert(!task_post(0,TASK_KEY,90));
    task_resume(0);
    for(unsigned n=1;n<TASK_QUEUE;n++){assert(task_next(&slot,&e)&&slot==0&&e.value==n);task_complete(slot,0);}
    assert(!task_next(&slot,&e));assert(desktop_tasks[0].state==TASK_WAITING);
    assert(task_post(0,TASK_KEY,5));assert(task_next(&slot,&e));task_complete(slot,13);
    assert(desktop_tasks[0].state==TASK_FAULTED&&desktop_tasks[0].fault==13);
    assert(!task_post(0,TASK_KEY,9));assert(task_start(0)!=a);assert(task_post(0,TASK_KEY,7));
    task_exit(0);assert(!task_next(&slot,&e));assert(desktop_tasks[0].count==0);
    assert(!task_start(TASK_SLOTS));assert(!task_post(TASK_SLOTS,1,0));
    return 0;
}
