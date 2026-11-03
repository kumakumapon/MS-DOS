/* Real-mode PM32 resident service; each call returns after at most one quantum.
 * Task snapshots contain only private trusted state, never the execution stacks.
 */
#include "manager.h"
#include "../window/hardware.h"
extern u8 results[1024],__task_start[],__task_end[];
extern u32 xms_entry,context[19],initialized,loader_request;
extern int loader_open(void),services_dispatch(void),services_wait(void),services_cancel(void),services_key(unsigned);
extern void services_close(void),loader_close(void),services_pause(void),services_resume(void);
extern int host_acquire(void);
extern void host_enter(void),host_drop_xms(void);
K32HostMailbox host_mailbox={.signature={'K','3','2','H','O','S','T','1'},.version=1,.size=sizeof(K32HostMailbox)};
char host_command[128];
extern u16 host_api_kind;
static u8 seed[7168];
static struct {u16 state,tables;u32 generation;} slots[2];
static unsigned active,ds_segment,task_bytes,indos_segment,indos_offset;
static unsigned csi,parameter[2],which;
/* Terminal parser belongs to the task, just like its syscall request. */
static unsigned terminal_state K32_STATE,terminal_parameter[2] K32_STATE,terminal_which K32_STATE;
static u32 get(unsigned n){return *(u32 *)(results+n);}
static void put(unsigned n,u32 x){*(u32 *)(results+n)=x;}
static void copy(void *to,const void *from,unsigned n){u8 *d=to;const u8 *s=from;for(unsigned i=0;i<n;i++)d[i]=s[i];}
static u32 ticks(void){u8 h,m,l,a,b;do{__asm__ volatile("inb $0x5f,%0":"=a"(h));__asm__ volatile("inb $0x5d,%0":"=a"(m));__asm__ volatile("inb $0x5c,%0":"=a"(l));__asm__ volatile("inb $0x5d,%0":"=a"(a));__asm__ volatile("inb $0x5f,%0":"=a"(b));}while(h!=b||m!=a);return (u32)h<<16|(u32)m<<8|l;}
void host_physical32(u32 at,u32 value){for(unsigned i=0;i<4;i++)mem_put(at>>4,(at&15)+i,value>>(i*8));}
static void load(unsigned id){active=id;mem_copy(ds_segment,(unsigned)__task_start,slots[id].state,0,task_bytes);}
static void save(unsigned id){mem_copy(slots[id].state,0,ds_segment,(unsigned)__task_start,task_bytes);}
static unsigned state(void){u32 s=get(12);if(get(16))return K386_FAULTED;if(s==3||s==5||s==6)return K386_ENDED;if(s==4)return K386_FAULTED;return s==7?K386_WAITING:K386_RUNNING;}
static void publish(unsigned id){
 K32HostView *t=&host_mailbox.task[id];K386View *v=&t->view;
 unsigned previous=v->state;
 if(v->state!=K386_STOPPED)v->state=state();
 v->exit_code=get(628);if(get(12)==5)v->exit_code=255;
 v->fault=get(16)?0x8000|get(16):get(12)==4?get(68):0;
 v->ticks=get(60);v->cr3=get(24);v->physical=get(28);v->eip=get(376);v->eflags=get(384);v->cr2=get(72);v->counter=get(468);v->keys=get(872);v->psp=host_mailbox.owner;
 copy(t->record,results,1024);if(previous!=v->state)v->flags++;
}
static int finish(unsigned id){
 services_close();loader_close();
 if(get(820))return 5;
 if(initialized){mem_fill(get(24)>>4,0,12288,0);put(464,1);put(948,1);}
 host_drop_xms();
 publish(id);
 if(*(u16 *)(results+40)||*(u16 *)(results+42)||*(u16 *)(results+142)){save(id);return 5;}
 mem_free(slots[id].tables);mem_free(slots[id].state);slots[id].state=slots[id].tables=0;return 0;
}
static void linefeed(K386View *v){
 if(++v->row<25)return;
 for(unsigned i=0;i<1920;i++)v->screen[i]=v->screen[i+80];
 for(unsigned i=1920;i<2000;i++)v->screen[i]=' ';
 v->row=24;
}
int host_write(const void *buffer,unsigned n){
 const u8 *s=buffer;K386View *v=&host_mailbox.task[active].view;
 csi=terminal_state;parameter[0]=terminal_parameter[0];parameter[1]=terminal_parameter[1];which=terminal_which;
 for(unsigned i=0;i<n;i++){
  unsigned c=s[i];
  if(csi==1){csi=c=='['?2:0;parameter[0]=parameter[1]=which=0;continue;}
  if(csi==2){
   if(c>='0'&&c<='9'){if(parameter[which]<1000)parameter[which]=parameter[which]*10+c-'0';continue;}
   if(c==';'){which=1;continue;}
   if(c=='H'||c=='f'){v->row=parameter[0]?parameter[0]-1:0;v->column=parameter[1]?parameter[1]-1:0;if(v->row>24)v->row=24;if(v->column>79)v->column=79;}
   if(c=='J'){unsigned begin=parameter[0]==2?0:v->row*80+v->column;for(unsigned j=begin;j<2000;j++)v->screen[j]=' ';}
   csi=0;continue;
  }
  if(c==27){csi=1;continue;}
  if(c=='\r'){v->column=0;continue;}if(c=='\n'){linefeed(v);continue;}if(c==8){if(v->column)v->column--;continue;}
  if(c=='\t'){unsigned count=8-(v->column&7);for(unsigned j=0;j<count;j++){v->screen[v->row*80+v->column]=' ';if(++v->column==80){v->column=0;linefeed(v);}}continue;}
  if(c<32||c==127)continue;
  v->screen[v->row*80+v->column]=(char)c;if(++v->column==80){v->column=0;linefeed(v);}
 }
 terminal_state=csi;terminal_parameter[0]=parameter[0];terminal_parameter[1]=parameter[1];terminal_which=which;v->flags++;return (int)n;
}
static int create(unsigned id){
 if(slots[id].state||(manager_v86_active(id)&&!manager_staging(id)))return 5;
 unsigned snapshot=mem_alloc(task_bytes),tables=mem_alloc(16384);
 if(!snapshot||!tables){mem_free(snapshot);mem_free(tables);return 8;}
 slots[id].state=snapshot;slots[id].tables=tables;
 copy(__task_start,seed,task_bytes);active=id;
 u32 pd=((u32)tables*16+4095)&~4095u;
 put(20,(u32)ds_segment*16);put(24,pd);put(44,host_mailbox.owner);put(56,ticks()|1);put(492,~0u);put(504,(unsigned)results);
 copy(host_command,host_mailbox.path,128);host_command[127]=0;
 int e=loader_open();if(e){put(16,20);put(12,6);}
 else {extern K32Plan loader_plan;put(768,loader_plan.abi);if(host_acquire())e=get(16)==8?8:11;}
 if(e){
  services_close();loader_close();host_drop_xms();copy(host_mailbox.failure,results,sizeof host_mailbox.failure);
  if(*(u16 *)(results+40)||get(820)){save(id);host_mailbox.task[id].view.state=K386_FAULTED;publish(id);return 5;}
  mem_free(tables);mem_free(snapshot);slots[id].state=slots[id].tables=0;return e;
 }
 K32HostView *v=&host_mailbox.task[id];for(unsigned i=0;i<sizeof *v;i++)((u8 *)v)[i]=0;
 for(unsigned i=0;i<2000;i++)v->view.screen[i]=' ';
 v->generation=++slots[id].generation;copy(v->command,host_command,128);publish(id);save(id);return 0;
}
int host_operate(unsigned id,unsigned cmd){
 if(cmd==K32_HOST_CREATE)return create(id);
 if(!slots[id].state){if(cmd==K32_HOST_END){host_mailbox.task[id].view.state=K386_ENDED;host_mailbox.task[id].view.flags++;return 0;}return 5;}
 K386View *v=&host_mailbox.task[id].view;load(id);
 if(cmd==K32_HOST_END){put(12,5);put(68,67);v->state=K386_ENDED;return finish(id);}
 if(cmd==K32_HOST_STOP){if(v->state==K386_WAITING)services_pause();v->state=K386_STOPPED;save(id);v->flags++;return 0;}
 if(cmd==K32_HOST_RESUME){if(v->state==K386_STOPPED){if(get(12)==7)services_resume();v->state=state();}publish(id);save(id);return 0;}
 if(cmd==K32_HOST_KEY){int e=services_key(host_mailbox.key);if(v->state!=K386_STOPPED&&get(12)==7)services_wait();if(v->state!=K386_STOPPED&&services_cancel())return finish(id);publish(id);save(id);return e;}
 if(cmd!=K32_HOST_STEP&&cmd!=K32_HOST_POLL)return 87;
 if(v->state==K386_STOPPED)return 0;
 if(services_cancel())return finish(id);
 if(get(12)==7)services_wait();
 else if(cmd==K32_HOST_STEP){
  host_enter();put(636,get(636)+1);
  if(!get(16)&&get(12)==2&&loader_request)context[11]=services_dispatch();
 }
 if(get(16)||get(12)==3||get(12)==4||get(12)==5)return finish(id);
 publish(id);save(id);return 0;
}
int host_install(void){
 u16 msw;u8 mask;__asm__ volatile("smsw %0; inb $2,%1":"=r"(msw),"=a"(mask));if((msw&1)||!(mask&1))return 1;
 u32 a=0x4300;__asm__ volatile("int $0x2f":"+a"(a)::"ebx","ecx","edx","esi","edi","cc","memory");if((a&255)!=128)return 1;
 u16 seg;u32 bx;a=0x4310;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(bx)::"edx","esi","edi","cc","memory");xms_entry=(u32)seg<<16|(bx&65535);
 u16 own_ds;__asm__ volatile("movw %%ds,%0":"=r"(own_ds));ds_segment=own_ds;
 a=0x6200;__asm__ volatile("int $0x21":"+a"(a),"=b"(bx)::"cc","memory");host_mailbox.owner=bx;
 a=0x6700;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(40):"cc","memory");if(a==~0u)return 1;
 a=0x3400;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(bx)::"cc","memory");indos_segment=seg;indos_offset=bx&65535;
 task_bytes=(unsigned)(__task_end-__task_start);if(task_bytes>sizeof seed)return 1;
 copy(seed,__task_start,task_bytes);host_mailbox.cookie=ticks()|1;host_mailbox.ready=1;manager_init(ds_segment);return 0;
}
unsigned host_occupied(void){return (slots[0].state?1u:0)|(slots[1].state?2u:0);}
void host_dispatch(void){
 unsigned request=host_api_kind;
 u16 msw;u8 mask;__asm__ volatile("smsw %0; inb $2,%1":"=r"(msw),"=a"(mask));
 if(host_mailbox.busy||k32_manager.busy||(msw&1)||!(mask&1)||mem_get(indos_segment,indos_offset)){if(request)k32_manager.error=5;else host_mailbox.error=5;return;}
 if(!request&&host_mailbox.session>=2){host_mailbox.error=87;return;}
 host_mailbox.busy=1;unsigned caller,a=0x6200;__asm__ volatile("int $0x21":"+a"(a),"=b"(caller)::"cc","memory");
 a=0x5000;__asm__ volatile("int $0x21":"+a"(a):"b"((unsigned)host_mailbox.owner):"cc","memory");
 if(request){
  k32_manager.busy=1;manager_dispatch();k32_manager.busy=0;
 }else host_mailbox.error=host_operate(host_mailbox.session,host_mailbox.command);
 a=0x5000;__asm__ volatile("int $0x21":"+a"(a):"b"(caller):"cc","memory");host_mailbox.busy=0;
}
