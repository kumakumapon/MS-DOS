/* Shared DPB identity and critical-error policy for the V86/PM32 brokers. */
#include "verified-disk.h"
static unsigned char byte(unsigned p){unsigned char c;__asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es":"=q"(c):"r"(p>>4),"r"(p&15):"memory");return c;}
static unsigned word(unsigned p){return byte(p)|(unsigned)byte(p+1)<<8;}
static unsigned dword(unsigned p){return word(p)|word(p+2)<<16;}
int fd_hdd_verified(unsigned drive){
 if(!drive||drive>25)return 0;
 unsigned a=0x3200,b;unsigned short segment;
 __asm__ volatile("pushw %%ds; int $0x21; movw %%ds,%1; popw %%ds":"+a"(a),"=c"(segment),"=b"(b):"d"(drive+1):"cc","memory");
 if(a&255)return 0;
 unsigned dpb=(unsigned)segment*16+(b&65535);if(dpb<0x500||dpb>0x9ffe0)return 0;
 unsigned pointer=dword(dpb+19),driver=(pointer>>16)*16+(pointer&65535);
 if(driver<0x500||driver>0x9ffad)return 0;
 unsigned p=driver+18;const char *signature="KHDDIOP1";
 for(unsigned i=0;i<8;i++)if(byte(p+i)!=(unsigned char)signature[i])return 0;
 return word(p+8)==1&&word(p+10)==64&&word(p+12)==1&&word(p+50)==drive&&
  dword(p+16)==40524&&dword(p+20)==264&&dword(p+24)==162360;
}
__asm__(".pushsection .text\n.code16\nfd_disk_fail:\nmovb $3,%al\niretw\n.code16gcc\n.popsection");
extern void fd_disk_fail(void);
unsigned fd_disk_critical(int install,unsigned previous){
 unsigned a,b;unsigned short seg;
 if(install){a=0x3524;__asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"cc","memory");previous=(unsigned)seg<<16|(b&65535);
  a=0x2524;__asm__ volatile("int $0x21":"+a"(a):"d"((unsigned)fd_disk_fail):"cc","memory");
 }else{a=0x2524;__asm__ volatile("pushw %%ds; movw %w2,%%ds; int $0x21; popw %%ds":"+a"(a):"d"(previous&65535),"r"(previous>>16):"cc","memory");}
 return previous;
}
