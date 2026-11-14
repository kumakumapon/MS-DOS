#include "k32x.h"
#include "libc.h"
volatile unsigned practical_stats[16] __attribute__((section(".data.stats")))={0x3252504b};
static unsigned stack_probe(void){volatile unsigned char bytes[32768];unsigned sum=0;for(unsigned i=0;i<sizeof bytes;i++){bytes[i]=(unsigned char)(i*17+3);sum+=bytes[i];}return sum;}
int main(int argc,char **argv){
 const K32Info *info=k32_info();practical_stats[1]=1;practical_stats[2]=info->abi;practical_stats[3]=info->reserved[0];practical_stats[4]=info->reserved[1];practical_stats[5]=info->heap_base;practical_stats[6]=info->heap_limit;
 if(info->abi!=258||info->reserved[4]!=1||info->reserved[1]!=65536)return 1;
 practical_stats[7]=stack_probe();if(practical_stats[7]!=4177920)return 2;
 unsigned char *p=k32_calloc(1048577,1);if(!p)return 3;
 for(unsigned i=0;i<1048577;i++){if(p[i])return 4;p[i]=(unsigned char)(i|1);}
 if(k32_realloc(p,~0u)||p[65536]!=1||p[1048576]!=1)return 5;
 k32_free_memory(p);practical_stats[8]=1;
 char text[80];int n=k32_snprintf(text,sizeof text,"PRACT32 PASS: stack=%u heap=%u number=%d hex=%x\r\n",info->reserved[1],info->heap_limit-info->heap_base,(-2147483647-1),0x386u);
 if(n<0||k32_write(text,(unsigned)n)!=n)return 6;
 practical_stats[1]=2;
 if(argc>1&&argv[1][0]=='f'){*(volatile unsigned *)info->reserved[2]=1;return 99;}
 if(argc>1)for(;;)k32_wait(K32_WAIT_KEY|K32_WAIT_TIME,1000);
 return 0;
}
