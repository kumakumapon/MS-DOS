#include "process.h"
#include "io.h"
#include "libc.h"
volatile unsigned process_stats[24] __attribute__((section(".data.stats")))={0x3250434b,1};
static int wait_child(unsigned child,unsigned expected,unsigned reason){for(unsigned i=0;i<4096;i++){unsigned code,why;int e=k32_process_wait(child,&code,&why);if(!e)return code==expected&&why==reason?0:-100;if(e!=-11)return e;k32_wait(K32_WAIT_TIME,16);}return -110;}
static int failure(unsigned line){process_stats[15]=line;k32_printf("PROCTEST FAIL %u\r\n",line);return 1;}
#define CHECK(x) do{if(!(x))return failure(__LINE__);}while(0)
static const K32Io readonly_request={.version=1,.size=sizeof(K32Io),.operation=KP_INFO};
static int boundaries(void){
 K32Io r=k32_process_request(KP_INFO);r.reserved=1;CHECK(k32_process_call(&r)==-22);
 r.reserved=0;r.flags=8;CHECK(k32_process_call(&r)==-22);
 CHECK(k32_process_call((K32Io *)&readonly_request)==-14);
 r=k32_process_request(KP_SPAWN);r.data=k32_info()->reserved[2];r.bytes=1;CHECK(k32_process_call(&r)==-14);
 r=k32_process_request(KP_RECEIVE);r.data=0x800000;r.bytes=64;CHECK(k32_process_call(&r)==-14);
 r=k32_process_request(KP_SPAWN);r.bytes=257;CHECK(k32_process_call(&r)==-22);
 process_stats[9]=6;return 0;
}
static unsigned hash(unsigned value,unsigned char byte){return (value^byte)*16777619u;}
static unsigned peer_number(const char *text){
 unsigned value=0;
 while(*text){
  unsigned digit=(unsigned char)*text++-'0';
  if(digit>9||value>(0x7fffffffu-digit)/10)return 0;
  value=value*10+digit;
 }
 return value;
}
static int peer_probe(int argc,char **argv){
 K32Io info;
 CHECK(!k32_process_info(&info)&&!info.offset);
 process_stats[2]=info.token;
 process_stats[3]=info.hash;
 process_stats[4]=info.length;
 if(!k32_strcmp(argv[1],"receive")){
  CHECK(argc==3);
  unsigned peer=peer_number(argv[2]);
  CHECK(peer&&k32_message_allow_peer(peer)==0);
  process_stats[1]=41;
  unsigned sender=0;char data[6];int result=-11;
  for(unsigned i=0;i<4096&&result==-11;i++){
   result=k32_message_receive(&sender,data,sizeof data);
   if(result==-11)k32_wait(K32_WAIT_TIME,16);
  }
  CHECK(result==6&&sender==peer&&!k32_memcmp(data,"hello",6));
  CHECK(!k32_message_allow_peer(0));
  process_stats[1]=42;
 }else{
  process_stats[1]=40;
  char digits[11];unsigned count=0;
  for(;;){
   int ch=k32_getchar();CHECK(ch>=0);
   if(ch=='\r'||ch=='\n')break;
   CHECK(count<10&&ch>='0'&&ch<='9');digits[count++]=(char)ch;
  }
  digits[count]=0;unsigned target=peer_number(digits);
  CHECK(target&&!k32_message_send(target,"hello",6));
  process_stats[1]=43;
  CHECK(k32_getchar()=='r');
  CHECK(k32_message_send(target,"hello",6)==-1);
  process_stats[1]=44;
 }
 for(;;){
  int key=k32_poll_key();
  if((key&255)=='q')return 0;
  k32_wait(K32_WAIT_TIME,100);
 }
}
int main(int argc,char **argv){
 if(argc>1&&argv[1][0]=='n'){K32Io denied=k32_process_request(KP_INFO);CHECK(k32_process_call(&denied)==-38);process_stats[1]=30;for(;;){int key=k32_poll_key();if(key>=0&&(key&255)=='q')return 0;k32_wait(K32_WAIT_TIME,100);}}
 if(argc>1&&(!k32_strcmp(argv[1],"send")||!k32_strcmp(argv[1],"receive")))return peer_probe(argc,argv);
 if(boundaries())return 1;
 K32Io info;CHECK(!k32_process_info(&info));process_stats[2]=info.token;process_stats[3]=info.hash;process_stats[4]=info.length;unsigned capacity=argc>1&&argv[1][0]=='4'?4:2;
 int child=k32_spawn("KTOOL32.EXE exit 37",0,0);CHECK(child>0);process_stats[5]=(unsigned)child;CHECK(!wait_child((unsigned)child,37,KP_EXIT));CHECK(k32_process_wait((unsigned)child,0,0)==-10);child=k32_spawn("ARGS32.EXE \"two words\" three",0,0);CHECK(child>0);CHECK(!wait_child((unsigned)child,0,KP_EXIT));process_stats[1]=2;
 child=k32_spawn("NOFILE.EXE",0,0);CHECK(child>0);unsigned code=0,reason=0;int status=-11;for(unsigned i=0;i<4096&&status==-11;i++){status=k32_process_wait((unsigned)child,&code,&reason);if(status==-11)k32_wait(K32_WAIT_TIME,16);}CHECK(!status&&code&&reason==KP_START_FAILED);CHECK(k32_spawn("../CON.EXE",0,0)==-22);CHECK(k32_spawn("CON.EXE",0,0)==-13);
 child=k32_spawn("KTOOL32.EXE ipc",0,0);CHECK(child>0);unsigned char message[64],copy[64];for(unsigned i=0;i<64;i++)message[i]=(unsigned char)(i*17);CHECK(!k32_message_send((unsigned)child,message,64));unsigned from=0;status=-11;for(unsigned i=0;i<4096&&status==-11;i++){status=k32_message_receive(&from,copy,64);if(status==-11)k32_wait(K32_WAIT_TIME,16);}CHECK(status==64&&from==(unsigned)child&&!k32_memcmp(message,copy,64));CHECK(!wait_child((unsigned)child,42,KP_EXIT));process_stats[1]=3;
 unsigned peer=0;if(capacity==4){int running=k32_spawn("KTOOL32.EXE count",0,0);CHECK(running>0);peer=(unsigned)running;process_stats[10]=peer;}
 unsigned input[2],output[2];CHECK(!k32_pipe(input)&&!k32_pipe(output));child=k32_spawn("KTOOL32.EXE cat",input[0],output[1]);CHECK(child>0);CHECK(!k32_pipe_close(input[0])&&!k32_pipe_close(output[1]));
 unsigned sent=0,received=0,expected=2166136261u,actual=expected,closed=0,eof=0;unsigned char bytes[256];
 for(unsigned turn=0;turn<65536&&!eof;turn++){
  if(sent<70001){unsigned n=70001-sent;if(n>sizeof bytes)n=sizeof bytes;for(unsigned i=0;i<n;i++)bytes[i]=(unsigned char)((sent+i)*13+7);int e=k32_pipe_write(input[1],bytes,n);CHECK(e>0||e==-11);if(e>0){for(int i=0;i<e;i++)expected=hash(expected,bytes[i]);sent+=(unsigned)e;}}
  if(sent==70001&&!closed){CHECK(!k32_pipe_close(input[1]));closed=1;}
  int n=k32_pipe_read(output[0],bytes,sizeof bytes);CHECK(n>=0||n==-11);if(n>0){for(int i=0;i<n;i++){CHECK(bytes[i]==(unsigned char)((received+(unsigned)i)*13+7));actual=hash(actual,bytes[i]);}received+=(unsigned)n;}else if(!n)eof=1;
  k32_wait(K32_WAIT_TIME,1);
 }
 CHECK(eof&&sent==70001&&received==sent&&actual==expected);CHECK(!k32_pipe_close(output[0]));CHECK(!wait_child((unsigned)child,0,KP_EXIT));process_stats[6]=received;process_stats[7]=actual;if(peer){CHECK(!k32_process_cancel(peer));CHECK(!wait_child(peer,130,KP_CANCELLED));}process_stats[1]=4;
 child=k32_spawn("KTOOL32.EXE wait",0,0);CHECK(child>0);k32_wait(K32_WAIT_TIME,250);CHECK(!k32_process_cancel((unsigned)child));CHECK(!wait_child((unsigned)child,130,KP_CANCELLED));
 child=k32_spawn("KTOOL32.EXE fault",0,0);CHECK(child>0);CHECK(!wait_child((unsigned)child,142,KP_FAULT));process_stats[1]=5;
 unsigned children[3];for(unsigned i=0;i<capacity-1;i++){child=k32_spawn("KTOOL32.EXE wait",0,0);CHECK(child>0);children[i]=(unsigned)child;k32_wait(K32_WAIT_TIME,250);}
 child=k32_spawn("KTOOL32.EXE wait",0,0);CHECK(child>0);CHECK(!wait_child((unsigned)child,11,KP_START_FAILED));for(unsigned i=0;i<capacity-1;i++){CHECK(!k32_process_cancel(children[i]));CHECK(!wait_child(children[i],130,KP_CANCELLED));}
 process_stats[1]=6;k32_printf("PROCTEST PASS: lifecycle IPC binary pipe EOF fault cancellation capacity\r\n");
 if(argc>2){child=k32_spawn("KTOOL32.EXE wait",0,0);CHECK(child>0);process_stats[8]=(unsigned)child;process_stats[1]=20;k32_wait(K32_WAIT_TIME,250);if(argv[2][0]=='f')*(volatile unsigned *)k32_info()->reserved[2]=1;}
 for(;;){int k=k32_poll_key();if(k>=0&&(k&255)=='q')return 0;k32_wait(K32_WAIT_TIME,100);}
}
