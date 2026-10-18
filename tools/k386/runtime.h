#ifndef K386_RUNTIME_H
#define K386_RUNTIME_H
#include "abi.h"
#include "backing.h"
#include "arena.h"
typedef struct {
    K386Frame frame;
    k32 dta;
    k16 psp,vif,last_exit,search_valid,handles[K386_HANDLES];
    k8 search[21];
} K386Process;
typedef struct {
    K386Backing backing;
    K386Arena arena;
    K386Process process,parent[K386_DEPTH-1];
    k16 depth;
    char cwd[128];
    k32 clock,accumulator,timer_count;
    k16 timer_owner,timer_armed,timer_pending;
} K386Runtime;
extern K386Runtime runtime[2];
typedef struct {k32 number,ax,bx,cx,dx,ds,es,cs,ip;} K386Call;
/* Physical extended RAM is not in NP2's legacy main-RAM debug buffer. These
   bounded snapshots come from the CPU's active mapping, never from the loader. */
typedef struct {
    char signature[8];k16 version,size;k32 count[2];K386Call calls[2][16];
    K386Frame frame[2];k8 memory[2][512];
} K386Diagnostics;
extern K386Diagnostics k386_diagnostics;
int k386_start(unsigned id,const char *command);
void k386_broker(unsigned id);
void k386_reap(unsigned id);
void k386_release(unsigned id);
void k386_clock(unsigned id);
int kernel_interrupt(K386Frame *f,k16 cs,k16 ip);
#endif
