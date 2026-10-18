#include "k386_client.h"
#include "hardware.h"
static unsigned segment,offset,turn;
#define OFF(m) __builtin_offsetof(K386Mailbox,m)
static unsigned word(unsigned at) {return mem_get(segment,offset+at)|(unsigned)mem_get(segment,offset+at+1)<<8;}
static void put(unsigned at,unsigned value) {mem_put(segment,offset+at,value);mem_put(segment,offset+at+1,value>>8);}
int k386_probe(void) {
    unsigned a=0xd386,b=0,di;u16 seg;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"cc","memory");
    if((a&0xffff)!=0x386d)return 0;
    segment=seg;offset=di&0xffff;
    if(word(OFF(version))!=1||word(OFF(size))!=sizeof(K386Mailbox)||word(OFF(ready))!=1){segment=0;return 0;}
    return 1;
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
    unsigned id=turn++&1;
    if(k386_word(id,0)!=K386_RUNNING)return 0;
    unsigned before=k386_word(id,6);
    if(k386_command(K386_STEP,id,0,0))return 0;
    return k386_word(id,6)!=before||k386_word(id,0)!=K386_RUNNING?1u<<id:0;
}
