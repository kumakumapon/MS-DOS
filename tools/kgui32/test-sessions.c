/* Exercise broker authorization and transaction policy with a monitor stub.
 * Real CPU, XMS and FAT coverage belongs to the WebNP2 browser suite. */
#include <assert.h>
#include <string.h>
#include "gui.h"
#include "../window/k386_client.h"
u8 loader_buffer[4096];
unsigned owned[4],manager=1,session_focus;u32 owned_generation[4];
static GuiSessionCommand request;static GuiSessionView snapshot;
static GuiTerminalView terminal_request,terminal_result;static unsigned terminal_test,bad_chunk;
static unsigned states[4],generation[4],kinds[4],last_command,last_key,calls,fail;
static char launched[128];
static unsigned capacity=2;
unsigned k386_capacity(void){return capacity;}
extern int gui_session(u32,unsigned),gui_snapshot(u32,unsigned),gui_open_document(unsigned,u32,unsigned,const char *);
int gui_copy(u32 p,void *v,unsigned n,int write){
 if(p==99)return -14;
 if(terminal_test){if(!write){assert(n==16);memcpy(v,&terminal_request,n);}else{assert(p>=1&&p-1+n<=sizeof terminal_result);memcpy((char *)&terminal_result+p-1,v,n);}return 0;}
 if(write){assert(n==sizeof snapshot);memcpy(&snapshot,v,n);}
 else if(n==16){u32 h[4]={1,sizeof snapshot,request.slot,0};memcpy(v,h,n);}
 else {assert(n==sizeof request);memcpy(v,&request,n);}return 0;
}
int gui_output_range(u32 p,unsigned n){return p!=201&&n==sizeof(GuiTerminalView);}
int k386_video(unsigned a,unsigned b,unsigned c,unsigned d,KvRead *e){(void)a;(void)b;(void)c;(void)d;(void)e;return 50;}
int k386_text(unsigned id,unsigned first,unsigned count,K386TextChunk *t){
 memset(t,0,sizeof *t);t->flags=bad_chunk;t->state=states[id];for(unsigned i=0;i<count*80;i++){t->codes[i]='A'+id;t->attributes[i]=0xe1;}assert(first<25);return 0;
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
 request.key=2;assert(op(GS_FOCUS)==0&&session_focus==5);
 request.key=0;assert(op(GS_FOCUS)==0&&!session_focus);
 owned[0]=1;owned_generation[0]=generation[0]=9;states[0]=1;owned[1]=0;generation[1]=4;states[1]=2;
 count=calls;assert(gui_open_document(0,9,1,"NOTE.TXT")==-16&&calls==count&&states[1]==2);
 states[1]=3;assert(gui_open_document(0,9,1,"NOTE.TXT")==-16&&calls==count);
 states[1]=4;assert(gui_open_document(0,9,1,"NOTE.TXT")==-16&&calls==count);
 states[1]=0;assert(gui_open_document(0,8,1,"NOTE.TXT")==-116&&calls==count);
 assert(gui_open_document(0,9,2,"NOTE.TXT")==-22&&calls==count);
 assert(gui_open_document(0,9,1,"../NOTE.TXT")==-22&&calls==count);
 assert(!gui_open_document(0,9,1,"note.txt")&&!strcmp(launched,"KGEDIT.EXE work NOTE.TXT")&&owned[1]&&owned_generation[1]==5);
 assert(states[0]==1&&generation[0]==9);states[1]=0;
 assert(!gui_open_document(0,9,0,"A.TXT")&&!strcmp(launched,"KGEDIT.EXE root A.TXT"));
 owned[0]=0;count=calls;assert(gui_open_document(0,9,1,"NOTE.TXT")==-116&&calls==count);
 terminal_test=1;terminal_request=(GuiTerminalView){.version=2,.size=sizeof terminal_request,.slot=0,.generation=9};
 owned[0]=1;kinds[0]=0;states[0]=2;assert(!gui_snapshot(1,sizeof terminal_request));assert(terminal_result.version==2&&terminal_result.codes[1999]=='A'&&terminal_result.attributes[1999]==0xe1);
 terminal_request.generation=8;assert(gui_snapshot(1,sizeof terminal_request)==-116);terminal_request.generation=9;
 owned[0]=0;assert(gui_snapshot(1,sizeof terminal_request)==-1);owned[0]=1;kinds[0]=1;assert(gui_snapshot(1,sizeof terminal_request)==-38);kinds[0]=0;
 assert(gui_snapshot(201,sizeof terminal_request)==-14);bad_chunk=1;assert(gui_snapshot(1,sizeof terminal_request)==-116);bad_chunk=0;
 terminal_request.version=3;assert(gui_snapshot(1,sizeof terminal_request)==-22);terminal_request.version=2;terminal_request.slot=2;assert(gui_snapshot(1,sizeof terminal_request)==-22);terminal_request.slot=0;
 capacity=4;terminal_test=0;memset(states,0,sizeof states);memset(owned,0,sizeof owned);
 request=(GuiSessionCommand){.version=1,.size=sizeof request,.slot=3,.operation=GS_CREATE};strcpy(request.path,"COUNT32.EXE 4");
 assert(!op(GS_CREATE)&&owned[3]&&owned_generation[3]==1);request.generation=1;request.key=1;
 assert(!op(GS_FOCUS)&&session_focus==4);request.key=2;assert(!op(GS_FOCUS)&&session_focus==5);
 request.key=0;assert(!op(GS_PAUSE)&&states[3]==3);request.generation=0;count=calls;assert(op(GS_END)==-116&&calls==count&&states[3]==3);
 request.generation=1;assert(!op(GS_END));assert(!op(GS_RESTART)&&generation[3]==2);request.generation=1;assert(op(GS_KEY)==-116);
 owned[0]=1;owned_generation[0]=generation[0]=99;states[0]=states[1]=states[2]=1;owned[1]=owned[2]=0;
 count=calls;assert(gui_open_document(0,99,1,"A.TXT")==-16&&calls==count);states[3]=0;
 assert(!gui_open_document(0,99,1,"A.TXT")&&owned[3]&&generation[3]==3&&states[1]==1&&states[2]==1&&!owned[1]&&!owned[2]);
 request.slot=4;assert(op(GS_CREATE)==-22);
 terminal_test=1;terminal_request.slot=0;manager=0;assert(gui_snapshot(1,sizeof terminal_request)==-38);
 return 0;
}
