/* Actual CPU bring-up: alternate the independent PM32 and V86 engines in real mode. */
#include "host.h"
#include "../window/hardware.h"
#include "../window/k386_client.h"
#include "../filer/filer.h"
static unsigned seg,off;
volatile struct {char signature[8];u32 version,size,tag,error,iterations;u32 pm[2],pd[2],physical[2],cs[2],flags[2],v86_counter,v86_flags,dos_before,dos_after;} proof={.signature={'K','3','2','M','I','X','0','1'},.version=2,.size=sizeof proof};
static unsigned word(unsigned at){return mem_get(seg,off+at)|(unsigned)mem_get(seg,off+at+1)<<8;}
static void put(unsigned at,unsigned v){mem_put(seg,off+at,v);mem_put(seg,off+at+1,v>>8);}
#define OFF(x) __builtin_offsetof(K32HostMailbox,x)
static unsigned task_at(unsigned id){return OFF(task)+id*sizeof(K32HostView);}
static u32 dword(unsigned at){return word(at)|(u32)word(at+2)<<16;}
static int call(unsigned cmd,unsigned id,const char *path){
 put(OFF(command),cmd);put(OFF(session),id);if(path)for(unsigned i=0;i<128;i++){mem_put(seg,off+OFF(path)+i,path[i]);if(!path[i])break;}
 unsigned a=0xd390,b=1;__asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");return word(OFF(error));
}
static int capture(unsigned id){
 unsigned at=task_at(id),r=at+__builtin_offsetof(K32HostView,record);
 proof.pm[id]=dword(at+__builtin_offsetof(K386View,counter));proof.pd[id]=dword(r+24);proof.physical[id]=dword(r+28);proof.cs[id]=dword(r+380);proof.flags[id]=dword(r+384);
 return word(at)==K386_RUNNING&&dword(r+16)==0&&proof.pm[id]&&proof.cs[id]==59&&!(proof.flags[id]&0x20000)&&dword(r+60)>0&&dword(r+92)==dword(r+96)&&dword(r+100)==dword(r+104)&&word(r+142)==0;
}
static unsigned largest(void){unsigned a=0x4800,b=65535;__asm__ volatile("int $0x21":"+a"(a),"+b"(b)::"cc","memory");return b&65535;}
int main(void){
 p_init();u8 hi,mid,lo;__asm__ volatile("inb $0x5f,%0":"=a"(hi));__asm__ volatile("inb $0x5d,%0":"=a"(mid));__asm__ volatile("inb $0x5c,%0":"=a"(lo));proof.tag=((u32)hi<<16|(u32)mid<<8|lo)|1;proof.dos_before=largest();
 unsigned a=0xd390,b=0,di;u16 es;__asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(es),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x390d||!k386_probe()){proof.error=1;goto failed;}
 seg=es;off=di&65535;
 if(call(K32_HOST_CREATE,0,"SPIN32.EXE")||k386_command(K386_CREATE,1,0,"COUNTER.EXE")){proof.error=2;goto cleanup;}
 for(unsigned i=0;i<64;i++){if(call(K32_HOST_STEP,0,0)||k386_command(K386_STEP,1,0,0)){proof.error=3;goto cleanup;}proof.iterations++;}
 proof.v86_counter=k386_counter(1);proof.v86_flags=k386_word(1,__builtin_offsetof(K386View,eflags))|(u32)k386_word(1,__builtin_offsetof(K386View,eflags)+2)<<16;
 if(!capture(0)||!proof.v86_counter||!(proof.v86_flags&0x20000)){proof.error=4;goto cleanup;}
 call(K32_HOST_END,0,0);k386_command(K386_END,1,0,0);
 if(call(K32_HOST_CREATE,0,"SPIN32.EXE")||call(K32_HOST_CREATE,1,"SPIN32.EXE")){proof.error=5;goto cleanup;}
 for(unsigned i=0;i<64;i++){if(call(K32_HOST_STEP,0,0)||call(K32_HOST_STEP,1,0)){proof.error=6;goto cleanup;}proof.iterations++;}
 if(!capture(0)||!capture(1)||proof.pd[0]==proof.pd[1]||proof.physical[0]==proof.physical[1])proof.error=7;
cleanup:
 call(K32_HOST_END,0,0);call(K32_HOST_END,1,0);k386_command(K386_END,1,0,0);
failed:
 proof.dos_after=largest();if(proof.dos_before!=proof.dos_after)proof.error=8;
 p_output(proof.error?"K32MIX FAIL\r\n":"K32MIX PASS: PM32/V86 and PM32/PM32 actual CPU slices\r\n");return proof.error;
}
