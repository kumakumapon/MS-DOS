#define K386_MONITOR
#include "runtime.h"
#include "resources.h"
#include "video.h"
int k386_video_call(unsigned id,KvPacket *p){
 K386Backing *backing=&runtime[id].backing;
 if(!backing->handle||(backing->pages&255)<0xe8)return -38;
 p->version=1;p->size=sizeof *p;p->slot=id+api_slot_bias;p->generation=k386_resources.usage[id].generation;
 p->handle=backing->handle;p->offset=backing->offset;p->ticks=k386_clock_ticks();p->result=-38;
 unsigned a=0xd39e,b=1,di=(unsigned)p;
 __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x2f; popw %%es":"+a"(a),"+b"(b),"+D"(di)::"ecx","edx","esi","cc","memory");
 return (a&65535)==0x39ed?p->result:-38;
}
void k386_video_broker(unsigned id){
 K386Session *s=&sessions[id];K386Frame *f=&s->frame;K386View *view=&k386_mailbox.view[id];
 unsigned at=(f->cs&65535)*16+(f->eip&65535),n=65536-(f->eip&65535);
 KvPacket p;p.width=0;p.operation=3;p.port=f->edx;p.value=f->eax;
 if(n>3)n=3;
 int result=-22;
 if(at<K386_RAM&&n<=K386_RAM-at&&backing_read(&runtime[id].backing,at,&p.width,n))result=k386_video_call(id,&p);
 if(result<0||p.width<1||p.width>3||p.operation>1){view->state=K386_FAULTED;view->fault=13;view->eip=f->eip;}
 else {
  if(!p.operation)f->eax=(f->eax&~255u)|(p.value&255);
  f->eip=(f->eip+p.width)&65535;
  runtime[id].backing.pages=(runtime[id].backing.pages&255)|((p.meta.flags&KV_GRAPH?0x8000:0)|(p.meta.font_writes||(p.meta.flags&KV_SEMI)||p.meta.columns!=80||p.meta.rows!=25||p.meta.text_start||p.meta.text_pitch!=80?0x4000:0));
  if(result)view->flags++;
  if(p.operation&&p.port==0x60&&result)runtime[id].text_valid=0;
  if(result&(KV_MODE_CHANGED|KV_CHANGED)){runtime[id].text.columns=p.meta.columns;runtime[id].text.rows=p.meta.rows;runtime[id].text.pitch=p.meta.text_pitch;runtime[id].text.start=p.meta.text_start;}
  if((result&KV_CURSOR_CHANGED)&&p.meta.text_pitch){view->column=runtime[id].text.column=((p.meta.text_cursor-p.meta.text_start)&4095)%p.meta.text_pitch;view->row=runtime[id].text.row=((p.meta.text_cursor-p.meta.text_start)&4095)/p.meta.text_pitch;}
 }
 s->request=0;
}
