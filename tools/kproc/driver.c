/* INT 2F queues only; DOS launch/stop runs on an explicit outside-host pump. */
#include "process.h"
#include "../k38632/manager.h"
#include "../window/hardware.h"
extern unsigned short kp_sender_segment,kp_sender_offset,kp_api_kind;
extern unsigned kp_old_vector;extern void kp_api(void);
static unsigned self,host_segment,host_offset,indos_segment,indos_offset;
static KpState model;
volatile struct {
 char signature[8];unsigned short version,size,ready,busy;
 unsigned owner,manager,calls,rejected,stack_ok,state,started,launches;
} kp_record={.signature={'K','3','2','P','R','O','C','1'},.version=1,.size=sizeof kp_record};
static unsigned read16(unsigned address){return mem_get(address>>4,address&15)|(unsigned)mem_get(address>>4,(address&15)+1)<<8;}
static unsigned read32(unsigned address){return read16(address)|read16(address+2)<<16;}
static unsigned host_at(unsigned offset){return host_segment*16+host_offset+offset;}
static unsigned capacity(void){return read16(host_at(__builtin_offsetof(K32Manager4,capacity)));}
static int inspect(void *unused,unsigned slot,KpIdentity *id){
 (void)unused;if(slot>=capacity()||read16(host_at(__builtin_offsetof(K32Manager4,kind)+slot*2))!=K32_PM32)return 0;
 unsigned view=read32(host_at(__builtin_offsetof(K32Manager4,view)+slot*4)),record=read32(host_at(__builtin_offsetof(K32Manager4,record)+slot*4));
 if(!view||!record)return 0;
 unsigned state=read16(view),fault=read16(view+4),cancelled=read32(record+12)==5;
 unsigned held=read16(record+40)||read32(record+820)||read16(record+142);
 *id=(KpIdentity){slot,read32(host_at(__builtin_offsetof(K32Manager4,generation)+slot*4)),read32(record+28),read32(record+56),state==K386_RUNNING||state==K386_WAITING||state==K386_STOPPED?KP_RUNNING:held?KP_RECLAIMING:cancelled?KP_DONE:0,fault?128+(fault&255):cancelled?130:read16(view+2),fault};return 1;
}
static int command(unsigned operation,unsigned slot,const char *path){
 struct {char signature[8];unsigned short version,size,ready,error,command,session,key,busy;char path[128];} previous;
 mem_copy(self,(unsigned)&previous,host_segment,host_offset,sizeof previous);
 typeof(previous) *scratch=&previous;scratch->command=operation;scratch->session=slot;scratch->key=0;
 if(path)for(unsigned i=0;i<128;i++){scratch->path[i]=path[i];if(!path[i])break;}
 /* Only the common command prefix is written; producer pointers stay private. */
 unsigned char old[152];mem_copy(self,(unsigned)old,host_segment,host_offset,sizeof old);
 mem_copy(host_segment,host_offset,self,(unsigned)&previous,sizeof previous);
 unsigned a=0xd392,b=1;__asm__ volatile("pushw %%es; int $0x2f; popw %%es":"+a"(a):"b"(b):"ecx","edx","esi","edi","cc","memory");
 int result=-(int)read16(host_at(__builtin_offsetof(K32Manager4,error)));
 mem_copy(host_segment,host_offset,self,(unsigned)old,sizeof old);return result;
}
static int cancel(void *unused,unsigned slot){(void)unused;return command(K386_END,slot,0);}
static int launch(void *unused,const char *path,unsigned streams,KpIdentity *id){
 (void)unused;
 for(unsigned slot=0;slot<capacity();slot++){
  unsigned pointer=read32(host_at(__builtin_offsetof(K32Manager4,view)+slot*4));unsigned state=pointer?read16(pointer):K386_ENDED;
  if(state==K386_RUNNING||state==K386_WAITING||state==K386_STOPPED)continue;
  int e=command(K386_CREATE,slot,path);if(e)return e;
  if(!inspect(0,slot,id)||id->state!=KP_RUNNING){command(K386_END,slot,0);return -8;}
  unsigned record=read32(host_at(__builtin_offsetof(K32Manager4,record)+slot*4));if(streams&&!(read32(record+772)&K32_FEATURE_PROCESS)){command(K386_END,slot,0);return -38;}
  kp_record.started|=1u<<slot;kp_record.launches++;return 0;
 }return -11;
}
static int safe(void){
 unsigned short msw;__asm__ volatile("smsw %0":"=r"(msw));if((msw&1)||mem_get(indos_segment,indos_offset))return 0;
 for(unsigned i=0;i<16;i++)if(mem_get(self,0x4000+i)!=0xa5)kp_record.stack_ok=0;
 return kp_record.stack_ok;
}
void kp_dispatch(void){
 kp_record.calls++;
 if(kp_api_kind==2){
  kp_record.started=0;
  if(!safe()||read16(host_at(__builtin_offsetof(K32Manager4,busy))))return;
  KpHost host={0,launch,inspect,cancel};kp_record.busy=1;kp_pump(&model,&host);kp_record.busy=0;return;
 }
 if(kp_sender_segment!=host_segment||kp_sender_offset>65536-sizeof(K32IoPacket))return;
 K32IoPacket p;mem_copy(self,(unsigned)&p,host_segment,kp_sender_offset,sizeof p);p.result=-22;
 if(!safe()||p.version!=1||p.size!=sizeof p||p.operation!=1||p.request>65536-sizeof(K32Io)||p.buffer>65536-256)goto done;
 KpIdentity caller={0};unsigned slot=0;for(;slot<capacity();slot++)if(inspect(0,slot,&caller)&&caller.state==KP_RUNNING&&caller.physical==p.owner&&caller.epoch==p.epoch)break;
 if(slot==capacity()){p.result=-116;goto done;}
 int owner=kp_attach(&model,&caller);if(owner<0){p.result=owner;goto done;}
 K32Io r;unsigned char data[256];mem_copy(self,(unsigned)&r,host_segment,p.request,sizeof r);
 if(r.bytes>sizeof data){p.result=-22;goto done;}
 if(r.operation==KP_SPAWN||r.operation==KP_SEND||r.operation==KP_WRITE)mem_copy(self,(unsigned)data,host_segment,p.buffer,r.bytes);
 kp_record.busy=1;p.result=kp_request(&model,(unsigned)owner,&r,data);kp_record.busy=0;
 if(p.result>0&&(r.operation==KP_READ||r.operation==KP_RECEIVE))mem_copy(host_segment,p.buffer,self,(unsigned)data,(unsigned)p.result);
 mem_copy(host_segment,p.request,self,(unsigned)&r,sizeof r);
 done:if(p.result<0&&p.result!=-11)kp_record.rejected++;mem_copy(host_segment,kp_sender_offset,self,(unsigned)&p,sizeof p);
}
int kp_install(void){
 unsigned a=0xd3a0,b=0,di;unsigned short es;
 __asm__ volatile("pushw %%es; int $0x2f; popw %%es":"+a"(a):"b"(b):"ecx","edx","esi","edi","cc","memory");if((a&65535)==0x3a0d)return 2;
 a=0xd392;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");if((a&65535)!=0x392d)return 0;
 host_segment=es;host_offset=di&65535;if(read16(host_at(8))!=2||read16(host_at(10))!=sizeof(K32Manager4))return 0;
 __asm__ volatile("movw %%ds,%w0":"=r"(self));
 a=0x3400;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");indos_segment=es;indos_offset=b&65535;
 a=0x352f;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");kp_old_vector=(unsigned)es<<16|(b&65535);
 a=0x252f;__asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)kp_api):"ebx","ecx","esi","edi","cc","memory");
 for(unsigned i=0;i<16;i++)mem_put(self,0x4000+i,0xa5);
 kp_record.owner=self;kp_record.manager=host_at(0);kp_record.state=self*16+(unsigned)&model;kp_record.ready=kp_record.stack_ok=1;return 1;
}
