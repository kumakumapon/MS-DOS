#include "k32.h"
int main(void){
 if(k32_write((void *)0x10000,1)!=-14)return 71;
 if(k32_write((void *)0xfffffff0,64)!=-14)return 72;
 if(k32_write((void *)0xbfefff,2)!=-14)return 73;
 if(k32_write((void *)0x800000,4097)!=-22)return 74;
 if(k32_call(999,0,0)!=-38)return 75;
 if(k32_write((void *)0,0)!=0)return 76;
 static const char text[]="BADPTR32: invalid buffers rejected, kernel alive\r\n";
 k32_write(text,sizeof text-1);return 0;
}
