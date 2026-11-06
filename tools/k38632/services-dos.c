/* Single-owner services. All calls here occur with the caller CR3 and real mode. */
#include "services.h"
#include "directory.h"
#include "../kgui32/sdk/protocol.h"
extern u8 results[1024],loader_buffer[4096];
extern u32 context[19],loader_map[1024],loader_request,xms_entry;
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
 #if defined(K32_HOST) && !defined(K32_GUI)
 f|=loader_plan.features&(KG_FEATURE|K32_FEATURE_DIRECTORY);
 #endif
 return f;
}
static u32 get(unsigned at){return *(u32 *)(results+at);}
static void put(unsigned at,u32 n){*(u32 *)(results+at)=n;}
static void inc(unsigned at){put(at,get(at)+1);}
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
 if(!k32_user_range(loader_map,at,n,write))return -14;
 return transfer(at-K32_BASE,buffer,n,write)?0:-5;
}
int gui_copy_far(u32 at,unsigned segment,unsigned n){
 if(!n||n>32768||(n&1)||!k32_user_range(loader_map,at,n,0))return -14;
 u16 h=*(u16 *)(results+40);u32 base=get(28)-get(32);
 return raw_move(h,base+at-K32_BASE,0,(u32)segment<<16,n)?0:-5;
}
#endif
static void pte(unsigned i,u32 value){
 loader_map[i]=value;
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
 if(loader_plan.abi!=257)return 0;
 u32 end=K32_BASE;
 for(unsigned i=0;i<loader_plan.count;i++){K32Segment *s=&loader_plan.segment[i];u32 at=(s->address+s->memsz+4095)&~4095u;if(at>end)end=at;}
 k32_heap_init(&heap,end,K32_INFO);
 put(768,257);put(772,service_features());put(776,loader_args.argc);put(780,end);put(784,K32_INFO);
 for(unsigned i=0;i<4096;i++)loader_buffer[i]=0;
 u32 *info=(u32 *)loader_buffer;info[0]=0x3149324b;info[1]=257;info[2]=64;info[3]=loader_args.argc;info[4]=K32_INFO+64;info[5]=service_features();info[6]=end;info[7]=K32_INFO;info[8]=4096;info[9]=307200;
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
static int directory(u32 op,u32 b,u32 c,u32 d){
 if(!(loader_plan.features&K32_FEATURE_DIRECTORY))return -38;
 if(op!=14&&(!b||b!=directory_token))return -9;
 if(op==16){directory_token=directory_eof=0;return 0;}
 u32 pointer=op==14?b:c,n=op==14?c:d;
 if(n!=sizeof(K32DirEntry))return -22;
 if(!k32_user_range(loader_map,pointer,n,1))return -14;
 if(op==14){directory_token=(++serial<<4)|15;directory_eof=0;}
 int found=0;
 while(!directory_eof){
  int e=find_entry("A:\\*.*",directory_dta,op!=14);op=15;
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
static int open_file(u32 pointer,u32 n){
 if(!n||n>12)return -22;
 if(!k32_user_range(loader_map,pointer,n,0))return -14;
 char text[12],path[16];if(!transfer(pointer-K32_BASE,text,n,0))return -5;
 if(!k32_filename(text,n,path))return -22;
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
static int read_file(u32 token,u32 pointer,u32 n){
 int i=index(token);if(i<0)return i;if(n>4096)return -22;
 if(!k32_user_range(loader_map,pointer,n,1))return -14;
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
int services_dispatch(void){
 u32 nr=loader_request;loader_request=0;
 if(nr==1)return loader_write();
 if(nr==4)return loader_plan.abi==257?service_features():0;
 if(loader_plan.abi!=257)return -38;
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
 #if defined(K32_HOST) && !defined(K32_GUI)
 case 14:case 15:case 16:return directory(nr,b,c,d);
 case 13:{
  extern int host_graphics(void *,unsigned);
  if(!(loader_plan.features&KG_FEATURE))return -38;
  if(c!=sizeof(KgRequest))return -22;
  if(!k32_user_range(loader_map,b,c,1))return -14;
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
 #if defined(K32_HOST) && !defined(K32_GUI)
 directory_token=directory_eof=0;
 #endif
 for(unsigned i=0;i<K32_FILES;i++)if(files[i].token&&close_file(files[i].token))put(16,23);
 for(unsigned i=0;i<K32_ALLOCS;i++)if(heap.block[i].pages)release(heap.block[i].first);
}
