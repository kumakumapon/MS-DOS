#include "k32x.h"
int main(int argc,char **argv){
 unsigned char *p=k32_alloc(65537);if(!p)return 1;
 if(argc>1){if(argv[1][0]=='r'){if(k32_free(p))return 10;*(volatile unsigned *)p=1;return 99;}
  if(argv[1][0]=='i'){*(volatile unsigned *)K32_BOOT_ADDRESS=1;return 99;}
  if(argv[1][0]=='f'){*(volatile unsigned *)0x10000=1;return 99;}k32_puts("HEAP32 HOLD\r\n");for(;;)k32_wait(1,0);}
 for(unsigned j=0;j<69632;j++){if(p[j])return 2;p[j]=(unsigned char)(j|1);}
 if(k32_free(p+1)!=-22||k32_free((void *)0x800000)!=-22||k32_free(p)||k32_free(p)!=-22||k32_free(0))return 3;
 unsigned char *q=k32_alloc(65537);if(q!=p)return 4;for(unsigned j=0;j<69632;j++)if(q[j])return 5;
 if(k32_free(q)||k32_alloc(0)||k32_alloc(~0u))return 6;
 void *all[64];unsigned n=0;while(n<64&&(all[n]=k32_alloc(262144)))n++;
 if(n<8||n==64)return 7;
 for(unsigned j=0;j<n;j++)if(k32_free(all[j]))return 8;
 k32_puts("HEAP32: zero reuse, ownership and exhaustion OK\r\n");return 0;
}
