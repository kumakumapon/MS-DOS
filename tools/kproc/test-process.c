#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "process.h"
static KpState s;static KpIdentity slots[4];static unsigned generation[4],launches,cancels;static int failed,cancel_failures;
static int inspect(void *unused,unsigned n,KpIdentity *out){(void)unused;*out=slots[n];return 1;}
static int launch(void *unused,const char *command,unsigned streams,KpIdentity *out){(void)unused;(void)streams;assert(command[0]);launches++;if(failed)return failed;for(unsigned i=0;i<4;i++)if(!slots[i].state){slots[i]=(KpIdentity){i,++generation[i],0x600000+i*0x400000,123+generation[i],KP_RUNNING,0,0};*out=slots[i];return 0;}return -11;}
static int cancel(void *unused,unsigned n){(void)unused;assert(slots[n].state);if(cancel_failures){cancel_failures--;return -5;}slots[n].state=0;cancels++;return 0;}
static KpHost host={0,launch,inspect,cancel};
static K32Io request(unsigned op){return (K32Io){.version=1,.size=sizeof(K32Io),.operation=op};}
static unsigned attach(unsigned i){slots[i]=(KpIdentity){i,++generation[i],0x600000+i*0x400000,123+generation[i],KP_RUNNING,0,0};int t=kp_attach(&s,&slots[i]);assert(t>0);return (unsigned)t;}
static unsigned spawn(unsigned parent,unsigned read,unsigned write){unsigned char command[]="KTOOL32.EXE wait";K32Io r=request(KP_SPAWN);r.data=1;r.bytes=sizeof command;r.flags=(read?1:0)|(write?2:0);r.offset=read;r.exists=write;int e=kp_request(&s,parent,&r,command);if(e)fprintf(stderr,"spawn parent=%u error=%d launches=%u\n",parent,e,launches);assert(!e);return r.token;}
static int call(unsigned owner,unsigned op,unsigned value,unsigned char *data,unsigned n){K32Io r=request(op);r.token=value;r.bytes=n;return kp_request(&s,owner,&r,data);}
int main(void){
 unsigned parent=attach(0),other=attach(3);K32Io r=request(KP_INFO);assert(!kp_request(&s,parent,&r,0)&&r.token==parent&&!r.offset);
 unsigned child=spawn(parent,0,0);assert(call(parent,KP_WAIT,child,0,0)==-11);assert(call(other,KP_CANCEL,child,0,0)==-10);kp_pump(&s,&host);assert(launches==1);
 unsigned cp=kp_attach(&s,&slots[1]);assert(cp==child);unsigned char msg[256]="hello";
 assert(!call(parent,KP_SEND,child,msg,6));r=request(KP_RECEIVE);r.bytes=3;assert(kp_request(&s,child,&r,msg)==-28);r.bytes=64;assert(kp_request(&s,child,&r,msg)==6&&r.token==parent&&!memcmp(msg,"hello",6));
 assert(call(other,KP_SEND,child,msg,6)==-1);for(unsigned i=0;i<KP_MESSAGES;i++)assert(!call(child,KP_SEND,parent,msg,6));assert(call(child,KP_SEND,parent,msg,6)==-11);
 slots[1].state=0;slots[1].code=37;kp_pump(&s,&host);r=request(KP_WAIT);r.token=child;assert(!kp_request(&s,parent,&r,0)&&r.length==37&&r.hash==KP_EXIT);assert(call(parent,KP_WAIT,child,0,0)==-10);
 child=spawn(parent,0,0);assert(!call(parent,KP_CANCEL,child,0,0));kp_pump(&s,&host);assert(launches==1);r=request(KP_WAIT);r.token=child;assert(!kp_request(&s,parent,&r,0)&&r.length==130&&r.hash==KP_CANCELLED);
 failed=-8;child=spawn(parent,0,0);kp_pump(&s,&host);r=request(KP_WAIT);r.token=child;assert(!kp_request(&s,parent,&r,0)&&r.length==8&&r.hash==KP_START_FAILED);failed=0;
 r=request(KP_PIPE);assert(!kp_request(&s,parent,&r,0));unsigned rd=r.token,wr=r.offset;
 child=spawn(parent,rd,wr);kp_pump(&s,&host);assert(kp_attach(&s,&slots[1])==(int)child);
 assert(call(other,KP_WRITE,wr,msg,1)==-9);for(unsigned i=0;i<256;i++)msg[i]=i;
 assert(call(parent,KP_WRITE,wr,msg,256)==256);assert(call(parent,KP_WRITE,wr,msg,256)==256);assert(call(parent,KP_WRITE,wr,msg,1)==-11);
 assert(call(child,KP_READ,0,msg,200)==200);for(unsigned i=0;i<200;i++)assert(msg[i]==i);assert(call(parent,KP_WRITE,wr,msg,200)==200);
 assert(!call(parent,KP_CLOSE,wr,0,0));unsigned childout=0;for(unsigned i=0;i<KP_PROCESSES;i++)if(s.task[i].token==child)childout=s.task[i].out;assert(childout);assert(!call(child,KP_CLOSE,childout,0,0));
 unsigned total=0;int n;while((n=call(child,KP_READ,0,msg,256))>0)total+=n;assert(n==0&&total==512);assert(!call(parent,KP_CLOSE,rd,0,0));
 assert(!call(parent,KP_CANCEL,child,0,0));kp_pump(&s,&host);assert(cancels==1);r=request(KP_WAIT);r.token=child;assert(!kp_request(&s,parent,&r,0)&&r.hash==KP_CANCELLED);for(unsigned i=0;i<KP_PIPES;i++)assert(!s.pipe[i].used);
 r=request(KP_PIPE);assert(!kp_request(&s,parent,&r,0));rd=r.token;wr=r.offset;assert(!call(parent,KP_CLOSE,rd,0,0));assert(call(parent,KP_WRITE,wr,msg,1)==-32);assert(!call(parent,KP_CLOSE,wr,0,0));assert(call(parent,KP_CLOSE,wr,0,0)==-9);
 child=spawn(parent,0,0);kp_pump(&s,&host);slots[1].state=0;kp_pump(&s,&host);unsigned newroot=attach(1);assert(newroot!=child);assert(!call(parent,KP_CANCEL,child,0,0));kp_pump(&s,&host);assert(slots[1].state&&cancels==1);r=request(KP_WAIT);r.token=child;assert(!kp_request(&s,parent,&r,0));
 /* Parent exit cancels its live descendants, but never a recycled slot. */
 child=spawn(parent,0,0);kp_pump(&s,&host);assert(slots[2].state);slots[0].state=0;kp_pump(&s,&host);assert(!slots[2].state&&slots[1].state&&slots[3].state&&cancels==2);for(unsigned i=0;i<KP_PROCESSES;i++)assert(s.task[i].parent!=parent);
 /* Reservation exhaustion and invalid spawn leave caller and live slots intact. */
 parent=newroot;child=spawn(parent,0,0);kp_pump(&s,&host);assert(slots[0].state);slots[0].generation++;slots[0].epoch++;kp_pump(&s,&host);r=request(KP_WAIT);r.token=child;assert(!kp_request(&s,parent,&r,0)&&r.length==116&&r.hash==KP_REPLACED&&slots[0].state&&cancels==2);
 for(unsigned i=0;i<4;i++)(void)spawn(parent,0,0);
 r=request(KP_SPAWN);unsigned char valid[]="KTOOL32.EXE wait";r.bytes=sizeof valid;assert(kp_request(&s,parent,&r,valid)==-11);assert(attach(2)>0);r=request(KP_SPAWN);unsigned char bad[]="../CON.EXE";r.bytes=sizeof bad;assert(kp_request(&s,parent,&r,bad)==-22);s.serial=0x7fffffffu;r=request(KP_PIPE);assert(kp_request(&s,parent,&r,msg)==-75);
 s=(KpState){0};memset(slots,0,sizeof slots);cancels=0;parent=attach(0);other=attach(3);r=request(KP_PIPE);assert(!kp_request(&s,parent,&r,0));rd=r.token;wr=r.offset;child=spawn(parent,rd,wr);kp_pump(&s,&host);
 slots[0].state=KP_RECLAIMING;slots[0].fault=23;slots[0].code=151;cancel_failures=1;kp_pump(&s,&host);assert(slots[0].state==KP_RECLAIMING&&!slots[1].state&&slots[3].state&&cancels==1);assert(s.task[0].token==parent&&s.task[0].fd[0].token);kp_pump(&s,&host);assert(!s.task[0].token&&cancels==2&&slots[3].state);for(unsigned i=0;i<KP_PIPES;i++)assert(!s.pipe[i].used);
 /* Independent roots must opt in, and recycling a slot cannot renew access. */
 s=(KpState){0};memset(slots,0,sizeof slots);parent=attach(0);other=attach(1);
 assert(call(other,KP_SEND,parent,msg,1)==-1);
 assert(!call(parent,KP_ALLOW_PEER,other,0,0));
 assert(!call(other,KP_SEND,parent,msg,1));
 assert(call(parent,KP_SEND,other,msg,1)==-1);
 assert(call(other,KP_CANCEL,parent,0,0)==-10);
 r=request(KP_RECEIVE);r.bytes=1;
 assert(kp_request(&s,parent,&r,msg)==1&&r.token==other);
 slots[1].state=0;kp_pump(&s,&host);unsigned replacement=attach(1);
 assert(replacement!=other&&call(replacement,KP_SEND,parent,msg,1)==-1);
 assert(call(parent,KP_ALLOW_PEER,other,0,0)==-116);
 assert(!call(parent,KP_ALLOW_PEER,replacement,0,0));
 assert(!call(replacement,KP_SEND,parent,msg,1));
 assert(!call(parent,KP_ALLOW_PEER,0,0,0));
 assert(call(replacement,KP_SEND,parent,msg,1)==-1);
 puts("process lifecycle, owned messages, pipe backpressure/EOF/EPIPE, generation isolation and reclamation PASS");
}
