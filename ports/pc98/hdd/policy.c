/* Original, deliberately restricted PC-98 40 MB data-volume proof policy. */
#include "policy.h"
static u16 word(const u8 *p){return p[0]|(u16)p[1]<<8;}
static u32 dword(const u8 *p){return word(p)|(u32)word(p+2)<<16;}
static int match(const u8 *a,const char *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=(u8)b[i])return 0;return 1;}
int khd_validate(const u8 *part,const u8 *boot,KhdPlan *out){
 if(part[0]!=0xa1||part[1]!=0x91||word(part+6)!=1||part[4]||part[5]||word(part+10)!=1||part[8]||part[9]||word(part+14)!=614||part[12]||part[13]||!match(part+16,"KHDPROOF        ",16))return 1;
 for(unsigned i=32;i<512;i++)if(part[i])return 2;
 if(!match(boot+3,"KHDPRF01",8)||word(boot+510)!=0xaa55||!match(boot+43,"KHDPROOF   ",11)||!match(boot+54,"FAT16   ",8))return 3;
 if(word(boot+11)!=1024||boot[13]!=4||word(boot+14)!=1||boot[16]!=2||word(boot+17)!=512||word(boot+19)!=KHD_SECTORS||boot[21]!=0xf8||word(boot+22)!=20||word(boot+24)!=33||word(boot+26)!=8||dword(boot+28)!=66||dword(boot+32))return 4;
 *out=(KhdPlan){KHD_START,KHD_SECTORS,1024,4,10116};return 0;
}
int khd_range(u32 start,u32 count,u32 buffer){
 if(start>KHD_SECTORS||count>KHD_SECTORS-start)return 1;
 if(!count)return 0;
 if(buffer<0x500||buffer>=0xa0000||count>(0xa0000-buffer)/KHD_BYTES)return 2;
 return 0;
}
