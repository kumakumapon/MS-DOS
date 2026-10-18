#include "abi.h"
#include "../window/hardware.h"
#define OFF(m) __builtin_offsetof(K386Mailbox,m)
#define VOFF(id,m) (OFF(view)+(id)*sizeof(K386View)+__builtin_offsetof(K386View,m))
static unsigned segment,offset,stage;
static void hex(unsigned x){char b[10];for(int i=0;i<8;i++)b[i]="0123456789ABCDEF"[x>>(28-i*4)&15];b[8]=' ';b[9]=0;p_output(b);}
static unsigned word(unsigned at) {return mem_get(segment,offset+at)|(unsigned)mem_get(segment,offset+at+1)<<8;}
static void set(unsigned at,unsigned value) {mem_put(segment,offset+at,value);mem_put(segment,offset+at+1,value>>8);}
static unsigned value(unsigned at) {return word(at)|word(at+2)<<16;}
static int command(unsigned cmd,unsigned id) {
    set(OFF(command),cmd);set(OFF(session),id);
    unsigned a=0xd386,b=1;__asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");
    return word(OFF(error));
}
static void path(const char *s) {unsigned i=0;do{mem_put(segment,offset+OFF(path)+i,s[i]);}while(s[i++]);}
int main(void) {
    unsigned a=0xd386,b=0,di;u16 seg;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"cc","memory");
    if((a&0xffff)!=0x386d){p_output("K386 CHECK: monitor unavailable\r\n");return 1;}
    segment=seg;offset=di&0xffff;
    stage=1;path("COUNTER.COM");if(command(K386_CREATE,0)||command(K386_CREATE,1))goto fail;
    stage=2;for(int i=0;i<8;i++){if(command(K386_STEP,0)||command(K386_STEP,1))goto fail;}
    stage=3;unsigned c0=value(VOFF(0,counter)),c1=value(VOFF(1,counter));
    if(!c0||!c1||value(VOFF(0,physical))==value(VOFF(1,physical))||value(VOFF(0,cr3))==value(VOFF(1,cr3)))goto fail;
    if(!(value(VOFF(0,eflags))&0x20000)||!value(VOFF(0,ticks)))goto fail;
    stage=4;command(K386_STOP,0);
    for(int i=0;i<4;i++){command(K386_STEP,0);command(K386_STEP,1);}
    if(value(VOFF(0,counter))!=c0||value(VOFF(1,counter))<=c1)goto fail;
    stage=5;command(K386_RESUME,0);command(K386_STEP,0);
    if(value(VOFF(0,counter))<=c0)goto fail;
    stage=6;path("FAULT.COM");if(command(K386_CREATE,1))goto fail;
    command(K386_STEP,1);if(word(VOFF(1,state))!=K386_FAULTED||word(VOFF(1,fault))!=14)goto fail;
    c0=value(VOFF(0,counter));command(K386_STEP,0);if(value(VOFF(0,counter))<=c0)goto fail;
    stage=7;path("ECHO.COM");if(command(K386_CREATE,1))goto fail;command(K386_STEP,1);
    if(word(VOFF(1,state))!=K386_WAITING)goto fail;
    set(OFF(key),'Z');command(K386_KEY,1);command(K386_STEP,1);
    if(word(VOFF(1,state))!=K386_WAITING)goto fail;
    stage=8;command(K386_STOP,1);
    for(int i=0;i<K386_KEY_CAPACITY;i++){set(OFF(key),'A'+i%26);if(command(K386_KEY,1))goto fail;}
    if(command(K386_KEY,1)!=8)goto fail;
    command(K386_RESUME,1);
    for(int i=0;i<64&&word(VOFF(1,state))!=K386_WAITING;i++)command(K386_STEP,1);
    if(word(VOFF(1,state))!=K386_WAITING||word(VOFF(1,keys)))goto fail;
    command(K386_END,0);command(K386_END,1);
    p_output("K386 CHECK PASS: V86 x2, IRQ0 preemption, private CR3/RAM, stop/resume, #PF isolation, keyboard\r\n");return 0;
fail:
    p_output("K386 CHECK FAIL stage ");hex(stage);hex(word(OFF(error)));p_output("\r\n");for(int id=0;id<2;id++){hex(word(VOFF(id,state)));hex(word(VOFF(id,fault)));hex(value(VOFF(id,physical)));hex(value(VOFF(id,cr3)));hex(value(VOFF(id,ticks)));hex(value(VOFF(id,counter)));hex(value(VOFF(id,cr2)));hex(value(VOFF(id,eip)));hex(value(VOFF(id,eflags)));hex(word(VOFF(id,exit_code)));p_output("\r\n");}return 1;
}
