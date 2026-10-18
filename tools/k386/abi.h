#ifndef K386_ABI_H
#define K386_ABI_H
typedef unsigned char k8;
typedef unsigned short k16;
typedef unsigned int k32;
enum { K386_SESSIONS=2, K386_SCREEN=2000, K386_SEGMENT=0x8000 };
enum { K386_ENDED, K386_RUNNING, K386_WAITING, K386_STOPPED, K386_FAULTED };
enum { K386_CREATE=1,K386_STEP,K386_KEY,K386_STOP,K386_RESUME,K386_END };
typedef struct {
    k32 edi,esi,ebp,ignored,ebx,edx,ecx,eax,vector,error,eip,cs,eflags,esp,ss,es,ds,fs,gs;
} K386Frame;
typedef struct {
    k16 state,exit_code,fault,flags;
    k32 ticks,cr3,physical,eip,eflags,cr2,counter;
    k16 column,row,keys,pad;
    char screen[K386_SCREEN];
} K386View;
typedef struct {
    char signature[8];k16 version,size,ready,error,command,session,key,reserved;
    char path[128];K386View view[K386_SESSIONS];
} K386Mailbox;
#ifdef K386_MONITOR
typedef struct {
    K386Frame frame;
    k32 physical,pd;
    k16 allocation,request,handles[8],line_count;
    k8 keys[32];k16 head,count;
} K386Session;
extern K386Mailbox k386_mailbox;
extern K386Session sessions[2];
extern k32 monitor_base,page_directory[2],selected,saved_cr0,saved_cr3;
extern k8 transfer[4096];
void pm_run(void);
int kernel_trap(K386Frame *frame);
void kernel_finish(void);
#endif
#endif
