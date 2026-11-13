/* Common versioned slot manager. Backends own CPU modes; this layer stays real-mode.
 * V86 ABI1 stays available. PM32 and V86 share slots, policy and lifecycle here.
 */
#include "manager.h"
#include "services.h"
#include "../window/hardware.h"
#include "../k386/runtime.h"
#include "../k386/extended.h"
extern K32HostMailbox host_mailbox;
extern char host_command[128];
extern u8 loader_buffer[4096];
extern int host_operate(unsigned,unsigned);
void manager_publish(void);
K32Manager k32_manager={.signature={'K','3','2','M','U','X','0','1'},.version=1,.size=sizeof(K32Manager)};
K32Manager4 k32_extended={.signature={'K','3','2','M','U','X','0','2'},.version=2,.size=sizeof(K32Manager4),.capacity=2,.profile=16};
static unsigned ds_segment,legacy_segment,legacy_offset,resource_segment,resource_offset;
static unsigned extension_segment,extension_offset;
u32 manager_generation(unsigned id){return id<k32_extended.capacity?resource_usage(id)->generation:0;}
static u32 seen_generation[4];
static unsigned diagnostic_segment,diagnostic_offset;
static unsigned preparing=4;
int manager_staging(unsigned id){return preparing==id;}
static unsigned word(unsigned seg,unsigned at){return mem_get(seg,at)|(unsigned)mem_get(seg,at+1)<<8;}
static u32 longword(unsigned seg,unsigned at){return word(seg,at)|(u32)word(seg,at+2)<<16;}
static void put(unsigned at,unsigned n){mem_put(legacy_segment,legacy_offset+at,n);mem_put(legacy_segment,legacy_offset+at+1,n>>8);}
static u32 ticks(void){u8 h,m,l;do{__asm__ volatile("inb $0x5f,%0":"=a"(h));__asm__ volatile("inb $0x5d,%0":"=a"(m));__asm__ volatile("inb $0x5c,%0":"=a"(l));u8 a,b;__asm__ volatile("inb $0x5d,%0":"=a"(a));__asm__ volatile("inb $0x5f,%0":"=a"(b));if(a==m&&b==h)break;}while(1);return (u32)h<<16|(u32)m<<8|l;}
static unsigned viewword(unsigned id,unsigned at){u32 p=k32_extended.view[id]+at;return word(p>>4,p&15);}
static u32 viewlong(unsigned id,unsigned at){u32 p=k32_extended.view[id]+at;return longword(p>>4,p&15);}
static int legacy(unsigned cmd,unsigned id,unsigned key,const char *path){
 if(!legacy_segment)return 50;
 put(__builtin_offsetof(K386Mailbox,command),cmd);put(__builtin_offsetof(K386Mailbox,session),id);put(__builtin_offsetof(K386Mailbox,key),key);
 if(path)for(unsigned i=0;i<128;i++){mem_put(legacy_segment,legacy_offset+__builtin_offsetof(K386Mailbox,path)+i,path[i]);if(!path[i])break;}
 unsigned a=id>=2||cmd==K386_CAPACITY?0xd38b:0xd386,b=1;__asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");return word(legacy_segment,legacy_offset+14);
}
static u32 backend_pointer(unsigned field,unsigned id){
 if(extension_segment){unsigned at=extension_offset+field+id*4;return longword(extension_segment,at);}
 if(id>=2)return 0;
 if(field==__builtin_offsetof(K386Extended,view))return (u32)legacy_segment*16+legacy_offset+__builtin_offsetof(K386Mailbox,view)+id*sizeof(K386View);
 if(field==__builtin_offsetof(K386Extended,usage))return (u32)resource_segment*16+resource_offset+__builtin_offsetof(K386Resources,usage)+id*sizeof(K386Usage);
 return (u32)diagnostic_segment*16+diagnostic_offset+__builtin_offsetof(K386Diagnostics,frame)+id*sizeof(K386Frame);
}
int manager_v86_active(unsigned id){
 if(!legacy_segment||id>=k32_extended.capacity)return 0;
 u32 p=backend_pointer(__builtin_offsetof(K386Extended,view),id);if(!p)return 0;
 unsigned s=word(p>>4,p&15);return s==K386_RUNNING||s==K386_WAITING||s==K386_STOPPED;
}
static u32 record_value(unsigned id,unsigned at){u32 v;host_task_read(id,__builtin_offsetof(K32HostView,record)+at,&v,4);return v;}
static void pointers(unsigned id){
 unsigned pm=k32_extended.kind[id]==K32_PM32;u32 p=host_task_address(id);
 k32_extended.view[id]=pm?p:backend_pointer(__builtin_offsetof(K386Extended,view),id);
 k32_extended.record[id]=p?p+__builtin_offsetof(K32HostView,record):0;
 k32_extended.abi[id]=pm?record_value(id,768):0;
 k32_extended.generation[id]=resource_usage(id)->generation;
 k32_extended.usage[id]=(u32)ds_segment*16+(unsigned)resource_usage(id);
 k32_extended.frame[id]=pm?k32_extended.record[id]+320:backend_pointer(__builtin_offsetof(K386Extended,frame),id);
}
static void usage(unsigned id){
 K386Usage *u=resource_usage(id);
 if(k32_extended.kind[id]==K32_PM32){
  u8 *r=loader_buffer;host_task_read(id,__builtin_offsetof(K32HostView,record),r,1024);
  u->memory_kb=*(u16 *)(r+40)?4100:0;u->heap_kb=*(u32 *)(r+788)*4;u->handles=*(u32 *)(r+820);u->psp=u->memory_kb?host_mailbox.owner:0;u->depth=u->memory_kb?1:0;
  const char *name=(char *)r+704;for(unsigned i=0;i<16;i++){u->program[i]=name[i];if(!name[i])break;}u->program[15]=0;
 }else if(resource_segment){
  K386Usage old;u32 p=backend_pointer(__builtin_offsetof(K386Extended,usage),id);mem_copy(ds_segment,(unsigned)&old,p>>4,p&15,sizeof old);
  u->memory_kb=old.memory_kb;u->heap_kb=old.heap_kb;u->handles=old.handles;u->depth=old.depth;u->psp=old.psp;
  for(unsigned i=0;i<16;i++)u->program[i]=old.program[i];
  seen_generation[id]=old.generation;
 }
 pointers(id);
}
static void event(unsigned id,unsigned type,unsigned reason){
 K386Frame f={0};u32 cr2=viewlong(id,__builtin_offsetof(K386View,cr2));unsigned fault=viewword(id,__builtin_offsetof(K386View,fault));
 f.eip=viewlong(id,__builtin_offsetof(K386View,eip));f.eflags=viewlong(id,__builtin_offsetof(K386View,eflags));
 if(k32_extended.kind[id]==K32_PM32)f.cs=record_value(id,380);
 else if(diagnostic_segment&&type!=K386_EVENT_START){u32 p=backend_pointer(__builtin_offsetof(K386Extended,frame),id);mem_copy(ds_segment,(unsigned)&f,p>>4,p&15,sizeof f);}
 resource_event(id,type,reason,&f,cr2,fault);
}
static u32 old_sequence(void){return resource_segment?longword(resource_segment,resource_offset+12):0;}
static void old_events(unsigned id,u32 before){
 if(!resource_segment)return;
 u32 after=old_sequence(),n=after-before;if(n>16)n=16;
 unsigned next=word(resource_segment,resource_offset+16);
 for(unsigned i=0;i<n;i++){
  K386Event e;unsigned index=(next+16-n+i)%16;
  mem_copy(ds_segment,(unsigned)&e,resource_segment,resource_offset+__builtin_offsetof(K386Resources,event)+index*sizeof e,sizeof e);
  if(e.session!=(id&1)||e.type==K386_EVENT_START)continue;
  K386Frame f={0};f.eip=e.eip;f.cs=e.cs;
  K386Usage *u=resource_usage(id);unsigned psp=u->psp;u->psp=e.psp;
  resource_event(id,e.type,e.reason,&f,e.cr2,e.fault);u->psp=psp;
 }
}
static void pm_event(unsigned id,unsigned previous){
 unsigned state=viewword(id,0);
 if(state==previous)return;
 if(state==K386_FAULTED)event(id,K386_EVENT_FAULT,K386_REASON_FAULT);
 else if(state==K386_ENDED)event(id,record_value(id,68)==65?K386_EVENT_INTERRUPT:K386_EVENT_EXIT,record_value(id,68)==65?K386_REASON_INTERRUPT:K386_REASON_EXIT);
}
static int control(unsigned id,unsigned pm,unsigned v86){return k32_extended.kind[id]==K32_PM32?host_operate(id,pm):legacy(v86,id,0,0);}
static void limit(unsigned id){unsigned s=viewword(id,0);if((s==K386_RUNNING||s==K386_WAITING)&&resource_exhausted(id)){control(id,K32_HOST_STOP,K386_STOP);usage(id);event(id,K386_EVENT_LIMIT,K386_REASON_LIMIT);}}
static void adopt(void){
 for(unsigned id=0;id<k32_extended.capacity;id++){
  if((host_occupied()&(1u<<id))&&k32_extended.kind[id]!=K32_PM32){k32_extended.kind[id]=K32_PM32;resource_begin(id);pointers(id);usage(id);event(id,K386_EVENT_START,K386_REASON_NONE);}
  else if(!(host_occupied()&(1u<<id))&&manager_v86_active(id)){
   u32 p=backend_pointer(__builtin_offsetof(K386Extended,usage),id);
   u32 g=longword(p>>4,p&15);
   if(k32_extended.kind[id]!=K32_V86||seen_generation[id]!=g){k32_extended.kind[id]=K32_V86;resource_begin(id);pointers(id);usage(id);event(id,K386_EVENT_START,K386_REASON_NONE);}
  }
  usage(id);
 }
}
/* Classify only the explicit PM32 outer descriptor. Full ELF validation remains
 * in the PM32 loader; ordinary/native/V86 validation remains in the V86 loader. */
static int pm_image(const char *command){
 char path[128];unsigned n=0,at=0;int quote=0;while(command[at]==' ')at++;
 for(;command[at]&&n<127;at++){char c=command[at];if(c=='"'){quote=!quote;continue;}if(!quote&&c==' ')break;path[n++]=c;}path[n]=0;
 int opened=k32_app_open(path);if(opened<0)return 0;unsigned h=opened;u8 b[96];unsigned a;
 /* DOS recognizes devices even with extensions (CON.COM). Never issue a
  * header read on a character device; leave its rejection to the V86 policy. */
 unsigned device=0;a=0x4400;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"=d"(device):"b"(h):"cc","memory");
 if(a==~0u||(device&128)){a=0x3e00;__asm__ volatile("int $0x21":"+a"(a):"b"(h):"cc","memory");return 0;}
 a=0x3f00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h),"c"(96),"d"((u32)b):"cc","memory");unsigned bytes=a;
 a=0x3e00;__asm__ volatile("int $0x21":"+a"(a):"b"(h):"cc","memory");
 const char *sig="K386APP1";if(bytes!=96||b[0]!='M'||b[1]!='Z'||b[38]!=3||b[39])return 0;
 for(unsigned i=0;i<8;i++)if(b[28+i]!=(u8)sig[i])return 0;
 return 1;
}
static int create(unsigned id,const char *path){
 if(host_occupied()&(1u<<id))return 5;
 int pm=pm_image(path),e;
 if(pm){
  for(unsigned i=0;i<128;i++){host_mailbox.path[i]=path[i];if(!path[i])break;}host_mailbox.path[127]=0;
  preparing=id;e=host_operate(id,K32_HOST_CREATE);preparing=4;
  if(!e){int old=legacy(K386_END,id,0,0);if(old){host_operate(id,K32_HOST_END);return old;}}
 }
 else e=legacy(K386_CREATE,id,0,path);
 if(e)return e;
 k32_extended.kind[id]=pm?K32_PM32:K32_V86;resource_begin(id);pointers(id);usage(id);event(id,K386_EVENT_START,K386_REASON_NONE);return 0;
}
static void poll(unsigned id){
 if(viewword(id,0)!=K386_WAITING)return;
 if(k32_extended.kind[id]==K32_PM32){host_operate(id,K32_HOST_POLL);usage(id);pm_event(id,K386_WAITING);}
 else legacy(K386_POLL,id,0,0);
}
static void step(unsigned id){
 limit(id);poll(id);
 unsigned state=viewword(id,0);if(state!=K386_RUNNING)return;
 u32 start=ticks(),seq=old_sequence();unsigned pm=k32_extended.kind[id]==K32_PM32;
 if(pm){if(state==K386_WAITING)return;host_operate(id,K32_HOST_STEP);}else legacy(K386_STEP,id,0,0);
 resource_charge(id,(ticks()-start)&0xffffff);usage(id);
 if(pm)pm_event(id,state);else old_events(id,seq);limit(id);
}
void manager_init(unsigned segment){
 ds_segment=segment;unsigned a=0xd386,b=0,di;u16 seg;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x386d)return;
 legacy_segment=seg;legacy_offset=di&65535;
 if(word(seg,legacy_offset+8)!=1||word(seg,legacy_offset+10)!=sizeof(K386Mailbox)){legacy_segment=0;return;}
 a=0xd387;b=0;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x387d)return;
 resource_segment=seg;resource_offset=di&65535;
 a=0xd388;b=0;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x388d)return;
 diagnostic_segment=seg;diagnostic_offset=di&65535;
 k32_manager.diagnostics=(u32)seg*16+diagnostic_offset;
 a=0xd38b;b=0;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)==0x38bd&&word(seg,(di&65535)+8)==1&&word(seg,(di&65535)+10)==sizeof(K386Extended)){extension_segment=seg;extension_offset=di&65535;}
 resource_init();k32_extended.owner=host_mailbox.owner;k32_extended.legacy=(u32)legacy_segment*16+legacy_offset;k32_extended.resources=(u32)segment*16+(unsigned)&k386_resources;
 for(unsigned id=0;id<k32_extended.capacity;id++)pointers(id);
 k32_extended.ready=1;manager_publish();
}
void manager_dispatch(void){
 adopt();unsigned id=k32_extended.session;k32_extended.error=0;
 if(!k32_extended.ready||id>=k32_extended.capacity){k32_extended.error=87;return;}
 switch(k32_extended.command){
 case K386_CREATE:k32_extended.error=create(id,k32_extended.path);break;
 case K386_STEP:step(id);break;
 case K386_POLL:poll(id);break;
 case K386_SCHEDULE:{
  if(k32_extended.key>8){k32_extended.error=87;break;}unsigned slices=k32_extended.key?k32_extended.key:1;u32 start=ticks();
  for(unsigned n=0;n<slices;n++){
   unsigned runnable=0;
   for(unsigned i=0;i<k32_extended.capacity;i++){limit(i);poll(i);unsigned s=viewword(i,0);if(s==K386_RUNNING)runnable|=1u<<i;}
   int chosen=resource_pick(runnable);k32_extended.session=chosen<0?65535:chosen;if(chosen<0)break;
   step(chosen);if(((ticks()-start)&0xffffff)>=3072)break;
  }break;}
 case K386_KEY:{unsigned before=viewword(id,0);if(k32_extended.kind[id]==K32_PM32){host_mailbox.key=k32_extended.key;k32_extended.error=host_operate(id,K32_HOST_KEY);usage(id);pm_event(id,before);}else k32_extended.error=legacy(K386_KEY,id,k32_extended.key,0);break;}
 case K386_STOP:k32_extended.error=control(id,K32_HOST_STOP,K386_STOP);usage(id);event(id,K386_EVENT_PAUSE,K386_REASON_PAUSE);break;
 case K386_RESUME:if(resource_exhausted(id))k32_extended.error=5;else{k32_extended.error=control(id,K32_HOST_RESUME,K386_RESUME);usage(id);event(id,K386_EVENT_RESUME,K386_REASON_NONE);}break;
 case K386_END:event(id,K386_EVENT_END,K386_REASON_END);k32_extended.error=control(id,K32_HOST_END,K386_END);usage(id);break;
 case K386_INTERRUPT:event(id,K386_EVENT_INTERRUPT,K386_REASON_INTERRUPT);k32_extended.error=control(id,K32_HOST_END,K386_INTERRUPT);usage(id);break;
 case K386_PRIORITY:k32_extended.error=resource_priority(id,k32_extended.key==65535?(resource_usage(id)->priority+1)%3:k32_extended.key);if(!k32_extended.error)event(id,K386_EVENT_PRIORITY,K386_REASON_NONE);break;
 case K386_BUDGET:{unsigned ms=k32_extended.key;if(ms==65535){unsigned n=resource_ms(resource_usage(id)->limit_ticks);ms=!n?1000:n<5000?5000:n<10000?10000:0;}k32_extended.error=resource_budget(id,ms);if(!k32_extended.error){event(id,K386_EVENT_BUDGET,K386_REASON_NONE);limit(id);}break;}
 case K386_DESCRIBE:k32_extended.error=resource_describe(id,k32_extended.key);break;
 case K386_TEXT:k32_extended.error=k32_extended.kind[id]==K32_V86?legacy(K386_TEXT,id,k32_extended.key,0):50;break;
 case K32_RESTART:if(k32_extended.kind[id]==K32_PM32){char command[128];host_task_read(id,__builtin_offsetof(K32HostView,command),command,sizeof command);k32_extended.error=create(id,command);}else k32_extended.error=create(id,"KSH.EXE");break;
 case K386_CAPACITY:k32_extended.error=manager_capacity(k32_extended.key);break;
 default:k32_extended.error=87;
 }
}
void manager_publish(void){
 k32_manager.ready=k32_extended.ready;k32_manager.owner=k32_extended.owner;
 k32_manager.resources=k32_extended.resources;k32_manager.legacy=k32_extended.legacy;
 for(unsigned i=0;i<2;i++){k32_manager.kind[i]=k32_extended.kind[i];k32_manager.abi[i]=k32_extended.abi[i];k32_manager.generation[i]=k32_extended.generation[i];k32_manager.view[i]=k32_extended.view[i];k32_manager.record[i]=k32_extended.record[i];}
}
/* Capacity changes allocate only control state. Each PM32 process still owns
 * an independent 4 MiB XMS block, page tables, handles and task snapshot. */
int manager_capacity(unsigned n){
 if(n==k32_extended.capacity)return 0;
 if(n!=2&&n!=4)return 87;
 if(!extension_segment)return 50;
 if(n==4){
  extern u32 xms_entry;unsigned a=0x0800,b=0,c=0,d=0;
  __asm__ volatile("pushw %%ds; pushw %%es; lcallw *%4; popw %%es; popw %%ds; cld":"+a"(a),"+b"(b),"+c"(c),"+d"(d):"m"(xms_entry):"esi","edi","cc","memory");
  /* 32 MiB profile has at least 24 MiB before worker creation, including
   * when the desktop already owns its private 4 MiB address space. */
  if((a&65535)<24576)return 8;
  int e=host_bank_capacity(4);if(e)return e;
  e=legacy(K386_CAPACITY,0,4,0);if(e){host_bank_capacity(2);return e;}
 }else{
  for(unsigned id=2;id<4;id++)if(manager_v86_active(id))return 5;
  if(host_occupied()&12)return 5;
  int e=legacy(K386_CAPACITY,2,2,0);if(e)return e;
  e=host_bank_capacity(2);if(e)return e;
 }
 k32_extended.capacity=resource_slots=n;k32_extended.profile=n==4?32:16;
 for(unsigned id=0;id<n;id++)pointers(id);
 for(unsigned id=n;id<4;id++){k32_extended.view[id]=k32_extended.record[id]=k32_extended.usage[id]=k32_extended.frame[id]=0;k32_extended.kind[id]=k32_extended.abi[id]=0;}
 return 0;
}
_Static_assert(sizeof(K32Manager4)==264,"extended mixed manager ABI");
_Static_assert(sizeof(K32Manager)==200,"versioned mixed manager ABI");
