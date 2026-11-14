/* Single-owner services. All calls here occur with the caller CR3 and real mode. */
#include "services.h"
#include "directory.h"
#include "workspace.h"
#include "io.h"
#include "../kgui32/sdk/protocol.h"
extern u8 results[1024],loader_buffer[4096];
extern u32 context[19],loader_request,xms_entry;
extern u8 loader_map[1024];
extern K32Plan loader_plan;
extern K32Args loader_args;
extern int loader_write(void);
static K32Heap heap K32_STATE;
static struct {u32 token,position,size;int handle;} files[K32_FILES] K32_STATE;
static u32 serial K32_STATE,wait_start K32_STATE,wait_ticks K32_STATE,wait_mask K32_STATE;
static u8 dta[43];
static u16 keys[64] K32_STATE;
static unsigned key_head K32_STATE,key_count K32_STATE,escape_pending K32_STATE;
static unsigned service_features(void){
 unsigned f=K32_FEATURES;
 #ifndef K32_GUI
 f|=loader_plan.features&K32_FEATURE_IO;
 #endif
 #if defined(K32_HOST) && !defined(K32_GUI)
 f|=loader_plan.features&(KG_FEATURE|K32_FEATURE_DIRECTORY|K32_FEATURE_SAVE);
 #endif
 return f;
}
#if defined(K32_HOST) && !defined(K32_GUI)
static void workspace_start(void);
#endif
static u32 get(unsigned at){return *(u32 *)(results+at);}
static void put(unsigned at,u32 n){*(u32 *)(results+at)=n;}
static void inc(unsigned at){put(at,get(at)+1);}
#ifndef K32_GUI
/* The resident query returns ES:DI; C string operations require our ES=DS. */
static int io_available(void){u32 a=0xd39f,b=0;__asm__ volatile("pushw %%es; int $0x2f; popw %%es":"+a"(a):"b"(b):"ecx","edx","esi","edi","cc","memory");return (a&65535)==0x39fd;}
#ifdef K32_HOST
static int io_lock(const char *,unsigned);
#endif
#endif
static u32 ticks(void){
 u8 h,m,l,a,b;
 do{__asm__ volatile("inb $0x5f,%0":"=a"(h));__asm__ volatile("inb $0x5d,%0":"=a"(m));__asm__ volatile("inb $0x5c,%0":"=a"(l));__asm__ volatile("inb $0x5d,%0":"=a"(a));__asm__ volatile("inb $0x5f,%0":"=a"(b));}while(a!=m||b!=h);
 return (u32)h<<16|(u32)m<<8|l;
}
static u32 local(const void *p){u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));return (u32)ds<<16|((u32)p&65535);}
static int raw_move(u16 from,u32 at,u16 to,u32 dest,unsigned n){
 struct __attribute__((packed)){u32 n;u16 from;u32 at;u16 to;u32 dest;} m={n,from,at,to,dest};
 u32 a=0x0b00,b=0,d=0,si=(u32)&m;
 __asm__ volatile("pushw %%ds; pushw %%es; lcallw *%5; popw %%es; popw %%ds; cld":"+a"(a),"+b"(b),"+d"(d),"+S"(si):"m"(m),"m"(xms_entry):"ecx","edi","cc","memory");
 return (a&65535)==1;
}
static int transfer(u32 at,void *buffer,unsigned n,int write){
 if(at>0x400000||n>0x400000-at)return 0;
 u16 h=*(u16 *)(results+40);u32 base=get(28)-get(32),even=n&~1u;u8 *p=buffer;
 if(even&&!(write?raw_move(0,local(p),h,base+at,even):raw_move(h,base+at,0,local(p),even)))return 0;
 if(n&1){u8 tail[2];u32 last=at+even,word=last&~1u;
  if(!raw_move(h,base+word,0,local(tail),2))return 0;
  if(write){tail[last&1]=p[even];if(!raw_move(0,local(tail),h,base+word,2))return 0;}else p[even]=tail[last&1];
 }
 return 1;
}
#ifdef K32_GUI
/* Only the dedicated GUI loader exports these copies. Ordinary app hosts do
 * not acquire GUI services or a pointer to the hardware transfer buffer. */
int gui_copy(u32 at,void *buffer,unsigned n,int write){
 if(!k32_user_access(loader_map,at,n,write))return -14;
 return transfer(at-K32_BASE,buffer,n,write)?0:-5;
}
int gui_copy_far(u32 at,unsigned segment,unsigned n){
 if(!n||n>32768||(n&1)||!k32_user_access(loader_map,at,n,0))return -14;
 u16 h=*(u16 *)(results+40);u32 base=get(28)-get(32);
 return raw_move(h,base+at-K32_BASE,0,(u32)segment<<16,n)?0:-5;
}
#endif
static void pte(unsigned i,u32 value){
 loader_map[i]=value&7;
 #ifdef K32_HOST
 if(get(24)){
  extern void host_physical32(u32,u32);host_physical32(get(24)+8192+i*4,value?get(28)+value:0);
 }
#else
 if(get(24)){u32 *table=(u32 *)(get(24)-get(20)+8192);table[i]=value?get(28)+value:0;}
#endif
 inc(848);
}
int services_start(void){
 if(loader_plan.abi<257)return 0;
 #ifndef K32_GUI
 if((loader_plan.features&K32_FEATURE_IO)&&!io_available())return 1;
 #endif
 #if defined(K32_HOST) && !defined(K32_GUI)
 if(loader_plan.features&K32_FEATURE_SAVE)workspace_start();
 #endif
 u32 end=K32_BASE;
 for(unsigned i=0;i<loader_plan.count;i++){K32Segment *s=&loader_plan.segment[i];u32 at=(s->address+s->memsz+4095)&~4095u;if(at>end)end=at;}
 k32_heap_init(&heap,end,loader_plan.heap_limit);
 put(768,loader_plan.abi);put(772,service_features());put(776,loader_args.argc);put(780,end);put(784,loader_plan.heap_limit);
 for(unsigned i=0;i<4096;i++)loader_buffer[i]=0;
 u32 *info=(u32 *)loader_buffer;info[0]=0x3149324b;info[1]=loader_plan.abi;info[2]=64;info[3]=loader_args.argc;info[4]=K32_INFO+64;info[5]=service_features();info[6]=end;info[7]=loader_plan.heap_limit;info[8]=4096;info[9]=307200;
 if(loader_plan.abi==258){info[10]=loader_plan.stack_base;info[11]=loader_plan.stack_top-loader_plan.stack_base;info[12]=loader_plan.stack_base-4096;info[13]=loader_plan.min_heap;info[14]=1;}
 unsigned at=128;
 for(unsigned i=0;i<loader_args.argc;i++){
  *(u32 *)(loader_buffer+64+i*4)=K32_INFO+at;
  for(unsigned j=0;;j++){u8 c=(u8)loader_args.arg[i][j];loader_buffer[at++]=c;if(!c)break;}
 }
 return transfer(0x3fd000,loader_buffer,4096,1)?0:1;
}
static int allocate(u32 bytes){
 unsigned slot;u32 at,pages;int e=k32_heap_find(&heap,bytes,&slot,&at,&pages);if(e){inc(844);return e;}
 for(unsigned i=0;i<4096;i++)loader_buffer[i]=0;
 for(unsigned i=0;i<pages;i++)if(!transfer(at-K32_BASE+i*4096,loader_buffer,4096,1))return -5;
 heap.block[slot]=(K32Allocation){at,pages};
 for(unsigned i=0;i<pages;i++){unsigned p=(at-K32_BASE)/4096+i;pte(p,p*4096|7);}
 put(788,get(788)+pages);if(get(788)>get(792))put(792,get(788));inc(796);return (int)at;
}
static int release(u32 at){
 if(!at)return 0;
 unsigned slot;int e=k32_heap_release(&heap,at,&slot);if(e){inc(844);return e;}
 K32Allocation *b=&heap.block[slot];for(unsigned i=0;i<b->pages;i++)pte((at-K32_BASE)/4096+i,0);
 put(788,get(788)-b->pages);b->pages=0;inc(800);return 0;
}
#ifndef K32_HOST
static int key(int consume){
 u32 a=0x0100,b=0;__asm__ volatile("int $0x18":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");
 if(!(b&0xff00))return -11;
 if(consume){a=0;__asm__ volatile("int $0x18":"+a"(a)::"ebx","ecx","edx","esi","edi","cc","memory");}
 return (int)(a&65535);
}
static void pump_keys(void){
 for(unsigned i=0;i<16;i++){
  int k=key(0);if(k<0)break;(void)key(1);
  if((k&255)==27){escape_pending=1;continue;}
  if(key_count==64){inc(876);continue;}
  keys[(key_head+key_count++)%64]=(u16)k;
 }
 put(872,key_count);
}
#else
static void pump_keys(void){}
int services_key(unsigned k){
 if((k&255)==27){escape_pending=1;return 0;}
 if(key_count==64){inc(876);return 8;}
 keys[(key_head+key_count++)%64]=(u16)k;put(872,key_count);return 0;
}
static u32 paused_wait K32_STATE;
void services_pause(void){paused_wait=(ticks()-wait_start)&0xffffff;if(paused_wait>wait_ticks)paused_wait=wait_ticks;}
void services_resume(void){wait_ticks-=paused_wait;wait_start=ticks();}
#endif
static int poll_key(void){
 inc(804);pump_keys();if(!key_count)return -11;
 unsigned k=keys[key_head];key_head=(key_head+1)%64;key_count--;put(872,key_count);put(852,k);return (int)k;
}
/* Returns one only when input/deadline or Escape has resolved the wait. */
int services_wait(void){
 inc(812);pump_keys();u32 elapsed=(ticks()-wait_start)&0xffffff;
 if(escape_pending){put(12,5);put(68,65);return 1;}
 u32 ready=0;if((wait_mask&1)&&key_count)ready|=1;if((wait_mask&2)&&elapsed>=wait_ticks)ready|=2;
 put(860,elapsed);
 if(!ready)return 0;
 put(856,ready);context[11]=ready;put(12,2);return 1;
}
int services_cancel(void){
 pump_keys();if(escape_pending){put(12,5);put(68,65);return 1;}return 0;
}
static int dos_close(int h){u32 a=0x3e00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h):"cc","memory");return a==~0u?-5:0;}
static int index(u32 token){for(unsigned i=0;i<K32_FILES;i++)if(files[i].token==token&&token)return (int)i;inc(840);return -9;}
static int close_file(u32 token){
 int i=index(token);if(i<0)return i;int e=dos_close(files[i].handle);if(e)return e;
 files[i].token=0;put(820,get(820)-1);inc(836);return 0;
}
/* Save and restore the process DTA on every path. The directory DTA itself
 * lives in the slot snapshot; regular-file checks use separate scratch. */
static int find_entry(const char *name,u8 *target,int next){
 u32 a=0x2f00,old;u16 seg;
 __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(old)::"cc","memory");
 a=0x1a00;__asm__ volatile("int $0x21":"+a"(a):"d"((u32)target):"cc","memory");
 a=next?0x4f00:0x4e00;
 __asm__ volatile("int $0x21; movzwl %%ax,%%eax; jc 1f; xorl %%eax,%%eax; jmp 2f; 1: negl %%eax; 2:":"+a"(a):"d"((u32)name),"c"(0x37):"cc","memory");int result=(int)a;
 a=0x1a00;__asm__ volatile("pushw %%ds; movw %w2,%%ds; int $0x21; popw %%ds":"+a"(a):"d"(old),"r"((u32)seg):"cc","memory");return result;
}
static int regular(const char *name){return !find_entry(name,dta,0)&&!(dta[21]&0x58);}
#if defined(K32_HOST) && !defined(K32_GUI)
static u8 directory_dta[43] K32_STATE;
static u32 directory_token K32_STATE,directory_eof K32_STATE;
static int directory_at(u32 op,u32 b,u32 c,u32 d,const char *pattern){
 if(!(loader_plan.features&K32_FEATURE_DIRECTORY))return -38;
 if(op!=14&&(!b||b!=directory_token))return -9;
 if(op==16){directory_token=directory_eof=0;return 0;}
 u32 pointer=op==14?b:c,n=op==14?c:d;
 if(n!=sizeof(K32DirEntry))return -22;
 if(!k32_user_access(loader_map,pointer,n,1))return -14;
 if(op==14){directory_token=(++serial<<4)|15;directory_eof=0;}
 int found=0;
 while(!directory_eof){
  int e=find_entry(pattern,directory_dta,op!=14);op=15;
  if(e){if(e!=-18&&e!=-2)return -5;directory_eof=1;break;}
  unsigned len=0;while(len<13&&directory_dta[30+len])len++;
  char path[16];
  if(!(directory_dta[21]&8)&&k32_filename((char *)directory_dta+30,len,path)){found=1;break;}
 }
 for(unsigned i=0;i<sizeof(K32DirEntry);i++)loader_buffer[i]=0;
 K32DirEntry *out=(K32DirEntry *)loader_buffer;out->version=1;out->size=sizeof *out;out->cursor=directory_token;
 if(found){out->attributes=directory_dta[21];out->time=*(u16 *)(directory_dta+22);out->date=*(u16 *)(directory_dta+24);out->bytes=*(u32 *)(directory_dta+26);
  for(unsigned i=0;i<13;i++){out->name[i]=(char)directory_dta[30+i];if(!out->name[i])break;}
 }
 return transfer(pointer-K32_BASE,loader_buffer,n,1)?found:-5;
}
#endif
static int open_path(const char *path){
 unsigned slot=K32_FILES;for(unsigned i=0;i<K32_FILES;i++)if(!files[i].token){slot=i;break;}
 if(slot==K32_FILES)return -24;
 if(!regular(path))return -2;
 u32 a=0x3d00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((u32)path):"cc","memory");if(a==~0u)return -2;int h=a&65535;
 u32 d=0;a=0x4202;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(h),"c"(0):"cc","memory");
 if(a==~0u){dos_close(h);return -5;}u32 bytes=(d&65535)<<16|(a&65535);
 a=0x4200;d=0;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(h),"c"(0):"cc","memory");
 if(a==~0u){dos_close(h);return -5;}
 u32 token=(++serial<<4)|(slot+1);files[slot].token=token;files[slot].handle=h;files[slot].position=0;files[slot].size=bytes;
 put(820,get(820)+1);if(get(820)>get(824))put(824,get(820));inc(832);put(864,token);return (int)token;
}
static int open_file(u32 pointer,u32 n){
 if(!n||n>12)return -22;
 if(!k32_user_access(loader_map,pointer,n,0))return -14;
 char text[12],path[16];if(!transfer(pointer-K32_BASE,text,n,0))return -5;
 if(!k32_filename(text,n,path))return -22;
 return open_path(path);
}
static int read_file(u32 token,u32 pointer,u32 n){
 int i=index(token);if(i<0)return i;if(n>4096)return -22;
 if(!k32_user_access(loader_map,pointer,n,1))return -14;
 if(!n)return 0;
 u32 a=0x3f00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(files[i].handle),"c"(n),"d"((u32)loader_buffer):"cc","memory");
 if(a==~0u)return -5;
 unsigned count=a&65535;if(count>n)return -5;
 if(!transfer(pointer-K32_BASE,loader_buffer,count,1))return -5;
 files[i].position+=count;put(828,get(828)+count);return (int)count;
}
static int seek_file(u32 token,u32 at){
 int i=index(token);if(i<0)return i;if(at>files[i].size)return -22;
 u32 a=0x4200,d=at&65535;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(files[i].handle),"c"(at>>16):"cc","memory");
 if(a==~0u||((d&65535)<<16|(a&65535))!=at)return -5;
 files[i].position=at;return (int)at;
}

#include "../filer/verified-disk.h"
#define work_hdd fd_hdd_verified
#define critical fd_disk_critical
/* Shared by classification and the full ELF loader. Never change DOS CWD.
 * A missing bare-name FD file may resolve on the verified data HDD only. */
static int app_open(const char *input){
 char path[64],root[16];unsigned n=0;int bare=1;
 while(input[n]&&n<63){if(input[n]==':'||input[n]=='\\'||input[n]=='/')bare=0;n++;}
 if(input[n]||!n)return -3;
 if(bare){if(!k32_filename(input,n,root))return -3;for(unsigned i=0;i<=n+3;i++)path[i]=root[i];}
 else for(unsigned i=0;i<=n;i++)path[i]=input[i];
 int h=-2;
 for(unsigned attempt=0;attempt<27;attempt++){
  if(attempt){
   if(!bare||h!=-2)break;
   unsigned drive=attempt-1;if(!work_hdd(drive))continue;
   const char *prefix="A:\\APPS\\";unsigned at=0;
   for(;prefix[at];at++){path[at]=prefix[at];}path[0]='A'+drive;
   for(unsigned i=3;root[i];i++){path[at++]=root[i];}path[at]=0;
  }
  u32 a=0x3d00;
  __asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"d"((u32)path):"cc","memory");
  h=(int)a;
  /* Missing APPS on an older marked volume still allows the FD-only profile. */
  if(h<0){if(attempt&&h==-3)h=-2;continue;}
  u32 device=0;a=0x4400;
  __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"=d"(device):"b"(h):"cc","memory");
  if(a!=~0u&&!(device&128))return h;
  a=0x3e00;__asm__ volatile("int $0x21":"+a"(a):"b"(h):"cc","memory");return -5;
 }
 return h;
}
int k32_app_open(const char *input){
 u32 old=critical(1,0);int h=app_open(input);critical(0,old);return h;
}
#if defined(K32_HOST) && !defined(K32_GUI)
static int attributes(const char *path){
 u32 a=0x4300,c=0;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; jmp 2f; 1: movl %%ecx,%%eax; 2:":"+a"(a),"+c"(c):"d"((u32)path):"cc","memory");return (int)a;
}
static K32WorkInfo work_info K32_STATE;
static int work_ready(void){
 int attr=attributes(work_info.root);
 if(attr==-2&&(work_info.flags&K32_WORK_HDD)){
  u32 a=0x3900;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((u32)work_info.root):"cc","memory");
  if(a==~0u)return -5;
  attr=attributes(work_info.root);
 }
 if(attr<0)return attr==-2?-2:-5;
 if(!(attr&16))return -20;
 return attr&0x4f?-13:0;
}
static int work_space(unsigned *free,unsigned *cluster){
 u32 a=0x3600,b,c,d=work_info.drive+1;
 __asm__ volatile("int $0x21":"+a"(a),"=b"(b),"=c"(c),"+d"(d)::"cc","memory");
 if((a&65535)==65535)return -5;
 int e=k32_work_space(&work_info,a&65535,b&65535,c&65535,d&65535);if(e)return e;
 *free=work_info.free_bytes;*cluster=work_info.sector_bytes*work_info.cluster_sectors;return 0;
}
static void workspace_start(void){
 work_info=(K32WorkInfo){.version=1,.size=sizeof work_info,.flags=K32_WORK_FD,.max_bytes=K32_WORK_LIMIT};
 const char *root="A:\\WORK";for(unsigned i=0;i<8;i++)work_info.root[i]=root[i];
 u32 old=critical(1,0);
 for(unsigned drive=0;drive<26;drive++)if(work_hdd(drive)){work_info.drive=drive;work_info.root[0]='A'+drive;work_info.flags=K32_WORK_HDD;break;}
 work_info.error=work_ready();unsigned free,cluster;if(!work_info.error)work_info.error=work_space(&free,&cluster);
 if(!work_info.error)work_info.flags|=K32_WORK_READY;
 critical(0,old);
}
static unsigned work_limit K32_STATE=K32_WORK_LIMIT;
static int work_handle K32_STATE=-1;
static void work_owned(int h){work_handle=h;put(820,get(820)+1);if(get(820)>get(824))put(824,get(820));inc(832);}
static int work_close(int h){int e=dos_close(h);if(e)e=dos_close(h);if(!e){work_handle=-1;put(820,get(820)-1);inc(836);}return e;}
static int work_stat(const char *path,unsigned *bytes,unsigned *hash){
 int attr=attributes(path);if(attr==-2)return 0;if(attr<0)return -5;
 if(attr&0x5f)return -13; /* regular, writable, non-system/non-hidden only */
 u32 a=0x3d00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((u32)path):"cc","memory");if(a==~0u)return -5;
 unsigned h=a&65535,n=0,value=2166136261u;int e=0;work_owned(h);
 for(;;){a=0x3f00;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h),"c"(4096),"d"((u32)loader_buffer):"cc","memory");
  if(a==~0u){e=-5;break;}unsigned got=a&65535;if(got>4096||got>work_limit-n){e=-27;break;}
  for(unsigned i=0;i<got;i++)value=(value^loader_buffer[i])*16777619u;
  n+=got;if(got<4096)break;
 }
 int closed=work_close(h);if(!e)e=closed;if(e)return e;*bytes=n;*hash=value;return 1;
}
static int work_create(const char *path){u32 a=0x5b00;__asm__ volatile("int $0x21; movzwl %%ax,%%eax; jnc 1f; negl %%eax; 1:":"+a"(a):"c"(0),"d"((u32)path):"cc","memory");if((int)a<0)return (int)a==-80?-17:-5;work_owned((int)a);return (int)a;}
static int work_write(int h,const void *p,unsigned n){u32 a=0x4000;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(h),"c"(n),"d"((u32)p):"cc","memory");return a==~0u?-5:(int)(a&65535);}
static int work_rename(const char *from,const char *to){u32 a=0x5600;__asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x21; popw %%es; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((u32)from),"D"((u32)to):"cc","memory");return a==~0u?-5:0;}
static int work_remove(const char *path){u32 a=0x4100;__asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((u32)path):"cc","memory");return a==~0u?-5:0;}
static int work_copy(unsigned at,void *p,unsigned n){return transfer(at-K32_BASE,p,n,0)?0:-5;}
static int workspace(u32 pointer,u32 size){
 if(!(loader_plan.features&K32_FEATURE_SAVE))return -38;
 if(size!=sizeof(K32Work))return -22;
 if(!k32_user_access(loader_map,pointer,size,1))return -14;
 K32Work r;if(!transfer(pointer-K32_BASE,&r,size,0))return -5;
 int e=k32_work_request(&r);if(e)return e;
 if((r.operation==K32_WORK_SAVE||r.operation==K32_WORK_REC_RESTORE_TMP||r.operation==K32_WORK_REC_RESTORE_BAK)&&io_lock(r.name,work_info.drive))return -16;
 work_limit=r.flags&K32_WORK_BMP?K32_BMP_LIMIT:K32_WORK_LIMIT;
 if(work_handle>=0&&r.operation!=K32_WORK_INFO)return -117;
 if((r.operation==K32_WORK_SAVE||r.operation==K32_WORK_REC_RESTORE_TMP||r.operation==K32_WORK_REC_RESTORE_BAK)&&!k32_user_access(loader_map,r.data,r.bytes,0))return -14;
 if((r.operation==K32_WORK_FIRST||r.operation==K32_WORK_INFO||r.operation==K32_WORK_REC_INFO||r.operation==K32_WORK_REC_FIRST_TMP||r.operation==K32_WORK_REC_FIRST_BAK)&&!k32_user_access(loader_map,r.data,r.bytes,1))return -14;
 u32 old=critical(1,0);char path[24];
 e=work_ready();
 if(r.operation==K32_WORK_INFO){
  unsigned free,cluster;int space=work_space(&free,&cluster);work_info.error=work_handle>=0?-117:e?e:space;
  work_info.flags&=~K32_WORK_READY;if(!work_info.error)work_info.flags|=K32_WORK_READY;
  e=transfer(r.data-K32_BASE,&work_info,sizeof work_info,1)?0:-5;goto done;
 }
 if(e)goto done;
 if(r.operation==K32_WORK_FIRST||r.operation==K32_WORK_REC_FIRST_TMP||r.operation==K32_WORK_REC_FIRST_BAK){
  char pattern[]="A:\\WORK\\*.TXT";pattern[0]='A'+work_info.drive;
  const char *ext=r.operation==K32_WORK_REC_FIRST_TMP?"TMP":r.operation==K32_WORK_REC_FIRST_BAK?"BAK":r.flags&K32_WORK_BMP?"BMP":"TXT";
  for(unsigned i=0;i<3;i++)pattern[10+i]=ext[i];
  e=directory_at(14,r.data,r.bytes,0,pattern);goto done;
 }
 if(r.operation>=K32_WORK_REC_INFO){
  const K32WorkIO io={work_stat,work_create,work_write,work_close,work_rename,work_remove,work_copy,work_space};
  if(r.operation==K32_WORK_REC_INFO){K32RecoveryInfo info;e=k32_recovery_info_kind_at(work_info.drive,r.name,r.flags,&info,&io);if(!e)e=transfer(r.data-K32_BASE,&info,sizeof info,1)?0:-5;}
  else if(r.operation==K32_WORK_REC_RESTORE_TMP||r.operation==K32_WORK_REC_RESTORE_BAK){K32RecoveryRestore restore;if(!transfer(r.data-K32_BASE,&restore,sizeof restore,0))e=-5;else if(io_lock(restore.destination,work_info.drive))e=-16;else e=k32_recovery_restore_kind_at(work_info.drive,r.name,r.operation-K32_WORK_REC_RESTORE_TMP+1,r.flags,&restore,&io);}
  else{unsigned kind=r.operation-K32_WORK_REC_OPEN_TMP+1,bytes,hash;e=k32_work_variant_kind_at(work_info.drive,r.name,kind,r.flags,path);if(!e){e=work_stat(path,&bytes,&hash);if(e>=0)e=e?open_path(path):-2;}}
  goto done;
 }
 k32_work_path_kind_at(work_info.drive,r.name,r.flags,path);
 if(r.operation==K32_WORK_OPEN)e=open_path(path);
 else if(r.operation==K32_WORK_STAT){e=work_stat(path,&r.length,&r.hash);if(e>=0){r.exists=e;e=transfer(pointer-K32_BASE,&r,size,1)?0:-5;}}
 else{const K32WorkIO io={work_stat,work_create,work_write,work_close,work_rename,work_remove,work_copy,work_space};e=k32_work_save_at(work_info.drive,&r,&io,loader_buffer);}
 done:critical(0,old);return e;
}
#endif
#ifndef K32_GUI
static unsigned io_handles K32_STATE;
static int io_broker(K32Io *r,unsigned operation){
 K32IoPacket p={1,sizeof p,get(28),get(56),(unsigned)r,(unsigned)loader_buffer,operation,-16,io_handles};
 unsigned a=0xd39f,b=1,di=(unsigned)&p;
 __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x2f; popw %%es":"+a"(a),"+b"(b),"+D"(di)::"ecx","edx","esi","cc","memory");
 if((a&65535)!=0x39fd)return -38;
 if(operation!=3){put(820,get(820)-io_handles+p.handles);io_handles=p.handles;if(get(820)>get(824))put(824,get(820));}
 return p.result;
}
#ifdef K32_HOST
static int io_lock(const char *name,unsigned drive){
 if(!io_available())return 0;
 K32Io r={.version=1,.size=sizeof r,.offset=drive};for(unsigned i=0;i<16;i++)r.name[i]=name[i];
 return io_broker(&r,3)==-16;
}
#endif
static int io_dispatch(u32 pointer,unsigned n){
 if(!(loader_plan.features&K32_FEATURE_IO))return -38;
 if(n!=sizeof(K32Io))return -22;
 if(!k32_user_access(loader_map,pointer,n,1))return -14;
 K32Io r;if(!transfer(pointer-K32_BASE,&r,n,0))return -5;
 if(r.bytes>4096&&(r.operation==K32_IO_READ||r.operation==K32_IO_WRITE||r.operation==K32_IO_RECOVER))return -22;
 unsigned reading=r.operation==K32_IO_READ;
 unsigned payload=reading||r.operation==K32_IO_WRITE||r.operation==K32_IO_RECOVER;
 if(payload&&!k32_user_access(loader_map,r.data,r.bytes,reading))return -14;
 if(payload&&!reading&&r.bytes&&!transfer(r.data-K32_BASE,loader_buffer,r.bytes,0))return -5;
 int result=io_broker(&r,1);
 if(reading&&result>0){if((unsigned)result>r.bytes||!transfer(r.data-K32_BASE,loader_buffer,(unsigned)result,1))return -5;}
 if(!transfer(pointer-K32_BASE,&r,n,1))return -5;
 return result;
}
#endif
int services_dispatch(void){
 u32 nr=loader_request;loader_request=0;
 if(nr==1)return loader_write();
 if(nr==4)return loader_plan.abi>=257?service_features():0;
 if(loader_plan.abi<257)return -38;
 u32 b=context[8],c=context[10],d=context[9];
 switch(nr){
 case 5:return allocate(b);
 case 6:return release(b);
 case 7:return poll_key();
 case 8:
  if(!b||(b&~3u)||c>30000)return -22;
  wait_start=ticks();wait_mask=b;wait_ticks=c*3072/10;inc(808);put(12,7);return 0;
 case 9:return open_file(b,c);
 case 10:return read_file(b,c,d);
 case 11:return seek_file(b,c);
 case 12:return close_file(b);
 #ifndef K32_GUI
 case 18:return io_dispatch(b,c);
 #endif
 #if defined(K32_HOST) && !defined(K32_GUI)
 case 14:case 15:case 16:return directory_at(nr,b,c,d,"A:\\*.*");
 case 17:return workspace(b,c);
 case 13:{
  extern int host_graphics(void *,unsigned);
  if(!(loader_plan.features&KG_FEATURE))return -38;
  if(c!=sizeof(KgRequest))return -22;
  if(!k32_user_access(loader_map,b,c,1))return -14;
  if(!transfer(b-K32_BASE,loader_buffer,c,0))return -5;
  int e=host_graphics(loader_buffer,1);
  if(e>=0&&!transfer(b-K32_BASE,loader_buffer,c,1))return -5;
  return e;
 }
 #endif
 default:return -38;
 }
}
void services_close(void){
 #ifndef K32_GUI
 if((loader_plan.features&K32_FEATURE_IO)&&io_broker(0,2)<0&&io_handles)put(16,23);
 #endif
 #if defined(K32_HOST) && !defined(K32_GUI)
 directory_token=directory_eof=0;
 if(work_handle>=0)work_close(work_handle);
 #endif
 for(unsigned i=0;i<K32_FILES;i++)if(files[i].token&&close_file(files[i].token))put(16,23);
 for(unsigned i=0;i<K32_ALLOCS;i++)if(heap.block[i].pages)release(heap.block[i].first);
}
