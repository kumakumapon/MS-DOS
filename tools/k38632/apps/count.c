#include "k32x.h"
static volatile struct {unsigned counter,identity,magic,heap;} data;
static void number(char *out,unsigned n){char rev[11];unsigned k=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);unsigned j=0;while(k)out[j++]=rev[--k];out[j]=0;}
int main(int argc,char **argv){
 data.identity=argc>1?(unsigned char)argv[1][0]:'1';data.magic=0x50333221;
 unsigned *p=k32_alloc(8192);if(!p)return 1;data.heap=(unsigned)p;
 p[0]=data.identity;p[1023]=data.identity^0xffffffff;
 if(argc>1&&argv[1][0]=='f')__asm__ volatile("cli");
 k32_puts("COUNT32: private heap and timer preemption\r\n");
 for(;;){
  data.counter++;
  if(!(data.counter&65535)){
   if(p[0]!=data.identity||p[1023]!=(data.identity^0xffffffff))return 2;
   char value[11];number(value,data.counter);k32_puts("\033[HCOUNT32 ");char id=(char)data.identity;k32_write(&id,1);k32_puts("  count ");k32_puts(value);k32_puts("       \r\n");
  }
 }
}
