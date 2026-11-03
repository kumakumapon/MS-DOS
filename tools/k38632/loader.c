/* Real-mode file and XMS broker. User code never calls DOS directly. */
#include "elf.h"
extern u8 results[1024];
extern u32 xms_entry;
u32 loader_map[1024],loader_entry,loader_rwbase,loader_rwbytes,loader_request,loader_length;
u8 loader_buffer[4096];
static K32Plan plan;
static int file=-1;
static u32 size;
static void put(unsigned at,u32 n){*(u32 *)(results+at)=n;}
static u32 get(unsigned at){return *(u32 *)(results+at);}
static int read_at(void *unused,u32 at,void *buffer,u32 n){
 (void)unused;u32 a=0x4200,d=at&65535,c=at>>16;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(file),"c"(c):"cc","memory");
 if(a==~0u||((d&65535)<<16|(a&65535))!=at)return 0;
 a=0x3f00;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(file),"c"(n),"d"((u32)buffer):"cc","memory");
 return a!=~0u&&(a&65535)==n;
}
void loader_close(void){
 if(file<0)return;
 u32 a=0x3e00;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(file):"cc","memory");
 if(a==~0u)put(16,21);else {file=-1;put(604,0);}
}
int loader_open(void){
 char name[64];unsigned length=0,psp=*(u16 *)(results+44);u8 tail[128];
 for(unsigned i=0;i<128;i++){
  u32 at=0x80+i;u8 value;
  __asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es":"=q"(value):"r"(psp),"r"(at):"memory");tail[i]=value;
 }
 unsigned end=tail[0],first=1;if(end>126)return 1;
 while(first<=end&&tail[first]==' ')first++;
 if(first+2<=end&&tail[first]=='/'&&(tail[first+1]=='T'||tail[first+1]=='t')&&tail[first+2]==' '){put(492,64);first+=3;}
 while(first<=end&&tail[first]==' ')first++;
 for(;first<=end&&tail[first]!=' ';first++){
  if(length>=63||tail[first]<33||tail[first]>126||tail[first]=='"'||tail[first]=='*'||tail[first]=='?')return 1;
  name[length++]=(char)tail[first];
 }
 while(first<=end&&tail[first]==' ')first++;
 if(!length||first<=end)return 1;
 name[length]=0;
 for(unsigned i=0;i<=length;i++)results[704+i]=(u8)name[i];
 u32 a=0x3d00;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"d"((u32)name):"cc","memory");
 if(a==~0u)return 1;
 file=a&65535;put(604,1);
 a=0x4202;u32 d=0;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a),"+d"(d):"b"(file),"c"(0):"cc","memory");
 if(a==~0u){loader_close();return 1;}size=(d&65535)<<16|(a&65535);
 int error=k32_plan(read_at,0,size,&plan);put(576,error);put(580,size);
 if(error){loader_close();return error;}
 loader_entry=plan.entry;loader_rwbase=plan.rwbase;loader_rwbytes=plan.rwbytes;
 put(476,plan.rwbase);put(488,plan.rwbytes);put(584,plan.entry);put(588,plan.count);put(592,plan.rwbase);put(596,plan.rwbytes);
 for(unsigned i=0;i<plan.count;i++){
  K32Segment *s=&plan.segment[i];unsigned firstpage=(s->address-K32_BASE)/4096,pages=(s->memsz+4095)/4096;
  for(unsigned j=0;j<pages;j++)loader_map[firstpage+j]=((firstpage+j)*4096)|((s->flags&2)?7:5);
 }
 loader_map[1023]=0x3ff007;
 return 0;
}
static int move(u32 at,unsigned bytes){
 u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));
 struct __attribute__((packed)){u32 n;u16 from;u32 offset;u16 to;u32 target;} m={bytes,0,(u32)ds<<16|((u32)loader_buffer&65535),*(u16 *)(results+40),get(28)-get(32)+at};
 u32 a=0x0b00,b=0,d=0,si=(u32)&m;
 __asm__ volatile("pushw %%ds; pushw %%es; lcallw *%5; popw %%es; popw %%ds":"+a"(a),"+b"(b),"+d"(d),"+S"(si):"m"(m),"m"(xms_entry):"ecx","edi","cc","memory");
 return (a&65535)==1;
}
int loader_prepare(void){
 for(unsigned i=0;i<4096;i++)loader_buffer[i]=0;
 for(u32 at=0;at<0x400000;at+=4096)if(!move(at,4096)){loader_close();return 1;}
 for(unsigned i=0;i<plan.count;i++){
  K32Segment *s=&plan.segment[i];
  for(u32 at=0;at<s->filesz;){
   unsigned n=s->filesz-at>4096?4096:s->filesz-at;
   if(!read_at(0,plan.elf_offset+s->offset+at,loader_buffer,n)){loader_close();return 2;}
   if(n&1)loader_buffer[n]=0;
   if(!move(s->address-K32_BASE+at,(n+1)&~1u)){loader_close();return 3;}at+=n;
  }
 }
 loader_close();return get(16)?4:0;
}
int loader_write(void){
 u32 a=0x4000;
 __asm__ volatile("int $0x21; jnc 1f; movl $-1,%%eax; 1:":"+a"(a):"b"(1),"c"(loader_length),"d"((u32)loader_buffer):"cc","memory");
 loader_request=0;return a==~0u?-5:(int)(a&65535);
}
