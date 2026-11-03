#include "k32.h"
static volatile unsigned data[524288];
static const unsigned char blob[70000] __attribute__((used))={[0]=0x3c,[65535]=0xa1,[65536]=0xb2,[69999]=0x4d};
extern void wide_code(void);
int main(void){
 const volatile unsigned char *p=blob;
 if(p[0]!=0x3c||p[65535]!=0xa1||p[65536]!=0xb2||p[69999]!=0x4d)return 81;
 wide_code();
 for(unsigned i=0;i<524288;i++)if(data[i])return 82;
 for(unsigned i=0;i<524288;i++)data[i]=i^0x5a93c6ef;
 unsigned sum=0;
 for(unsigned i=0;i<524288;i++){if(data[i]!=(i^0x5a93c6ef))return 83;sum+=data[i];}
 if(sum!=0xfffc0000)return 84;
 static const char text[]="MEM32: 2 MiB BSS, >64 KiB file, wide code and checksum OK\r\n";
 return k32_write(text,sizeof text-1)==(int)sizeof text-1?0:85;
}
