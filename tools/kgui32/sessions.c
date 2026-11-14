/* Real-mode session broker. Commands cannot address another GUI/owner or a
 * recycled slot. Snapshot copies never expose a live monitor pointer. */
#include "gui.h"
#include "document.h"
#include "../window/hardware.h"
#include "../window/k386_client.h"
extern int gui_copy(u32,void *,unsigned,int);
extern int gui_output_range(u32,unsigned);
extern unsigned owned[4],manager,session_focus;
extern u32 owned_generation[4];
static char commands[4][128];
/* Shared wire scratch: snapshot copies never schedule or call DOS. */
extern u8 loader_buffer[4096];
#define view (*(GuiSessionView *)loader_buffer)
_Static_assert(sizeof(GuiSessionView)<=4096,"session wire scratch capacity");
static unsigned live(unsigned s){return s==1||s==2||s==3;}
static int launch(unsigned id,const char *path){
 int e=k386_command(K386_CREATE,id,0,path);if(e)return -e;
 K386Usage u;if(!k386_usage(id,&u)){k386_command(K386_END,id,0,0);return -5;}
 owned[id]=1;owned_generation[id]=u.generation;
 for(unsigned i=0;i<128;i++){commands[id][i]=path[i];if(!path[i])break;}
 return 0;
}
int gui_open_document(unsigned requestor,u32 generation,unsigned work,const char *name){
 char command[64];int e=kg_document_command(work,name,command);if(e)return e;
 if(!manager||requestor>=k386_capacity()||!owned[requestor]||owned_generation[requestor]!=generation)return -116;
 K386Usage owner;if(!k386_usage(requestor,&owner)||owner.generation!=generation||!live(k386_word(requestor,0)))return -116;
 for(unsigned id=0;id<k386_capacity();id++)if(id!=requestor){
  unsigned state=k386_word(id,0);K386Usage u;if(!k386_usage(id,&u))return -5;
  if(live(state)||(state==4&&(!owned[id]||owned_generation[id]!=u.generation)))continue;
  return launch(id,command);
 }
 return -16;
}
int gui_session(u32 pointer,unsigned bytes){
 GuiSessionCommand r;
 if(bytes!=sizeof r)return -22;
 int e=gui_copy(pointer,&r,sizeof r,0);if(e)return e;
 if(r.version!=1||r.size!=sizeof r||r.slot>=k386_capacity()||r.operation<1||r.operation>8||r.operation==7)return -22;
 if(r.operation==GS_FOCUS&&!r.key){session_focus=0;return 0;}
 /* The GUI command editor may receive Escape even with no owned task. This
  * affects GUI input only and grants no monitor operation. F10 always exits. */
 if(r.operation==GS_FOCUS&&r.key==2){session_focus=5;return 0;}
 if(!manager)return -38;
 unsigned id=r.slot,s=k386_word(id,0);K386Usage u;
 if(!k386_usage(id,&u))return -5;
 if(owned[id]&&owned_generation[id]!=u.generation)owned[id]=0;
 if(r.operation==GS_CREATE){
  if(r.generation)return -116;
  unsigned n=0;for(;n<128&&r.path[n];n++)if((u8)r.path[n]<32||(u8)r.path[n]>126)return -22;
  if(!n||n==128)return -22;
  if(live(s))return -16;
  if(s==4&&!owned[id])return -1;
  return launch(id,r.path);
 }
 if(r.generation!=u.generation)return -116;
 if(!owned[id]||owned_generation[id]!=r.generation)return -1;
 unsigned command;
 switch(r.operation){
 case GS_END:command=K386_END;break;
 case GS_PAUSE:command=K386_STOP;break;
 case GS_RESUME:command=K386_RESUME;break;
 case GS_RESTART:if(live(s))return -16;if(!commands[id][0])return -22;return launch(id,commands[id]);
 case GS_FOCUS:if(r.key!=1)return -22;session_focus=id+1;return 0;
 case GS_KEY:
  command=K386_KEY;
  if(r.key==3){command=K386_INTERRUPT;r.key=0;}
  else if(r.key>=256&&r.key<=263){
   if(!k386_kind(id))return -22;
   static const u8 scan[]={0x3a,0x3d,0x3b,0x3c,0x3e,0x3f,0x36,0x37};r.key=(unsigned)scan[r.key-256]<<8;
  }else if(r.key>255)return -22;
  break;
 default:return -22;
 }
 e=k386_command(command,id,r.key,0);return e?-e:0;
}
int gui_snapshot(u32 pointer,unsigned bytes){
 u32 header[4];if(bytes!=sizeof view&&bytes!=sizeof(GuiTerminalView))return -22;
 int e=gui_copy(pointer,header,sizeof header,0);if(e)return e;
 if(header[0]==2){
  if(bytes!=sizeof(GuiTerminalView)||header[1]!=bytes||header[2]>=k386_capacity())return -22;
  if(!manager)return -38;
  if(!gui_output_range(pointer,bytes))return -14;
  unsigned id=header[2];K386Usage u;
  if(!k386_usage(id,&u))return -5;
  if(header[3]!=u.generation)return -116;
  if(!owned[id]||owned_generation[id]!=u.generation)return -1;
  if(k386_kind(id))return -38;
  u32 out[8]={2,bytes,id,u.generation,k386_word(id,6),k386_word(id,0),k386_word(id,36),k386_word(id,38)};
  /* No worker is scheduled while this synchronous read is assembled. Never
   * accept a chunk from another generation/revision, including recycled slots. */
  for(unsigned first=0;first<25;first+=3){unsigned count=25-first;if(count>3)count=3;
   K386TextChunk *chunk=(K386TextChunk *)loader_buffer;
   if(k386_text(id,first,count,chunk))return -5;
   if(chunk->flags!=out[4]||chunk->column!=out[6]||chunk->row!=out[7]||chunk->state!=out[5])return -116;
   e=gui_copy(pointer+32+first*160,chunk->codes,count*160,1);if(e)return e;
   e=gui_copy(pointer+4032+first*160,chunk->attributes,count*160,1);if(e)return e;
  }
  K386Usage after;if(!k386_usage(id,&after)||after.generation!=out[3]||k386_word(id,6)!=out[4])return -116;
  return gui_copy(pointer,out,sizeof out,1);
 }
 if(bytes!=sizeof view)return -22;
 if(header[0]!=1||header[1]!=sizeof view||header[2]>=k386_capacity())return -22;
 unsigned id=header[2];view=(GuiSessionView){.version=1,.size=sizeof view,.slot=id};
 if(manager){
  K386Usage u;if(!k386_usage(id,&u))return -5;
  view.generation=u.generation;view.state=k386_word(id,0);view.kind=k386_kind(id);
  view.exit_code=k386_word(id,2);view.fault=k386_word(id,4);view.counter=k386_counter(id);
  view.column=k386_word(id,36);view.row=k386_word(id,38);view.keys=k386_word(id,40);
  view.text_revision=k386_word(id,6);
  view.cpu_ticks=u.cpu_ticks;view.runs=u.runs;
  view.owned=owned[id]&&owned_generation[id]==u.generation;
  for(unsigned i=0;i<16;i++)view.program[i]=u.program[i];
  view.program[15]=0;
  k386_screen(id,view.screen);
 }
 return gui_copy(pointer,&view,sizeof view,1);
}

int gui_video_read(u32 pointer,unsigned bytes){
 GuiVideoRead r;if(bytes!=sizeof r)return -22;
 int e=gui_copy(pointer,&r,sizeof r,0);if(e)return e;
 if(r.version!=1||r.size!=sizeof r||r.slot>=k386_capacity()||!r.bytes||r.bytes>KV_READ_BYTES)return -22;
 for(unsigned i=0;i<7;i++)if(r.reserved[i])return -22;
 unsigned address;if(r.region?kv_region(r.region,r.offset,r.bytes,&address):(r.offset>128||r.bytes>128-r.offset))return -22;
 if(!manager)return -38;
 if(!gui_output_range(r.data,r.bytes))return -14;
 K386Usage u;unsigned id=r.slot;
 if(!k386_usage(id,&u))return -5;
 if(r.generation!=u.generation||r.revision!=k386_word(id,6))return -116;
 if(!owned[id]||owned_generation[id]!=r.generation)return -1;
 if(k386_kind(id))return -38;
 KvRead *chunk=(KvRead *)loader_buffer;
 if(k386_video(id,r.region,r.offset,r.bytes,chunk))return -38;
 if(chunk->packet.generation!=u.generation||k386_word(id,6)!=r.revision)return -116;
 return gui_copy(r.data,chunk->data,r.bytes,1);
}
