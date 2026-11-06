/* Transient graphical service. Called only in real mode; the INT 2F callback
 * never invokes DOS, BIOS, scheduling or rendering. Its stack is separate. */
#include "gui.h"
#include "client-policy.h"
#include "../window/hardware.h"
extern u8 loader_buffer[4096];
extern unsigned owned[2];extern u32 owned_generation[2];
extern int gui_copy(u32,void *,unsigned,int);
extern void kg_gate(void);
extern u32 kg_old_vector;extern u16 kg_sender_segment,kg_sender_offset;
static KgClient client[2];static unsigned arena,self,host_segment,installed;
static u32 serial,seconds,last_clock;
volatile struct {
 char signature[8];u32 version,size,owner,arena,old_vector,vector,ready,requests,rejected;
 u32 generation[2],token[2],revision[2],events[2],dropped[2];
} kg_producer={.signature={'K','G','C','L','N','T','0','1'},.version=1,.size=sizeof kg_producer};
static void publish(void){
 for(unsigned i=0;i<2;i++){kg_producer.generation[i]=client[i].generation;kg_producer.token[i]=client[i].token;kg_producer.revision[i]=client[i].revision;kg_producer.events[i]=client[i].received;kg_producer.dropped[i]=client[i].dropped;}
}
void kg_clients_tick(void){
 u32 now=input_ticks();if(last_clock&&((now-last_clock)&0xffffff)<30720)return;last_clock=now;
 unsigned a=0x2c00,c=0,d=0;__asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"ebx","esi","edi","cc","memory");
 seconds=((c>>8)&255)*3600+(c&255)*60+((d>>8)&255);
}
int kg_clients_start(void){
 unsigned a=0xd394,b=0;__asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");if((a&65535)==0x394d)return -16;
 a=0xd390;b=0;unsigned di;u16 es;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x390d)return 0;
 host_segment=es;__asm__ volatile("movw %%ds,%w0":"=r"(self));
 arena=mem_alloc(8192);if(!arena)return -12;mem_fill(arena,0,8192,0);
 a=0x352f;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");kg_old_vector=(u32)es<<16|(b&65535);
 a=0x252f;__asm__ volatile("int $0x21":"+a"(a):"d"((u32)kg_gate):"ebx","ecx","esi","edi","cc","memory");installed=1;
 kg_producer.owner=self;kg_producer.arena=arena;kg_producer.old_vector=kg_old_vector;kg_producer.vector=(u32)self<<16|(unsigned)kg_gate;kg_producer.ready=1;
 kg_clients_tick();return 0;
}
void kg_clients_close(void){
 if(installed){unsigned a=0x252f,d=kg_old_vector&65535,seg=kg_old_vector>>16;
  __asm__ volatile("pushw %%ds; movw %w1,%%ds; int $0x21; popw %%ds":"+a"(a),"+c"(seg):"d"(d):"ebx","esi","edi","cc","memory");installed=0;
 }
 mem_free(arena);arena=0;kg_producer.ready=kg_producer.arena=0;
 for(unsigned i=0;i<2;i++)client[i]=(KgClient){0};
 publish();
}
void kg_dispatch(void){
 KgPacket p;kg_producer.requests++;
 if(kg_sender_segment!=host_segment||kg_sender_offset>65536-sizeof p)return;
 mem_copy(self,(unsigned)&p,kg_sender_segment,kg_sender_offset,sizeof p);
 p.result=-22;
 if(p.version!=1||p.size!=sizeof p||p.slot>=2)goto done;
 unsigned id=p.slot;
 if(!p.operation){if(client[id].generation==p.generation){client[id].token=0;client[id].revision++;client[id].head=client[id].count=0;}p.result=0;goto done;}
 if(p.operation!=1||p.bytes!=sizeof(KgRequest)||p.request>65536-sizeof(KgRequest))goto done;
 if(!owned[id]||owned_generation[id]!=p.generation){p.result=-1;goto done;}
 KgRequest *r=(KgRequest *)loader_buffer;
 mem_copy(self,(unsigned)r,host_segment,p.request,sizeof *r);
 extern volatile GuiDriverRecord gui_record;
 p.result=kg_policy(&client[id],r,p.generation,&serial,gui_record.width,gui_record.height);
 if(p.result>=0){
  if(r->operation==KG_FRAME)mem_copy(arena,id*4096,self,(unsigned)r,sizeof *r);
  if(r->operation==KG_OPEN)mem_fill(arena,id*4096,sizeof *r,0);
  r->slot=id;r->generation=p.generation;r->revision=client[id].revision;r->event.seconds=seconds;
  mem_copy(host_segment,p.request,self,(unsigned)r,sizeof *r);
 }
done:
 if(p.result<0)kg_producer.rejected++;
 publish();
 mem_copy(kg_sender_segment,kg_sender_offset,self,(unsigned)&p,sizeof p);
}
int kg_clients_snapshot(u32 pointer,unsigned bytes){
 if(bytes!=sizeof(KgRequest))return -22;
 u32 h[4];int e=gui_copy(pointer,h,sizeof h,0);if(e)return e;
 if(h[0]!=1||h[1]!=bytes||h[3]>=2)return -22;
 unsigned id=h[3];KgClient *c=&client[id];KgRequest *r=(KgRequest *)loader_buffer;
 if(arena&&c->token&&owned[id]&&owned_generation[id]==c->generation)mem_copy(self,(unsigned)r,arena,id*4096,sizeof *r);
 else {*r=(KgRequest){0};}
 r->version=1;r->size=sizeof *r;r->slot=id;r->generation=c->generation;r->token=owned[id]&&owned_generation[id]==c->generation?c->token:0;
 r->revision=c->revision;r->width=c->width;r->height=c->height;r->event.control=c->input_flags;
 for(unsigned i=0;i<32;i++)r->title[i]=c->title[i];
 return gui_copy(pointer,r,sizeof *r,1);
}
int kg_clients_event(u32 pointer,unsigned bytes){
 if(bytes!=sizeof(KgRequest))return -22;
 KgRequest *r=(KgRequest *)loader_buffer;int e=gui_copy(pointer,r,sizeof *r,0);if(e)return e;
 if(r->version!=1||r->size!=sizeof *r||r->slot>=2)return -22;
 unsigned id=r->slot;KgClient *c=&client[id];
 if(!owned[id]||owned_generation[id]!=r->generation)return -1;
 if(c->generation!=r->generation||!c->token||c->token!=r->token)return -116;
 KgEvent v=r->event;
 if(v.type<KG_KEY||v.type>KG_POINTER)return -22;
 if(v.type==KG_RESIZE&&(v.width<1||v.width>784||v.height<1||v.height>416))return -22;
 if((v.type==KG_POINTER||v.type==KG_MOUSE)&&(v.x < -800||v.x>800||v.y < -600||v.y>600||v.key>7))return -22;
 v.seconds=seconds;kg_enqueue(c,&v);publish();return 0;
}
