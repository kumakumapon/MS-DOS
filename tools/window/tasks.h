#ifndef KDESK_TASKS_H
#define KDESK_TASKS_H
#define TASK_SLOTS 12
#define TASK_QUEUE 8
enum { TASK_EXITED, TASK_RUNNING, TASK_WAITING, TASK_STOPPED, TASK_FAULTED };
enum { TASK_KEY=1, TASK_TICK };
typedef struct { unsigned short kind,value; } TaskEvent;
typedef struct {
    unsigned short id,state,head,count,fault;
    unsigned long dispatched;
    TaskEvent events[TASK_QUEUE];
} Task;
extern Task desktop_tasks[TASK_SLOTS];
unsigned task_start(unsigned slot);
int task_post(unsigned slot,unsigned kind,unsigned value);
void task_stop(unsigned slot);
void task_resume(unsigned slot);
void task_exit(unsigned slot);
int task_next(unsigned *slot,TaskEvent *event);
void task_complete(unsigned slot,int error);
const char *task_state(unsigned slot);
#endif
