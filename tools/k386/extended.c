#define K386_MONITOR
#include "runtime.h"
#include "resources.h"
#include "xmem.h"
#include "bank.h"
#include "extended.h"
#include "../window/hardware.h"
extern void api_dispatch(void);
extern int api_available(void),api_capacity(unsigned);
extern unsigned api_slot_bias;
K386Extended k386_extended={.signature={'K','3','8','6','E','X','T','1'},.version=1,.size=sizeof(K386Extended),.capacity=2};
typedef struct {
 K386View view[2];K386Session session[2];K386Runtime runtime[2];k32 pd[2];
 K386Usage usage[2];k32 counts[2];K386Call calls[2][16];K386Frame frame[2];k8 memory[2][512];
 k16 pending[2];KXBlock blocks[2][KX_BLOCKS];
} Bank;
#define SPAN(field,local) {local,__builtin_offsetof(Bank,field),sizeof(((Bank *)0)->field)}
static const KBankSpan spans[]={SPAN(view,k386_mailbox.view),SPAN(session,sessions),SPAN(runtime,runtime),SPAN(pd,page_directory),SPAN(usage,k386_resources.usage),SPAN(counts,k386_diagnostics.count),SPAN(calls,k386_diagnostics.calls),SPAN(frame,k386_diagnostics.frame),SPAN(memory,k386_diagnostics.memory),SPAN(pending,k386_xmem.pending),SPAN(blocks,k386_xmem.blocks)};
static unsigned cold,spare,extra_tables;
static void copy(unsigned segment,int save){kbank_copy(segment,spans,sizeof spans/sizeof spans[0],save);}
static void publish(void){
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 for(unsigned i=0;i<2;i++){
  k386_extended.view[i]=(k32)ds*16+(unsigned)&k386_mailbox.view[i];
  k386_extended.usage[i]=(k32)ds*16+(unsigned)&k386_resources.usage[i];
  k386_extended.frame[i]=(k32)ds*16+(unsigned)&k386_diagnostics.frame[i];
  k386_extended.view[i+2]=cold?(k32)cold*16+__builtin_offsetof(Bank,view)+i*sizeof(K386View):0;
  k386_extended.usage[i+2]=cold?(k32)cold*16+__builtin_offsetof(Bank,usage)+i*sizeof(K386Usage):0;
  k386_extended.frame[i+2]=cold?(k32)cold*16+__builtin_offsetof(Bank,frame)+i*sizeof(K386Frame):0;
 }
}
int k386_bank_capacity(unsigned n){
 publish();if(n==k386_extended.capacity)return 0;
 if(n!=2&&n!=4)return 87;
 if(n==4){
  unsigned a=mem_alloc(sizeof(Bank)),b=mem_alloc(sizeof(Bank)),t=mem_alloc(28672);
  if(!a||!b||!t){mem_free(a);mem_free(b);mem_free(t);return 8;}
  cold=a;spare=b;extra_tables=t;copy(spare,1);
  for(unsigned i=0;i<sizeof spans/sizeof spans[0];i++){u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));mem_fill(ds,(unsigned)spans[i].local,spans[i].bytes,0);}
  k32 pd=((k32)t*16+4095)&~4095u;
  for(unsigned i=0;i<2;i++){page_directory[i]=pd+i*12288;k386_resources.usage[i].priority=K386_NORMAL;for(unsigned j=0;j<2000;j++)k386_mailbox.view[i].screen[j]=' ';}
  copy(cold,1);copy(spare,0);k386_extended.capacity=4;publish();return 0;
 }
 if(!cold)return 5;
 copy(spare,1);copy(cold,0);int busy=0;
 for(unsigned i=0;i<2;i++){unsigned blocks;if(runtime[i].backing.handle||xmem_used(i,0,&blocks))busy=1;}
 copy(spare,0);if(busy)return 5;
 mem_free(cold);mem_free(spare);mem_free(extra_tables);cold=spare=extra_tables=0;k386_extended.capacity=2;publish();return 0;
}
void api_dispatch_extended(void){
 if(k386_extended.busy||!api_available()){k386_mailbox.error=5;return;}
 k386_extended.busy=1;
 unsigned id=k386_mailbox.session;
 if(k386_mailbox.command==K386_CAPACITY){k386_mailbox.error=api_capacity(k386_mailbox.key);goto done;}
 if(id>=k386_extended.capacity){k386_mailbox.error=87;goto done;}
 if(id<2){api_dispatch();goto done;}
 k386_mailbox.reserved=1;copy(spare,1);copy(cold,0);api_slot_bias=2;k386_mailbox.session=id-2;
 api_dispatch();
 if(k386_mailbox.command==K386_TEXT&&!k386_mailbox.error)((K386TextChunk *)transfer)->slot=id;
 copy(cold,1);copy(spare,0);api_slot_bias=0;k386_mailbox.session=id;k386_mailbox.reserved=0;
 done:k386_extended.busy=0;
}
_Static_assert(sizeof(Bank)<16384,"bounded V86 bank control");
