/* Exercise broker authorization and transaction policy with a monitor stub.
 * Real CPU, XMS and FAT coverage belongs to the WebNP2 browser suite. */
#include <assert.h>
#include <string.h>
#include "gui.h"
#include "../window/k386_client.h"
unsigned owned[2],manager=1,session_focus;u32 owned_generation[2];
static GuiSessionCommand request;static GuiSessionView snapshot;
static unsigned states[2],generation[2],kinds[2],last_command,last_key,calls,fail;
static char launched[128];
extern int gui_session(u32,unsigned),gui_snapshot(u32,unsigned);
int gui_copy(u32 p,void *v,unsigned n,int write){
 if(p==99)return -14;
 if(write){assert(n==sizeof snapshot);memcpy(&snapshot,v,n);}
 else if(n==16){u32 h[4]={1,sizeof snapshot,request.slot,0};memcpy(v,h,n);}
 else {assert(n==sizeof request);memcpy(v,&request,n);}return 0;
}
int k386_usage(unsigned id,K386Usage *u){memset(u,0,sizeof *u);u->generation=generation[id];return 1;}
unsigned k386_word(unsigned id,unsigned o){return o?0:states[id];}
unsigned k386_kind(unsigned id){return kinds[id];}
unsigned k386_counter(unsigned id){return id+42;}
void k386_screen(unsigned id,char *s){memset(s,'A'+id,2000);}
int k386_command(unsigned c,unsigned id,unsigned k,const char *p){
 calls++;last_command=c;last_key=k;if(fail)return fail;
 if(c==K386_CREATE){assert(p);strcpy(launched,p);generation[id]++;states[id]=1;}
 if(c==K386_END)states[id]=0;
 if(c==K386_STOP)states[id]=3;
 if(c==K386_RESUME)states[id]=1;
 return 0;
}
static int op(unsigned n){request.operation=n;return gui_session(1,sizeof request);}
int main(void){
 request=(GuiSessionCommand){.version=1,.size=sizeof request,.operation=GS_CREATE};strcpy(request.path,"COUNT32.EXE 1");
 assert(gui_session(99,sizeof request)==-14);assert(gui_session(1,sizeof request-1)==-22);
 request.slot=2;assert(op(GS_CREATE)==-22);request.slot=0;
 assert(op(7)==-22&&calls==0);
 request.version=2;assert(op(GS_CREATE)==-22);request.version=1;
 manager=0;assert(op(GS_CREATE)==-38);manager=1;
 states[0]=1;assert(op(GS_CREATE)==-16&&calls==0);
 request.generation=1;assert(op(GS_END)==-116&&calls==0);
 request.generation=0;assert(op(GS_END)==-1&&calls==0);
 states[0]=4;assert(op(GS_CREATE)==-1);states[0]=0;
 request.path[0]=0;assert(op(GS_CREATE)==-22);memset(request.path,'X',128);assert(op(GS_CREATE)==-22);
 strcpy(request.path,"COUNT32.EXE 1");fail=2;assert(op(GS_CREATE)==-2&&!owned[0]);fail=0;
 assert(op(GS_CREATE)==0&&owned[0]&&owned_generation[0]==1);request.generation=1;
 request.key=1;assert(op(GS_FOCUS)==0&&session_focus==1);
 assert(op(GS_PAUSE)==0&&states[0]==3);assert(op(GS_RESUME)==0&&states[0]==1);
 request.key=256;assert(op(GS_KEY)==-22);kinds[0]=1;assert(op(GS_KEY)==0&&last_key==0x3a00);
 request.key=3;assert(op(GS_KEY)==0&&last_command==K386_INTERRUPT);
 unsigned count=calls;request.generation=0;assert(op(GS_END)==-116&&calls==count);request.generation=1;
 assert(op(GS_RESTART)==-16);assert(op(GS_END)==0);assert(op(GS_RESTART)==0&&!strcmp(launched,"COUNT32.EXE 1"));
 assert(generation[0]==2&&owned_generation[0]==2);
 request.generation=1;assert(op(GS_KEY)==-116);request.generation=2;
 assert(gui_snapshot(1,sizeof snapshot)==0&&snapshot.owned&&snapshot.generation==2&&snapshot.counter==42&&snapshot.screen[1999]=='A');
 generation[0]=3;assert(op(GS_END)==-116&&!owned[0]);request.generation=3;
 assert(op(GS_PAUSE)==-1);assert(gui_snapshot(1,sizeof snapshot)==0&&!snapshot.owned);
 request.key=2;assert(op(GS_FOCUS)==0&&session_focus==3);
 request.key=0;assert(op(GS_FOCUS)==0&&!session_focus);
 return 0;
}
