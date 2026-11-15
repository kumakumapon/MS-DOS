#include "process.h"
#include "io.h"
#include "libc.h"
volatile unsigned tool_stats[4] __attribute__((section(".data.stats")))={0x32544b50,1};
static int same(const char *a,const char *b){return !k32_strcmp(a,b);}
static unsigned number(const char *s){unsigned n=0;for(unsigned i=0;s[i];i++){if(s[i]<'0'||s[i]>'9'||n>25)return 255;n=n*10+(unsigned)(s[i]-'0');}return n>255?255:n;}
int main(int argc,char **argv){
 const char *mode=argc>1?argv[1]:"cat";
 if(same(mode,"exit"))return argc>2?(int)number(argv[2]):37;
 if(same(mode,"fault")){*(volatile unsigned *)k32_info()->reserved[2]=1;return 99;}
 if(same(mode,"count")){for(;;){tool_stats[2]++;k32_yield();}}
 if(same(mode,"wait")){for(;;)k32_wait(K32_WAIT_TIME,100);}
 if(same(mode,"ipc")){unsigned char message[64];unsigned sender;for(unsigned n=0;n<4096;n++){int e=k32_message_receive(&sender,message,sizeof message);if(e==-11){k32_wait(K32_WAIT_TIME,16);continue;}if(e<0)return 1;for(unsigned at=0;at<4096;at++){int sent=k32_message_send(sender,message,(unsigned)e);if(!sent)return 42;if(sent!=-11)return 2;k32_wait(K32_WAIT_TIME,16);}return 3;}return 4;}
 if(same(mode,"echo")){for(int i=2;i<argc;i++){if(i>2&&k32_stream_write(" ",1)!=1)return 1;unsigned n=k32_strlen(argv[i]);if(k32_stream_write(argv[i],n)!=(int)n)return 2;}return k32_stream_write("\r\n",2)==2?0:3;}
 if(!same(mode,"cat")&&!same(mode,"upper"))return 64;
 unsigned char data[256];
 for(;;){int n=k32_pipe_read(0,data,sizeof data);if(n==-38){int c=k32_getchar();if(c==4)return 0;if(c<0)return 1;data[0]=(unsigned char)c;n=1;}if(n==-11){k32_wait(K32_WAIT_TIME,16);continue;}if(n<0)return 2;if(!n)return 0;
  if(same(mode,"upper"))for(int i=0;i<n;i++)if(data[i]>='a'&&data[i]<='z')data[i]-=32;
  if(k32_stream_write(data,(unsigned)n)!=n)return 3;
 }
}
