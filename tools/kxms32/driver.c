/* PC-98 real-mode XMS 2 supplement. FDXMS286 still owns all low memory,
 * HMA and A20. Only an explicit >=32 MiB machine enables the high pool. */
#include "policy.h"
#include "../window/hardware.h"
typedef struct {u32 ax,bx,dx,si;u16 ds;} Regs;
Regs kx_regs,kx_call;u32 kx_old_entry,kx_old_vector;extern u32 kx_request;
extern void kx_old(void),kx_api(void),kx_interrupt(void);
extern void kx_copy(u32,u32,u32);
extern u8 resident_end[],inactive_end[];
static KxPool pool;
volatile struct {
 char signature[8];u16 version,size,busy,ready;
 u32 pool_kb,free_kb,largest_kb,calls,allocations,frees,moves,bytes,errors,base,end,last_error;
} kx_record={.signature={'K','X','3','2','M','E','M','1'},.version=1,.size=64,.base=KX32_BASE};
static void old(unsigned fn,unsigned b,unsigned d,unsigned si,unsigned seg){kx_call=(Regs){fn<<8,b,d,si,seg};kx_old();}
static void error(unsigned e){kx_regs.ax=0;kx_regs.bx=(kx_regs.bx&~255u)|e;kx_record.errors++;kx_record.last_error=e;}
static int address(unsigned h,u32 off,u32 n,u32 *out,unsigned *locked){
 *locked=0;
 if(!h){u32 p=(off>>16)*16+(off&65535);if(p>0x110000||n>0x110000-p)return 0;*out=p;return 1;}
 if(h>=KX32_HANDLE)return kx_range(&pool,h,off,n,out);
 old(14,0,h,0,0);if((kx_call.ax&65535)!=1)return 0;
 u32 size=(kx_call.dx&65535)*1024;if(off>size||n>size-off)return 0;
 old(12,0,h,0,0);if((kx_call.ax&65535)!=1)return 0;
 *out=((kx_call.dx&65535)<<16|(kx_call.bx&65535))+off;*locked=h;return 1;
}
static unsigned move(void){
 struct __attribute__((packed)) {u32 bytes;u16 source;u32 from;u16 target;u32 to;} m;
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 if((kx_regs.si&65535)>65536-sizeof m)return 0xa7;
 mem_copy(ds,(unsigned)&m,kx_regs.ds,kx_regs.si&65535,sizeof m);
 if(m.source<KX32_HANDLE&&m.target<KX32_HANDLE){kx_call=kx_regs;kx_old();kx_regs=kx_call;return 0;}
 if(m.bytes&1)return 0xa7;
 u16 msw;__asm__ volatile("smsw %0":"=r"(msw));if(msw&1)return 0x80;
 u32 src,dst;unsigned sl=0,dl=0,e=0;
 if(!address(m.source,m.from,m.bytes,&src,&sl)){e=0xa3;goto done;}
 if(!address(m.target,m.to,m.bytes,&dst,&dl)){e=0xa5;goto done;}
 if(src!=dst&&m.bytes&&src<dst&&dst-src<m.bytes){e=0xa8;goto done;}
 if(src!=dst&&m.bytes&&dst<src&&src-dst<m.bytes){e=0xa8;goto done;}
 old(5,0,0,0,0);if((kx_call.ax&65535)!=1){e=0x82;goto done;}
 for(u32 at=0;src!=dst&&at<m.bytes;){unsigned n=m.bytes-at;if(n>32768)n=32768;kx_copy(dst+at,src+at,n);at+=n;}
 old(6,0,0,0,0);if((kx_call.ax&65535)!=1)e=0x82;
 if(!e){kx_regs.ax=1;kx_record.moves++;kx_record.bytes+=m.bytes;}
 done:if(sl)old(13,0,sl,0,0);if(dl)old(13,0,dl,0,0);return e;
}
void kx_dispatch(void){
 unsigned fn=(kx_regs.ax>>8)&255,h=kx_regs.dx&65535;kx_record.calls++;
 if(fn==8){
  old(8,0,0,0,0);u32 total,big;kx_space(&pool,&total,&big);
  kx_regs.ax=big>(kx_call.ax&65535)?big:kx_call.ax&65535;kx_regs.dx=total+(kx_call.dx&65535);kx_regs.bx&=~255u;
 }else if(fn==9){
  kx_call=kx_regs;kx_old();
  if((kx_call.ax&65535)==1)kx_regs=kx_call;
  else {int v=kx_alloc(&pool,h);if(v<0)error(-v);else{kx_regs.ax=1;kx_regs.dx=v;kx_record.allocations++;}}
 }else if(fn==11){unsigned e=move();if(e)error(e);}
 else if(h>=KX32_HANDLE&&fn>=10&&fn<=15){
  int i=kx_index(&pool,h);if(i<0){error(0xa2);goto done;}KxBlock *b=pool.block+i;
  int e=0;kx_regs.ax=1;
  switch(fn){
  case 10:e=kx_free(&pool,h);if(!e)kx_record.frees++;break;
  case 12:if(b->locks==255)e=-0xac;else{b->locks++;u32 a=KX32_BASE+(u32)b->first*1024;kx_regs.dx=a>>16;kx_regs.bx=a&65535;}break;
  case 13:if(!b->locks)e=-0xaa;else b->locks--;break;
  case 14:{unsigned free=0;for(unsigned j=0;j<KX32_BLOCKS;j++)if(!pool.block[j].used)free++;kx_regs.bx=(unsigned)b->locks<<8|free;kx_regs.dx=b->size;break;}
  case 15:e=kx_resize(&pool,h,kx_regs.bx&65535);break;
  default:e=-0x80;
  }if(e)error(-e);
 }else{kx_call=kx_regs;kx_old();kx_regs=kx_call;}
 done:{u32 total,big;kx_space(&pool,&total,&big);kx_record.free_kb=total;kx_record.largest_kb=big;}
}
void kx_init(void){
 unsigned seg=kx_request>>4,off=kx_request&15;
 mem_put(seg,off+3,0);mem_put(seg,off+4,1);
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 pool=(KxPool){0};
 unsigned end=(unsigned)inactive_end,high=0;
 unsigned cap=0xd38d,di;u16 cs;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(cap),"=c"(cs),"=D"(di)::"ebx","edx","esi","cc","memory");
 if((cap&65535)==0x38dd){
  const char *sig="KX32BOOT";unsigned good=1;for(unsigned i=0;i<8;i++)if(mem_get(cs,(di&65535)+i)!=(u8)sig[i])good=0;
  if(good&&mem_get(cs,(di&65535)+8)==1&&mem_get(cs,(di&65535)+10)==16)high=mem_get(cs,(di&65535)+12)|(unsigned)mem_get(cs,(di&65535)+13)<<8;
 }
 unsigned probe=0xd38c;__asm__ volatile("int $0x2f":"+a"(probe)::"ebx","ecx","edx","esi","edi","cc","memory");
 unsigned found=0x4300;__asm__ volatile("int $0x2f":"+a"(found)::"ebx","ecx","edx","esi","edi","cc","memory");
 if(high>=16&&(probe&65535)!=0x38cd&&(found&255)==128){
  unsigned a=0x4310,b;u16 es;
  __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");
  kx_old_entry=(u32)es<<16|(b&65535);
  if(es){
   pool.size=16384;kx_record.pool_kb=kx_record.free_kb=kx_record.largest_kb=pool.size;kx_record.end=0x2000000;
   a=0x352f;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=b"(b)::"edx","esi","edi","cc","memory");kx_old_vector=(u32)es<<16|(b&65535);
   a=0x252f;__asm__ volatile("int $0x21":"+a"(a):"d"((u32)kx_interrupt):"ebx","ecx","esi","edi","cc","memory");kx_record.ready=1;end=(unsigned)resident_end;
  }
 }
 mem_put(seg,off+14,end);mem_put(seg,off+15,end>>8);mem_put(seg,off+16,ds);mem_put(seg,off+17,ds>>8);
}
_Static_assert(sizeof kx_record==64,"high XMS diagnostic producer");
