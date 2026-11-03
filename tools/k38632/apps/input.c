#include "k32x.h"
int main(int argc,char **argv){
 (void)argc;(void)argv;
 if(k32_wait(0,0)!=-22||k32_wait(4,0)!=-22||k32_wait(2,30001)!=-22)return 1;
 if(k32_poll_key()!=-11)return 2;
 unsigned before=k32_ticks();if(k32_wait(2,100)!=2)return 3;
 if(((k32_ticks()-before)&0xffffff)<30720)return 4;
 k32_puts("INPUT32 WAIT\r\n");if(k32_wait(1,0)!=1)return 5;
 int a=k32_poll_key();if((a&255)!='a')return 6;
 k32_puts("INPUT32 NEXT\r\n");if(k32_wait(3,10000)&1){int b=k32_poll_key();if((b&255)!='b')return 7;}else return 8;
 k32_puts("INPUT32: deadline and queued keys OK\r\n");return 0;
}
