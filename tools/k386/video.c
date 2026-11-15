#define K386_MONITOR
#include "runtime.h"
#include "resources.h"
#include "video.h"
int k386_video_call(unsigned id,KvPacket *p){
 K386Backing *backing=&runtime[id].backing;
 if(!backing->handle||(backing->pages&4095)<0xe8)return -38;
 p->version=1;p->size=sizeof *p;p->slot=id+api_slot_bias;p->generation=k386_resources.usage[id].generation;
 p->handle=backing->handle;p->offset=backing->offset;p->ticks=k386_clock_ticks();p->result=-38;
 unsigned a=0xd39e,b=1,di=(unsigned)p;
 __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x2f; popw %%es":"+a"(a),"+b"(b),"+D"(di)::"ecx","edx","esi","cc","memory");
 return (a&65535)==0x39ed?p->result:-38;
}
void k386_video_broker(unsigned id){
 K386Session *s=&sessions[id];K386Frame *f=&s->frame;K386View *view=&k386_mailbox.view[id];
 unsigned bios=s->request==KV_BIOS_REQUEST,ax=f->eax&65535;
 KvPacket p;int result=-22;
 if(bios){p.operation=5;p.port=ax;p.value=f->edx&65535;p.width=f->ecx&65535;result=k386_video_call(id,&p);}
 else{
  unsigned at=(f->cs&65535)*16+(f->eip&65535),n=65536-(f->eip&65535);
  p.width=0;p.operation=3;p.port=f->edx;p.value=f->eax;if(n>3)n=3;
  if(at<K386_RAM&&n<=K386_RAM-at&&backing_read(&runtime[id].backing,at,&p.width,n))result=k386_video_call(id,&p);
 }
 if(bios&&result<0){f->eax=(f->eax&~255u)|((unsigned)-result&255);f->eflags|=1;}
 else if(result<0||(!bios&&(p.width<1||p.width>3||p.operation>1))){view->state=K386_FAULTED;view->fault=13;view->eip=f->eip;}
 else{
  if(bios){f->eax=(f->eax&~65535u)|(p.port&65535);f->eflags&=~1u;}
  else{if(!p.operation)f->eax=p.value;f->eip=(f->eip+p.width)&65535;}
  runtime[id].backing.pages=(runtime[id].backing.pages&4095)|(p.ticks&0xe000);
  if(result)view->flags++;
  if((bios&&(ax>>8)==0x16)||(!bios&&(p.operation&1)&&p.port==0x60&&result))runtime[id].text_valid=0;
  if(result&(KV_MODE_CHANGED|KV_CHANGED)){runtime[id].text.columns=p.meta.columns;runtime[id].text.rows=p.meta.rows;runtime[id].text.pitch=p.meta.text_pitch;runtime[id].text.start=p.meta.text_start;}
  if((result&KV_CURSOR_CHANGED)&&p.meta.text_pitch){view->column=runtime[id].text.column=((p.meta.text_cursor-p.meta.text_start)&4095)%p.meta.text_pitch;view->row=runtime[id].text.row=((p.meta.text_cursor-p.meta.text_start)&4095)/p.meta.text_pitch;}
 }
 s->request=0;
}
