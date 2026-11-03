#include "k32.h"
int main(void){
 static const char text[]="HELLO32: C SDK in Ring 3\r\n";
 if(k32_write(text,sizeof text-1)!=(int)sizeof text-1)return 90;
 unsigned before=k32_ticks();k32_yield();unsigned after=k32_ticks();
 if(((after-before)&0xffffff)>0x800000)return 91;
 return 42;
}
