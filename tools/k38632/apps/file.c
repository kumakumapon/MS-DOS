#include "k32x.h"
int main(int argc,char **argv){
 unsigned char *p=k32_alloc(8192);if(!p)return 1;
 int h=k32_open("DATA32.BIN");if(h<0)return 2;
 if(argc>1){int hs[8];hs[0]=h;for(unsigned j=1;j<8;j++)if((hs[j]=k32_open("DATA32.BIN"))<0)return 30;
  (void)hs;k32_puts("FILE32 HOLD\r\n");if(argv[1][0]=='f')*(volatile unsigned *)0x10000=1;for(;;)k32_wait(1,0);}
 p[0]=0xa5;p[18]=0x5a;if(k32_read(h,p+1,17)!=17||p[0]!=0xa5||p[18]!=0x5a)return 3;
 for(unsigned j=0;j<17;j++)if(p[j+1]!=(unsigned char)j)return 4;
 if(k32_seek(h,65535)!=65535||k32_read(h,p,3)!=3)return 5;
 for(unsigned j=0;j<3;j++)if(p[j]!=(65535+j)%251)return 6;
 if(k32_seek(h,70001)!=70001||k32_read(h,p,10)!=0||k32_seek(h,70002)!=-22)return 7;
 if(k32_read(h,(void *)0x800000,1)!=-14||k32_read(h,(void *)0xbfdfff,2)!=-14||k32_read(h,(void *)0xffffffff,2)!=-14||k32_read(h,p,4097)!=-22)return 8;
 if(k32_close(h)||k32_close(h)!=-9||k32_read(h,p,1)!=-9)return 9;
 int hs[8];for(unsigned j=0;j<8;j++)if((hs[j]=k32_open("DATA32.BIN"))<0)return 10;
 if(k32_open("DATA32.BIN")!=-24||k32_close(hs[3]))return 11;
 int newer=k32_open("DATA32.BIN");if(newer<=0||newer==hs[3]||k32_close(hs[3])!=-9)return 12;
 hs[3]=newer;for(unsigned j=0;j<8;j++)if(k32_close(hs[j]))return 13;
 if(k32_open("CON.TXT")!=-22||k32_open("NUL")!=-22||k32_open("../X")!=-22||k32_open("A:X")!=-22||k32_open("MISSING.TXT")!=-2)return 14;
 if(k32_free(p))return 15;
 k32_puts("FILE32: odd copies, seek, tokens and read-only policy OK\r\n");return 0;
}
