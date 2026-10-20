#include "k386_client.h"
#include "hardware.h"
static unsigned segment,offset,turn;
static unsigned resources_segment,resources_offset;
#define OFF(m) __builtin_offsetof(K386Mailbox,m)
static unsigned word(unsigned at) {return mem_get(segment,offset+at)|(unsigned)mem_get(segment,offset+at+1)<<8;}
static void put(unsigned at,unsigned value) {mem_put(segment,offset+at,value);mem_put(segment,offset+at+1,value>>8);}
int k386_probe(void) {
    unsigned a=0xd386,b=0,di;u16 seg;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"cc","memory");
    if((a&0xffff)!=0x386d)return 0;
    segment=seg;offset=di&0xffff;
    if(word(OFF(version))!=1||word(OFF(size))!=sizeof(K386Mailbox)||word(OFF(ready))!=1){segment=0;return 0;}
    a=0xd387;b=0;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"cc","memory");
    if((a&65535)==0x387d){
        resources_segment=seg;resources_offset=di&65535;
        if(mem_get(seg,resources_offset+8)!=1||mem_get(seg,resources_offset+9)!=0||
           (mem_get(seg,resources_offset+10)|(unsigned)mem_get(seg,resources_offset+11)<<8)!=sizeof(K386Resources))resources_segment=0;
    }
    return 1;
}
int k386_resources_available(void){return resources_segment!=0;}
int k386_usage(unsigned id,K386Usage *out){
    if(!resources_segment||id>=K386_SESSIONS)return 0;
    unsigned at=resources_offset+__builtin_offsetof(K386Resources,usage)+id*sizeof(K386Usage);
    for(unsigned i=0;i<sizeof *out;i++)((unsigned char *)out)[i]=mem_get(resources_segment,at+i);
    return 1;
}
int k386_history(unsigned age,K386Event *out){
    if(!resources_segment)return 0;
    unsigned at=resources_offset+__builtin_offsetof(K386Resources,count);
    if(age>=mem_get(resources_segment,at))return 0;
    at=resources_offset+__builtin_offsetof(K386Resources,next);
    unsigned index=(mem_get(resources_segment,at)+K386_EVENTS-1-age)%K386_EVENTS;
    at=resources_offset+__builtin_offsetof(K386Resources,event)+index*sizeof(K386Event);
    for(unsigned i=0;i<sizeof *out;i++)((unsigned char *)out)[i]=mem_get(resources_segment,at+i);
    return 1;
}
void k386_report_line(unsigned row,char *out){
    if(!resources_segment||row>=6){out[0]=0;return;}
    unsigned at=resources_offset+__builtin_offsetof(K386Resources,report)+row*72;
    for(unsigned i=0;i<72;i++)out[i]=mem_get(resources_segment,at+i);
    out[71]=0;
}
int k386_command(unsigned command,unsigned id,unsigned key,const char *path) {
    if(!segment||id>=2)return 1;
    put(OFF(command),command);put(OFF(session),id);put(OFF(key),key);
    if(path)for(unsigned i=0;i<128;i++){mem_put(segment,offset+OFF(path)+i,path[i]);if(!path[i])break;}
    unsigned a=0xd386,b=1;
    __asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");
    return word(OFF(error));
}
unsigned k386_word(unsigned id,unsigned at) {return word(OFF(view)+id*sizeof(K386View)+at);}
unsigned k386_counter(unsigned id) {return k386_word(id,__builtin_offsetof(K386View,counter))|k386_word(id,__builtin_offsetof(K386View,counter)+2)<<16;}
char k386_character(unsigned id,unsigned cell) {unsigned c=mem_get(segment,offset+OFF(view)+id*sizeof(K386View)+__builtin_offsetof(K386View,screen)+cell);return c<32?' ':c;}
unsigned k386_tick(void) {
    if(!segment)return 0;
    if(resources_segment){
        unsigned flags[2],states[2],changed=0;
        for(unsigned i=0;i<2;i++){flags[i]=k386_word(i,6);states[i]=k386_word(i,0);}
        if(k386_command(K386_SCHEDULE,0,0,0))return 0;
        for(unsigned i=0;i<2;i++)if(flags[i]!=k386_word(i,6)||states[i]!=k386_word(i,0))changed|=1u<<i;
        return changed;
    }
    unsigned id=turn++&1;
    unsigned state=k386_word(id,0);
    if(state!=K386_RUNNING&&state!=K386_WAITING)return 0;
    unsigned before=k386_word(id,6);
    if(k386_command(K386_STEP,id,0,0))return 0;
    return k386_word(id,6)!=before||k386_word(id,0)!=state?1u<<id:0;
}
