#include "host.h"
#include "../k386/bank.h"
#include "../window/hardware.h"
extern K32HostMailbox host_mailbox;
extern K32HostSlot host_slots[2];
extern int host_local_operate(unsigned,unsigned);
unsigned host_slot_bias;
static unsigned cold,spare;
typedef struct {K32HostView task[2];K32HostSlot slots[2];} Bank;
static const KBankSpan spans[]={{host_mailbox.task,0,sizeof host_mailbox.task},{host_slots,__builtin_offsetof(Bank,slots),sizeof(K32HostSlot)*2}};
static void copy(unsigned seg,int save){kbank_copy(seg,spans,2,save);}
static unsigned state_at(unsigned seg,unsigned id){unsigned at=__builtin_offsetof(Bank,slots)+id*sizeof(K32HostSlot);return mem_get(seg,at)|(unsigned)mem_get(seg,at+1)<<8;}
unsigned host_occupied(void){
 unsigned bits=0;
 for(unsigned i=0;i<2;i++){
  if(host_slot_bias?state_at(spare,i):host_slots[i].state)bits|=1u<<i;
  if(cold&&(host_slot_bias?host_slots[i].state:state_at(cold,i)))bits|=1u<<(i+2);
 }
 return bits;
}
int host_bank_capacity(unsigned n){
 if(n==4){if(cold)return 0;unsigned a=mem_alloc(sizeof(Bank)),b=mem_alloc(sizeof(Bank));if(!a||!b){mem_free(a);mem_free(b);return 8;}cold=a;spare=b;mem_fill(cold,0,sizeof(Bank),0);return 0;}
 if(n!=2)return 87;
 if(host_occupied()&12)return 5;
 mem_free(cold);mem_free(spare);cold=spare=0;return 0;
}
u32 host_task_address(unsigned id){
 if(id>=4||(id>=2&&!cold))return 0;
 if(id>=2)return (u32)cold*16+(id-2)*sizeof(K32HostView);
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));return (u32)ds*16+(unsigned)&host_mailbox.task[id];
}
void host_task_read(unsigned id,unsigned offset,void *out,unsigned n){
 u32 p=host_task_address(id)+offset;u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));mem_copy(ds,(unsigned)out,p>>4,p&15,n);
}
int host_operate(unsigned id,unsigned cmd){
 if(id<2)return host_local_operate(id,cmd);
 if(id>=4||!cold)return 87;
 copy(spare,1);copy(cold,0);host_slot_bias=2;
 int e=host_local_operate(id-2,cmd);
 copy(cold,1);copy(spare,0);host_slot_bias=0;return e;
}
