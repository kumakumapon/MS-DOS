/* Original small TSR. Called at the monitor's serialized real-mode boundary. */
#include "video.h"
#include "../window/hardware.h"
extern unsigned short kv_sender_segment,kv_sender_offset;
extern unsigned kv_old_vector;
extern void kv_api(void);
static unsigned self,monitor,xms_entry;
static KvVideo slots[4];static unsigned generations[4],handles[4];
volatile struct {
 char signature[8];unsigned short version,size,ready,busy;
 unsigned owner,monitor,calls,rejected,stack_ok,generation[4],handle[4];KvMeta meta[4];
} kv_record={.signature={'K','V','9','8','V','I','D','1'},.version=1,.size=sizeof kv_record};
static unsigned linear(const void *p){return self<<16|((unsigned)p&65535);}
static int move(unsigned from,unsigned offset,unsigned to,unsigned target,unsigned bytes){
 struct __attribute__((packed)){unsigned n;unsigned short from;unsigned at;unsigned short to;unsigned target;} r={bytes,from,offset,to,target};
 unsigned a=0x0b00,b=0,d=0,s=(unsigned)&r;
 __asm__ volatile("pushw %%ds; pushw %%es; lcallw *%4; popw %%es; popw %%ds":"+a"(a),"+b"(b),"+d"(d),"+S"(s):"m"(xms_entry):"ecx","edi","cc","memory");
 return (a&65535)==1;
}
static int read_memory(void *context,unsigned at,void *out,unsigned n){
 KvPacket *p=context;if(at>KV_BACKING_BYTES||n>KV_BACKING_BYTES-at||n>1024)return 0;
 if((n&1)&&n>1)return read_memory(context,at,out,n-1)&&read_memory(context,at+n-1,(unsigned char *)out+n-1,1);
 if(n==1){unsigned char b[2];if(!move(p->handle,p->offset+(at&~1u),0,linear(b),2))return 0;*(unsigned char *)out=b[at&1];return 1;}
 return !(n&1)&&move(p->handle,p->offset+at,0,linear(out),n);
}
static int write_memory(void *context,unsigned at,const void *in,unsigned n){
 KvPacket *p=context;if(at>KV_BACKING_BYTES||n>KV_BACKING_BYTES-at||n>1024)return 0;
 if(n==1){unsigned char b[2];if(!move(p->handle,p->offset+(at&~1u),0,linear(b),2))return 0;b[at&1]=*(const unsigned char *)in;return move(0,linear(b),p->handle,p->offset+(at&~1u),2);}
 return !(n&1)&&move(0,linear(in),p->handle,p->offset+at,n);
}
void kv_dispatch(void){
 if(kv_sender_segment!=monitor||kv_sender_offset>65536-sizeof(KvPacket))return;
 KvPacket p;mem_copy(self,(unsigned)&p,monitor,kv_sender_offset,sizeof p);p.result=-22;
 kv_record.calls++;
 if(kv_record.busy){p.result=-16;goto done;}
 for(unsigned i=0;i<16;i++)if(mem_get(self,0x5000+i)!=0xa5)kv_record.stack_ok=0;
 if(!kv_record.stack_ok){p.result=-75;goto done;}
 if(p.version!=1||p.size!=sizeof p||p.slot>=4||!p.generation||!p.handle||p.handle>65535||p.offset>4095)goto done;
 kv_record.busy=1;
 unsigned id=p.slot;
 if(generations[id]!=p.generation||handles[id]!=p.handle){kv_reset(&slots[id]);generations[id]=p.generation;handles[id]=p.handle;}
 unsigned instruction=p.operation==3,original_eax=p.value;
 KvMemory io={&p,read_memory,write_memory};
 if(p.operation==3){p.result=kv_decode(&p);if(p.result<0)goto publish;}
 else if(p.operation!=2&&p.operation!=4&&p.operation!=5&&p.width!=1){p.result=-38;goto publish;}

 if(p.operation==4){
  unsigned at;unsigned char data[KV_READ_BYTES];
  p.result=-22;
  if(kv_sender_offset<=65536-sizeof(KvRead)&&p.width&&p.width<=sizeof data){
   if(!p.port){if(p.value<=sizeof(KvMeta)&&p.width<=sizeof(KvMeta)-p.value){mem_copy(self,(unsigned)data,self,(unsigned)&slots[id].meta+p.value,p.width);p.result=0;}}
   else if(!kv_region(p.port,p.value,p.width,&at)){if(p.port>=2&&p.port<=5)at=kv_page_plane(&slots[id],p.port-2,1)+p.value;p.result=read_memory(&p,at,data,p.width)?0:-5;}
   if(!p.result)mem_copy(monitor,kv_sender_offset+sizeof p,self,(unsigned)data,p.width);
  }
 }
 else if(p.operation==5)p.result=kv_bios(&slots[id],&p.port,p.width,p.value,&io);
 else if(p.operation==2)p.result=0;
 else if(p.operation==1||p.operation==7)p.result=kv_out(&slots[id],p.port,p.value,&io);
 else if(p.operation==0||p.operation==6)p.result=kv_in(&slots[id],p.port,p.ticks,&p.value,&io);
publish:
 if(instruction&&p.result>=0){if(!(p.operation&1)){unsigned mask=p.operation==6?65535:255;p.value=(original_eax&~mask)|(p.value&mask);}p.operation&=1;}
 if(instruction||p.operation==5)p.ticks=(slots[id].meta.flags&KV_ACCESS_PAGE?0x2000:0)|(slots[id].meta.flags&KV_GRAPH?0x8000:0)|(slots[id].meta.font_writes||(slots[id].meta.flags&KV_SEMI)||slots[id].meta.columns!=80||slots[id].meta.rows!=25||slots[id].meta.text_start||slots[id].meta.text_pitch!=80?0x4000:0);
 p.meta=slots[id].meta;kv_record.generation[id]=p.generation;kv_record.handle[id]=p.handle;kv_record.meta[id]=p.meta;
 kv_record.busy=0;
done:
 if(p.result<0)kv_record.rejected++;
 mem_copy(monitor,kv_sender_offset,self,(unsigned)&p,sizeof p);
}
int kv_install(void){
 unsigned a=0xd39e,b=0,di;unsigned short es;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)==0x39ed)return 2;
 a=0xd386;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x386d)return 0;
 monitor=es;
 __asm__ volatile("movw %%ds,%w0":"=r"(self));
 a=0x4310;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");
 xms_entry=(unsigned)es<<16|(b&65535);if(!xms_entry)return 0;
 a=0x352f;
 __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");kv_old_vector=(unsigned)es<<16|(b&65535);
 a=0x252f;__asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)kv_api):"ebx","ecx","esi","edi","cc","memory");
 kv_record.owner=self;kv_record.monitor=monitor;kv_record.ready=kv_record.stack_ok=1;
 for(unsigned i=0;i<16;i++)mem_put(self,0x5000+i,0xa5);
 return 1;
}
