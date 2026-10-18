/* Serialized real-mode DOS services. No guest IRQ or DOS call runs recursively. */
#define K386_MONITOR
#include "runtime.h"
#include "loader.h"
#include "../window/hardware.h"
#include "../filer/filer.h"
K386Runtime runtime[2];
static k8 host_dta[43];
static unsigned resident(void){return monitor_base>>4;}
static k16 word(const k8 *p){return p[0]|(k16)p[1]<<8;}
static void physical32(k32 at,k32 value){for(int i=0;i<4;i++)mem_put(at>>4,(at&15)+i,value>>(i*8));}
static void close_files(unsigned id){for(int i=0;i<8;i++)if(sessions[id].handles[i]){p_close(sessions[id].handles[i]-1);sessions[id].handles[i]=0;}}
/* Reject real devices even with extensions. Paths passed to host DOS are absolute. */
static int disk_path(unsigned id,const char *input,char *out,int wild) {
    char name[128];int n=0,last=0;
    if(!input[0])return 3;
    if(input[1]==':'&&(input[0]!='A'&&input[0]!='a'))return 15;
    if(input[1]==':'&&input[2]!='\\'&&input[2]!='/')input+=2;
    for(;input[n]&&n<127;n++){char c=input[n];if(c=='\\'||c=='/')last=n+1;name[n]=(c=='*'||c=='?')?'X':c;}
    if(input[n]||!n)return 3;
    name[n]=0;
    for(int i=0;i<n;i++)if((input[i]=='*'||input[i]=='?')&&(!wild||i<last))return 3;
    if(fd_path(out,runtime[id].cwd[0]?runtime[id].cwd:"A:\\",name)||out[0]!='A')return 3;
    for(int i=3;out[i];) {
        char stem[9];int at=0;
        while(out[i]&&out[i]!='\\'){if(out[i]=='.'){while(out[i]&&out[i]!='\\')i++;break;}if(at<8)stem[at++]=out[i];i++;}
        stem[at]=0;if(!fd_cmp(stem,"COM0")||!fd_cmp(stem,"LPT0"))return 5;
        if(out[i])i++;
    }
    if(wild){int at=fd_len(out)-(n-last);for(int i=last;i<n;i++)if(input[i]=='*'||input[i]=='?')out[at+i-last]=input[i];}
    return 0;
}
static int file_length(int h,k32 *length) {
    unsigned a=0x4202,b=h,c=0,d=0;
    __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:":"+a"(a),"+d"(d):"b"(b),"c"(c):"cc","memory");
    if((int)a<0)return -a;
    *length=((d&65535)<<16)|(a&65535);return p_seek(h,0)<0?11:0;
}
/* Validate headers and every relocation before replacing an existing VM. */
static int prepare(unsigned id,const char *name,int *handle,K386Image *plan) {
    char path[128];int error=disk_path(id,name,path,0);if(error)return error;
    int h=p_open(path);if(h<0)return -h;
    k32 size;k8 header[32]={0};error=file_length(h,&size);
    if(!error){int n=p_read(h,header,32);error=n<0?-n:k386_image(header,size,plan);}
    if(!error&&plan->relocations){
        if(p_seek(h,plan->table)<0)error=11;
        for(unsigned i=0;!error&&i<plan->relocations;i++){
            k8 entry[4];if(p_read(h,entry,4)!=4||!k386_relocation(plan,word(entry),word(entry+2)))error=11;
        }
    }
    if(error){p_close(h);return error;}
    *handle=h;return 0;
}
static int load_bytes(unsigned id,int h,const K386Image *p,const k8 *tail,int child) {
    K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];K386Runtime *r=&runtime[id];
    v->state=K386_ENDED;close_files(id);
    if(!s->allocation)s->allocation=mem_alloc(0x16000);
    if(!s->allocation)return 8;
    s->physical=((k32)s->allocation*16+4095)&~4095u;
    mem_fill(s->physical>>4,0,0x10000,0);
    if(p_seek(h,p->header)<0)return 11;
    for(k32 used=0;used<p->bytes;){
        unsigned wanted=p->bytes-used;if(wanted>sizeof transfer)wanted=sizeof transfer;
        int n=p_read(h,transfer,wanted);if(n!=(int)wanted)return n<0?-n:11;
        mem_copy(s->physical>>4,0x100+used,resident(),(unsigned)transfer,n);used+=n;
    }
    if(p->relocations){
        if(p_seek(h,p->table)<0)return 11;
        for(unsigned i=0;i<p->relocations;i++){
            k8 entry[4];if(p_read(h,entry,4)!=4)return 11;
            k32 at=0x100+(k32)word(entry+2)*16+word(entry);
            k16 value=mem_get(s->physical>>4,at)|(k16)mem_get(s->physical>>4,at+1)<<8;
            value+=0x8010;mem_put(s->physical>>4,at,value);mem_put(s->physical>>4,at+1,value>>8);
        }
    }
    physical32(s->physical,0x900020cd);
    mem_put(s->physical>>4,0x50,0xcd);mem_put(s->physical>>4,0x51,0x21);mem_put(s->physical>>4,0x52,0xcb);
    mem_copy(s->physical>>4,0x80,resident(),(unsigned)tail,tail[0]+2);
    s->frame=(K386Frame){0};s->frame.cs=p->cs;s->frame.eip=p->ip;s->frame.ss=p->ss;s->frame.esp=p->sp;
    s->frame.ds=s->frame.es=K386_SEGMENT;s->frame.eflags=0x20202;s->request=s->line_count=0;
    r->dta=0x80000080;r->vif=1;
    if(!child){
        r->last_exit=0;
        s->head=s->count=0;mem_fill(s->physical>>4,0x10000,0x5000,0);
        *v=(K386View){0};
        for(k32 i=0;i<2000;i++){mem_put(s->physical>>4,0x10000+i*2,' ');mem_put(s->physical>>4,0x12000+i*2,0xe1);v->screen[i]=' ';}
    }else mem_fill(s->physical>>4,0x14000,4096,0);
    v->state=K386_RUNNING;v->physical=s->physical;v->fault=0;v->exit_code=0;v->counter=0;
    v->ticks=0;v->eip=p->ip;v->eflags=s->frame.eflags;v->cr2=0;v->keys=s->count;
    k32 pd=page_directory[id];s->pd=pd;v->cr3=pd;
    mem_fill(pd>>4,0,8192,0);physical32(pd,pd+4096+7);
    for(k32 i=0;i<1024;i++)physical32(pd+4096+i*4,i*4096+3);
    physical32(pd+4096,s->physical+0x14000+7);
    for(k32 i=0;i<16;i++)physical32(pd+4096+(0x80+i)*4,s->physical+i*4096+7);
    for(k32 i=0;i<4;i++)physical32(pd+4096+(0xa0+i)*4,s->physical+0x10000+i*4096+7);
    return 0;
}
int k386_start(unsigned id,const char *command) {
    int length=0;while(length<128&&command[length])length++;
    if(!length||length==128)return 3;
    while(*command==' ')command++;
    char path[128];k8 tail[128]={0};int at=0;
    while(command[at]&&command[at]!=' '&&at<127){path[at]=command[at];at++;}path[at]=0;
    int n=0;while(command[at]&&n<126)tail[1+n++]=command[at++];
    if(command[at])return 3;
    tail[0]=n;tail[n+1]=13;
    int h,error;K386Image plan;error=prepare(id,path,&h,&plan);if(error)return error;
    if(runtime[id].saved){mem_free(runtime[id].saved);runtime[id].saved=0;}
    error=load_bytes(id,h,&plan,tail,0);p_close(h);return error;
}
static void restore_parent(unsigned id,int error) {
    K386Runtime *r=&runtime[id];K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    close_files(id);mem_copy(s->physical>>4,0,r->saved,0,0x10000);
    mem_copy(s->physical>>4,0x14000,r->saved,0x10000,4096);mem_free(r->saved);r->saved=0;
    s->frame=r->parent;s->frame.eax=error;s->frame.eflags=(s->frame.eflags&~1u)|(error?1:0);
    s->line_count=s->request=0;r->dta=r->parent_dta;r->vif=r->parent_vif;v->state=K386_RUNNING;v->fault=0;
    v->eip=s->frame.eip;v->eflags=s->frame.eflags;v->cr2=0;v->counter=0;
}
void k386_reap(unsigned id) {
    K386Runtime *r=&runtime[id];K386View *v=&k386_mailbox.view[id];
    if(v->state!=K386_ENDED&&v->state!=K386_FAULTED)return;
    if(r->saved){r->last_exit=v->state==K386_FAULTED?0x200|(v->fault&255):v->exit_code;restore_parent(id,0);}
    else close_files(id);
}
void k386_release(unsigned id) {
    close_files(id);if(runtime[id].saved)mem_free(runtime[id].saved);
    if(sessions[id].allocation)mem_free(sessions[id].allocation);
    runtime[id]=(K386Runtime){0};sessions[id]=(K386Session){0};
}
static int attributes(const char *path,k16 *attrs) {
    if(!fd_cmp(path,"A:\\")){*attrs=0x10;return 0;}
    unsigned a=0x4300,c=0,d=(unsigned)path;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:":"+a"(a),"+c"(c):"d"(d):"cc","memory");
    *attrs=c;return (int)a;
}
static int directory(unsigned id,int next,const char *path) {
    K386Runtime *r=&runtime[id];K386Session *s=&sessions[id];unsigned a=0x2f00,b; k16 seg;
    __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"cc","memory");
    mem_copy(resident(),(unsigned)host_dta,s->physical>>4,((r->dta>>16)*16+(r->dta&65535))-0x80000,43);
    unsigned set=0x1a00,d=(unsigned)host_dta;__asm__ volatile("int $0x21":"+a"(set):"d"(d):"cc","memory");
    a=next?0x4f00:0x4e00;unsigned c=s->frame.ecx&0x37;d=(unsigned)path;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:":"+a"(a):"c"(c),"d"(d):"cc","memory");
    int result=(int)a;
    mem_copy(s->physical>>4,((r->dta>>16)*16+(r->dta&65535))-0x80000,resident(),(unsigned)host_dta,43);
    set=0x1a00;d=b&65535;
    __asm__ volatile("pushw %%ds; movw %w2,%%ds; int $0x21; popw %%ds":"+a"(set):"d"(d),"r"(seg):"cc","memory");
    return result;
}
void k386_broker(unsigned id) {
    K386Session *s=&sessions[id];K386Frame *f=&s->frame;K386Runtime *r=&runtime[id];
    int result=0;unsigned handle=f->ebx&0xffff;char path[128];
    f->eflags&=~1u;
    if(s->request==0x4b){
        k8 tail[128];for(int i=0;i<128;i++)tail[i]=transfer[128+i];
        int h,error;K386Image plan;
        if(r->saved)result=-8;
        else {for(int i=3;i<8;i++)if(s->handles[i])result=-4;}
        if(!result){error=prepare(id,(char *)transfer,&h,&plan);if(error)result=-error;
            else {r->saved=mem_alloc(0x11000);if(!r->saved)result=-8;
                else {mem_copy(r->saved,0,s->physical>>4,0,0x10000);mem_copy(r->saved,0x10000,s->physical>>4,0x14000,4096);
                    r->parent=s->frame;r->parent_dta=r->dta;r->parent_vif=r->vif;
                    error=load_bytes(id,h,&plan,tail,1);if(error){restore_parent(id,error);result=-error;}}
                p_close(h);}
        }
        if(!result){s->request=0;return;}
    } else if(s->request==0x3d){
        int at;for(at=3;at<8&&s->handles[at];at++);
        if(at==8)result=-4;
        else if((result=disk_path(id,(char *)transfer,path,0)))result=-result;
        else {result=p_open(path);if(result>=0){s->handles[at]=result+1;result=at;}}
    } else if(s->request==0x4e||s->request==0x4f){
        if(s->request==0x4e&&(result=disk_path(id,(char *)transfer,path,1)))result=-result;
        else result=directory(id,s->request==0x4f,path);
    } else if(s->request==0x3b||s->request==0x43){
        if((result=disk_path(id,(char *)transfer,path,0)))result=-result;
        else {k16 attrs;result=attributes(path,&attrs);if(!result){
            if(s->request==0x3b){if(!(attrs&0x10))result=-3;else fd_text(r->cwd,path,sizeof r->cwd);}
            else f->ecx=attrs;
        }}
    } else if(s->request==0x2a||s->request==0x2c){
        unsigned a=(unsigned)s->request<<8,c,d;__asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"ebx","cc","memory");
        f->eax=a&0xffff;f->ecx=c&0xffff;f->edx=d&0xffff;s->request=0;return;
    } else if(handle<3||handle>=8||!s->handles[handle])result=-6;
    else if(s->request==0x3e){result=p_close(s->handles[handle]-1);if(result>=0)s->handles[handle]=0;}
    else if(s->request==0x3f){
        unsigned n=f->ecx&0xffff;if(n>sizeof transfer)n=sizeof transfer;result=p_read(s->handles[handle]-1,transfer,n);
        if(result>0)mem_copy(s->physical>>4,((f->ds&0xffff)*16+(f->edx&0xffff))-0x80000,resident(),(unsigned)transfer,result);
    } else if(s->request==0x42){
        unsigned a=0x4200|(f->eax&255),b=s->handles[handle]-1,c=f->ecx&0xffff,d=f->edx&0xffff;
        __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:":"+a"(a),"+d"(d):"b"(b),"c"(c):"cc","memory");
        if((int)a<0)result=(int)a;else{f->eax=a&0xffff;f->edx=d&0xffff;s->request=0;return;}
    }
    if(result<0){f->eax=-result;f->eflags|=1;}else f->eax=result;
    s->request=0;
}
