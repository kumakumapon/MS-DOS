/* Pure bounded parser. Read-only callbacks, no DOS, allocation or execution. */
#include "elf.h"
static u16 word(const u8 *p){return p[0]|(u16)p[1]<<8;}
static u32 dword(const u8 *p){return word(p)|(u32)word(p+2)<<16;}
static int magic(const u8 *p,const char *s,unsigned n){for(unsigned i=0;i<n;i++)if(p[i]!=(u8)s[i])return 0;return 1;}
static int range(u32 at,u32 n,u32 end){return at<=end&&n<=end-at;}
int k32_plan(K32Read read,void *opaque,u32 bytes,K32Plan *out){
 u8 mz[96],e[52],p[32];K32Plan plan={0};
 if(bytes<96||!read(opaque,0,mz,96)||word(mz)!=0x5a4d||word(mz+8)!=6||word(mz+6)||word(mz+24)!=96)return 1;
 if(!magic(mz+28,"K386APP1",8)||word(mz+36)!=256||word(mz+38)!=3||!magic(mz+72,"K32EXE01",8)||word(mz+82)!=24)return 2;
 plan.abi=word(mz+80);plan.features=dword(mz+92);
#ifdef K32_GUI
 const unsigned supported=31;
#elif defined(K32_HOST)
 const unsigned supported=47;
#else
 const unsigned supported=15;
#endif
 if((plan.abi!=256&&plan.abi!=257)||(plan.abi==256&&plan.features)||(plan.features&~supported))return 2;
 unsigned title=40;for(;title<72&&mz[title];title++)if(mz[title]<32||mz[title]>126)return 2;
 if(title==40||title==72||!word(mz+4)||word(mz+2)>511||word(mz+12)<word(mz+10))return 2;
 u32 load=(word(mz+4)-1u)*512+(word(mz+2)?word(mz+2):512);
 plan.elf_offset=dword(mz+84);plan.elf_size=dword(mz+88);
 if(load<=96||word(mz+22)*16u+word(mz+20)>=load-96||plan.elf_offset<load||plan.elf_offset>4096||plan.elf_size<52||plan.elf_size>0x400000||!range(plan.elf_offset,plan.elf_size,bytes)||plan.elf_offset+plan.elf_size!=bytes)return 3;
 if(!read(opaque,plan.elf_offset,e,52)||!magic(e,"\177ELF",4)||e[4]!=1||e[5]!=1||e[6]!=1||e[7]||e[8]||word(e+16)!=2||word(e+18)!=3||dword(e+20)!=1||dword(e+36)||word(e+40)!=52||word(e+42)!=32)return 4;
 u32 ph=dword(e+28),number=word(e+44);plan.entry=dword(e+24);
 if(!number||number>8||ph<52||!range(ph,number*32,plan.elf_size))return 5;
 int executable=0;
 for(unsigned i=0;i<number;i++){
  if(!read(opaque,plan.elf_offset+ph+i*32,p,32))return 5;
  u32 type=dword(p);if(!type)continue;if(type!=1)return 6;
  K32Segment s={dword(p+4),dword(p+8),dword(p+16),dword(p+20),dword(p+24)};
  u32 align=dword(p+28);
  if(!s.memsz||s.filesz>s.memsz||!range(s.offset,s.filesz,plan.elf_size)||s.address<K32_BASE||!range(s.address,s.memsz,K32_LIMIT)||s.address%4096||s.offset%4096||align!=4096||!(s.flags&4)||(s.flags&~7u)||((s.flags&3)==3))return 7;
  u32 end=(s.address+s.memsz+4095)&~4095u;
  for(unsigned j=0;j<plan.count;j++){
   K32Segment *old=&plan.segment[j];u32 oldend=(old->address+old->memsz+4095)&~4095u;
   if(s.address<oldend&&old->address<end)return 8;
   if(s.filesz&&old->filesz&&s.offset<old->offset+old->filesz&&old->offset<s.offset+s.filesz)return 8;
  }
  if((s.flags&1)&&plan.entry>=s.address&&plan.entry-s.address<s.filesz)executable=1;
  if((s.flags&2)&&!plan.rwbytes){plan.rwbase=s.address;plan.rwbytes=s.memsz;}
  plan.segment[plan.count++]=s;
 }
 if(!plan.count||!executable)return 9;
 *out=plan;return 0;
}
