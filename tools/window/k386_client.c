#include "k386_client.h"
#include "hardware.h"
#include "../k38632/manager.h"
#include "../k386/extended.h"
static unsigned segment,offset,turn,mixed,capacity=2;
static u32 cached_view[4];
static unsigned word(unsigned);
static void refresh_views(void){
 capacity=mixed==2?word(__builtin_offsetof(K32Manager4,capacity)):2;
 if(capacity!=2&&capacity!=4)capacity=2;
 for(unsigned id=0;id<capacity;id++){unsigned at=(mixed==2?__builtin_offsetof(K32Manager4,view):__builtin_offsetof(K32Manager,view))+id*4;cached_view[id]=word(at)|(u32)word(at+2)<<16;}
}
unsigned k386_capacity(void){return capacity;}
int k386_capacity_request(unsigned n){if(mixed!=2)return 50;return k386_command(K386_CAPACITY,0,n,0);}
static unsigned resources_segment,resources_offset;
#define OFF(m) __builtin_offsetof(K386Mailbox,m)
static unsigned word(unsigned at) {return mem_get(segment,offset+at)|(unsigned)mem_get(segment,offset+at+1)<<8;}
static void put(unsigned at,unsigned value) {mem_put(segment,offset+at,value);mem_put(segment,offset+at+1,value>>8);}
int k386_probe(void) {
    unsigned query=0xd392,zero=0,di_mixed;u16 seg_mixed;
    capacity=2;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(query),"=c"(seg_mixed),"=D"(di_mixed):"b"(zero):"cc","memory");
    if((query&65535)==0x392d){
        segment=seg_mixed;offset=di_mixed&65535;
        if(word(8)==2&&word(10)==sizeof(K32Manager4)&&word(12)==1){
            mixed=2;unsigned at=__builtin_offsetof(K32Manager4,resources);u32 p=word(at)|(u32)word(at+2)<<16;
            resources_segment=p>>4;resources_offset=p&15;
            if(p>=0x10000&&p+sizeof(K386Resources)<=0xa0000){refresh_views();return 1;}
        }
    }
    query=0xd391;
    __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(query),"=c"(seg_mixed),"=D"(di_mixed):"b"(zero):"cc","memory");
    if((query&65535)==0x391d){
        segment=seg_mixed;offset=di_mixed&65535;
        if(word(8)==1&&word(10)==sizeof(K32Manager)&&word(12)==1){
            mixed=1;unsigned at=__builtin_offsetof(K32Manager,resources);u32 p=word(at)|(u32)word(at+2)<<16;
            resources_segment=p>>4;resources_offset=p&15;
            if(p>=0x10000&&p+sizeof(K386Resources)<=0xa0000){refresh_views();return 1;}
        }
    }
    mixed=0;resources_segment=0;
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
    if(!resources_segment||id>=capacity)return 0;
    unsigned seg=resources_segment,at=resources_offset+__builtin_offsetof(K386Resources,usage)+id*sizeof(K386Usage);
    if(mixed==2){unsigned pos=__builtin_offsetof(K32Manager4,usage)+id*4;u32 p=word(pos)|(u32)word(pos+2)<<16;if(p<0x10000||p+sizeof *out>0xa0000)return 0;seg=p>>4;at=p&15;}
    for(unsigned i=0;i<sizeof *out;i++)((unsigned char *)out)[i]=mem_get(seg,at+i);
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
    if(!segment||id>=capacity)return 1;
    put(OFF(command),command);put(OFF(session),id);put(OFF(key),key);
    if(path)for(unsigned i=0;i<(command==K386_VIDEO?12u:128u);i++){mem_put(segment,offset+OFF(path)+i,path[i]);if(!path[i]&&command!=K386_VIDEO)break;}
    unsigned a=mixed==2?0xd392:mixed?0xd391:0xd386,b=1;
    __asm__ volatile("int $0x2f":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");
    if(mixed)refresh_views();
    return word(OFF(error));
}
static u32 view_address(unsigned id){return cached_view[id];}
unsigned k386_kind(unsigned id){return mixed&&id<capacity?word(__builtin_offsetof(K32Manager,kind)+id*2):0;}
int k386_restart(unsigned id){return k386_command(mixed?K32_RESTART:K386_CREATE,id,0,mixed?0:"KSH.EXE");}
unsigned k386_word(unsigned id,unsigned at) {
    if(id>=capacity||at+2>sizeof(K386View))return 0;
    if(!mixed)return word(OFF(view)+id*sizeof(K386View)+at);
    u32 p=view_address(id)+at;if(p<0x10000||p+2>0xa0000)return 0;
    return mem_get(p>>4,p&15)|(unsigned)mem_get(p>>4,(p&15)+1)<<8;
}
unsigned k386_counter(unsigned id) {return k386_word(id,__builtin_offsetof(K386View,counter))|k386_word(id,__builtin_offsetof(K386View,counter)+2)<<16;}
char k386_character(unsigned id,unsigned cell) {
    if(id>=capacity||cell>=K386_SCREEN)return ' ';
    u32 p=mixed?view_address(id):(u32)segment*16+offset+OFF(view)+id*sizeof(K386View);
    p+=__builtin_offsetof(K386View,screen)+cell;
    unsigned c=mem_get(p>>4,p&15);return c<32?' ':c;
}
void k386_screen(unsigned id,char *out) {
    unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
    if(mixed){u32 p=view_address(id)+__builtin_offsetof(K386View,screen);mem_copy(ds,(unsigned)out,p>>4,p&15,K386_SCREEN);}
    else mem_copy(ds,(unsigned)out,segment,offset+OFF(view)+id*sizeof(K386View)+__builtin_offsetof(K386View,screen),K386_SCREEN);
    for(unsigned i=0;i<K386_SCREEN;i++)if((unsigned char)out[i]<32)out[i]=' ';
}
unsigned k386_tick(void) {
    if(!segment)return 0;
    if(resources_segment){
        unsigned flags[4],states[4],changed=0;
        for(unsigned i=0;i<capacity;i++){flags[i]=k386_word(i,6);states[i]=k386_word(i,0);}
        if(k386_command(K386_SCHEDULE,0,8,0))return 0;
        for(unsigned i=0;i<capacity;i++)if(flags[i]!=k386_word(i,6)||states[i]!=k386_word(i,0))changed|=1u<<i;
        return changed;
    }
    unsigned id=turn++%capacity;
    unsigned state=k386_word(id,0);
    if(state!=K386_RUNNING&&state!=K386_WAITING)return 0;
    unsigned before=k386_word(id,6);
    if(k386_command(K386_STEP,id,0,0))return 0;
    return k386_word(id,6)!=before||k386_word(id,0)!=state?1u<<id:0;
}
int k386_text(unsigned id,unsigned first,unsigned count,K386TextChunk *out){
 if(id>=capacity||first>=25||!count||count>3||count>25-first||k386_kind(id))return 87;
 int error=k386_command(K386_TEXT,id,first|(count<<8),0);if(error)return error;
 unsigned a=0xd38a,b=0,di;u16 seg;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x38ad)return 50;
 unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));mem_copy(ds,(unsigned)out,seg,di&65535,sizeof *out);
 const char *sig="K386TXT1";for(unsigned i=0;i<8;i++)if(out->signature[i]!=sig[i])return 5;
 return out->version!=1||out->size!=sizeof *out||out->slot!=id||out->first!=first||out->count!=count||out->reserved?5:0;
}

int k386_video(unsigned id,unsigned region,unsigned at,unsigned n,KvRead *out){
 if(id>=capacity||k386_kind(id)||!n||n>KV_READ_BYTES)return 87;
 unsigned args[3]={region,at,n};
 int error=k386_command(K386_VIDEO,id,0,(const char *)args);if(error)return error;
 unsigned a=0xd38a,b=0,di;u16 seg;
 __asm__ volatile("pushw %%es; int $0x2f; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=D"(di):"b"(b):"edx","esi","cc","memory");
 if((a&65535)!=0x38ad)return 50;
 unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));mem_copy(ds,(unsigned)out,seg,di&65535,sizeof *out);
 KvPacket *p=&out->packet;
 return p->version!=1||p->size!=sizeof *p||p->slot!=id||p->operation!=4||p->port!=region||p->value!=at||p->width!=n||p->result?5:0;
}
