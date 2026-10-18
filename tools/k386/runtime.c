/* DOS runs once, in real mode. Each VM owns XMS RAM and trusted DOS metadata. */
#define K386_MONITOR
#include "runtime.h"
#include "loader.h"
#include "../window/hardware.h"
#include "../filer/filer.h"
K386Runtime runtime[2];
K386Diagnostics k386_diagnostics={.signature={'K','3','8','6','D','O','S','1'},.version=2,.size=sizeof(K386Diagnostics)};
static k8 host_dta[43];
static unsigned resident(void){return monitor_base>>4;}
static k16 word(const k8 *p){return p[0]|(k16)p[1]<<8;}
static void physical32(k32 at,k32 value){for(int i=0;i<4;i++)mem_put(at>>4,(at&15)+i,value>>(i*8));}
static int get(unsigned id,k32 at,void *b,unsigned n){return backing_read(&runtime[id].backing,at,b,n);}
static int put(unsigned id,k32 at,const void *b,unsigned n){return backing_write(&runtime[id].backing,at,b,n);}
static k16 getword(unsigned id,k32 at){k8 b[2]={0};get(id,at,b,2);return word(b);}
static void putword(unsigned id,k32 at,k16 w){k8 b[2]={w,w>>8};put(id,at,b,2);}
static void snapshot(unsigned id){
    k386_diagnostics.frame[id]=sessions[id].frame;
    k32 base=(k32)runtime[id].process.psp*16,n=K386_RAM-base;
    if(n>512)n=512;
    for(unsigned i=0;i<512;i++)k386_diagnostics.memory[id][i]=0;
    get(id,base,k386_diagnostics.memory[id],n);
}
static k8 port(k16 p){k8 v;__asm__ volatile("inb %w1,%0":"=a"(v):"d"(p));return v;}
static k32 ticks(void){k8 hi,mid,lo;do{hi=port(0x5f);mid=port(0x5d);lo=port(0x5c);}while(mid!=port(0x5d)||hi!=port(0x5f));return (k32)hi<<16|(k32)mid<<8|lo;}
void k386_clock(unsigned id){
    K386Runtime *r=&runtime[id];K386View *v=&k386_mailbox.view[id];k32 now=ticks();
    if(!r->clock){r->clock=now;return;}
    k32 elapsed=(now-r->clock)&0xffffff;r->clock=now;
    if(r->timer_armed){
        r->accumulator+=elapsed;k32 count=r->accumulator/3072;r->accumulator%=3072;
        if(count>=r->timer_count){r->timer_count=0;r->timer_armed=0;r->timer_pending=1;}
        else r->timer_count-=count;
        if(count)putword(id,0x58a,r->timer_count);
    }
    if(r->timer_pending&&r->process.vif&&v->state==K386_WAITING)v->state=K386_RUNNING;
}
static void close_token(k16 token){if(token&&token<0xff00)p_close(token-1);}
static void close_files(unsigned id){for(int i=0;i<K386_HANDLES;i++){close_token(sessions[id].handles[i]);sessions[id].handles[i]=0;}}
static int duplicate(k16 token){if(!token)return 0;if(token>=0xff00)return token;unsigned a=0x4500,b=token-1;__asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:":"+a"(a):"b"(b):"cc","memory");return (int)a<0?(int)a:(int)(a&65535)+1;}
static int inherit(unsigned id,k16 *out){for(int i=0;i<K386_HANDLES;i++)out[i]=0;for(int i=0;i<K386_HANDLES;i++){int n=duplicate(sessions[id].handles[i]);if(n<0){for(int j=0;j<i;j++)close_token(out[j]);return -n;}out[i]=n;}return 0;}
static void jft(unsigned id){k8 table[20];for(int i=0;i<20;i++)table[i]=i<K386_HANDLES&&sessions[id].handles[i]?i:255;put(id,(k32)runtime[id].process.psp*16+0x18,table,20);}
static void arenas(unsigned id){K386Arena *a=&runtime[id].arena;for(unsigned i=0;i<a->count;i++){k8 m[16]={0};K386Block *b=&a->block[i];m[0]=i+1==a->count?'Z':'M';m[1]=b->owner;m[2]=b->owner>>8;m[3]=b->size;m[4]=b->size>>8;put(id,(k32)(b->segment-1)*16,m,16);}}
static void mapping(unsigned id){
    k32 pd=page_directory[id];mem_fill(pd>>4,0,12288,0);
    physical32(pd,pd+4096+7);physical32(pd+4,pd+8192+3);
    for(k32 i=0;i<1024;i++){physical32(pd+4096+i*4,i*4096+3);physical32(pd+8192+i*4,i*4096+3);}
    sessions[id].pd=pd;k386_mailbox.view[id].cr3=pd;
}
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
static int prepare(unsigned id,const char *name,int *handle,K386Image *plan,char *path) {
    int error=disk_path(id,name,path,0);if(error)return error;
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
static int environment(unsigned id,k16 source,k16 owner,const char *path,k16 *segment){
    unsigned n=0;int found=0;
    if(source){
        k32 base=(k32)source*16;if(base>=K386_RAM-2048)return 10;
        for(unsigned at=0;at<2048&&!found;at+=128){if(!get(id,base+at,transfer+at,128))return 8;for(unsigned j=at?at:1;j<at+128;j++)if(!transfer[j]&&!transfer[j-1]){n=j+1;found=1;break;}}
        if(!found)return 10;
    }else{static const char defaults[]="COMSPEC=A:\\COMMAND.COM\0PATH=A:\\\0PROMPT=$P$G\0\0";n=sizeof defaults-1;for(unsigned i=0;i<n;i++)transfer[i]=defaults[i];}
    unsigned length=n+2+fd_len(path)+1;transfer[n]=1;transfer[n+1]=0;fd_text((char *)transfer+n+2,path,sizeof transfer-n-2);
    *segment=arena_alloc(&runtime[id].arena,(length+15)/16,owner,0);
    return !*segment||!put(id,(k32)*segment*16,transfer,length)?8:0;
}
static int load_program(unsigned id,int h,const K386Image *p,const k8 *tail,const char *path,k16 env,int root){
    K386Runtime *r=&runtime[id];K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    k16 psp=arena_alloc(&r->arena,p->required,0xffff,root&&p->required<=0x2000?0x8000:0);
    if(!psp)psp=arena_alloc(&r->arena,p->required,0xffff,0);
    if(!psp)return 8;
    for(unsigned i=0;i<r->arena.count;i++)if(r->arena.block[i].owner==0xffff)r->arena.block[i].owner=psp;
    k16 envseg=0;int error=environment(id,env,psp,path,&envseg);k32 base=(k32)psp*16;
    if(!error&&p_seek(h,p->header)<0)error=11;
    for(k32 used=0;!error&&used<p->bytes;){
        unsigned wanted=p->bytes-used;if(wanted>sizeof transfer)wanted=sizeof transfer;
        int n=p_read(h,transfer,wanted);if(n!=(int)wanted){error=n<0?-n:11;break;}
        if(!put(id,base+0x100+used,transfer,n)){error=8;break;}used+=n;
    }
    if(!error&&p->relocations){
        if(p_seek(h,p->table)<0)error=11;
        for(unsigned i=0;!error&&i<p->relocations;i++){k8 entry[4];if(p_read(h,entry,4)!=4){error=11;break;}k32 at=base+0x100+(k32)word(entry+2)*16+word(entry);putword(id,at,getword(id,at)+psp+16);}
    }
    if(error){arena_release(&r->arena,psp);arenas(id);return error;}
    k16 parent=r->process.psp;k8 header[256]={0};header[0]=0xcd;header[1]=0x20;header[2]=psp+p->required;header[3]=(psp+p->required)>>8;
    header[0x16]=parent;header[0x17]=parent>>8;header[0x2c]=envseg;header[0x2d]=envseg>>8;
    header[0x32]=K386_HANDLES;header[0x34]=0x18;header[0x36]=psp;header[0x37]=psp>>8;
    header[0x50]=0xcd;header[0x51]=0x21;header[0x52]=0xcb;
    for(int i=1;i<12;i++)header[0x5c+i]=header[0x6c+i]=' ';
    for(unsigned i=0;i<(unsigned)tail[0]+2;i++)header[0x80+i]=tail[i];
    put(id,base,header,256);if(!p->header)putword(id,base+0xfffc,0);
    r->process=(K386Process){0};r->process.psp=psp;r->process.vif=1;r->process.dta=(k32)psp<<16|0x80;
    s->frame=(K386Frame){0};s->frame.cs=(p->cs+psp-0x8000)&65535;s->frame.eip=p->ip;s->frame.ss=(p->ss+psp-0x8000)&65535;s->frame.esp=p->sp;
    s->frame.ds=s->frame.es=psp;s->frame.edx=psp;s->frame.eflags=0x20202;s->request=s->line_count=0;
    v->state=K386_RUNNING;v->physical=r->backing.physical+base;v->psp=psp;v->fault=v->exit_code=0;v->counter=v->ticks=v->cr2=0;
    v->eip=p->ip;v->eflags=s->frame.eflags;v->keys=s->count;v->flags++;
    arenas(id);jft(id);snapshot(id);return 0;
}
static void discard(unsigned id){K386Runtime *r=&runtime[id];close_files(id);for(unsigned d=0;d<r->depth;d++)for(int i=0;i<K386_HANDLES;i++)close_token(r->parent[d].handles[i]);r->depth=0;}
int k386_start(unsigned id,const char *command){
    int length=0;while(length<128&&command[length])length++;if(!length||length==128)return 3;
    while(*command==' ')command++;
    char name[128],path[128];k8 tail[128]={0};int at=0;
    while(command[at]&&command[at]!=' '&&at<127){name[at]=command[at];at++;}name[at]=0;
    int n=0;while(command[at]&&n<126)tail[1+n++]=command[at++];if(command[at])return 3;tail[0]=n;tail[n+1]=13;
    K386Image plan;int h,error=prepare(id,name,&h,&plan,path);if(error)return error;
    K386Runtime *r=&runtime[id];K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    if(!r->backing.handle&&!backing_new(&r->backing)){p_close(h);return 8;}
    discard(id);r->process=(K386Process){0};arena_init(&r->arena);r->timer_armed=r->timer_pending=r->timer_owner=0;r->clock=r->accumulator=0;s->head=s->count=0;
    *v=(K386View){0};v->state=K386_ENDED;
    if(!backing_zero(&r->backing,0,K386_BACKING)){p_close(h);return 8;}
    k8 bios[256];mem_copy(resident(),(unsigned)bios,0,0x400,256);put(id,0x400,bios,256);
    /* Private NLS data, below the first allocatable DOS MCB. No host pointers. */
    transfer[0]=128;transfer[1]=0;for(unsigned i=0;i<128;i++)transfer[i+2]=i+128;put(id,0x8000,transfer,130);
    static const k8 dbcs[]={0x81,0x9f,0xe0,0xfc,0,0};put(id,0x8200,dbcs,sizeof dbcs);
    putword(id,0x8240,sizeof dbcs);put(id,0x8242,dbcs,sizeof dbcs);
    for(unsigned i=0;i<4000;i++)transfer[i]=0;
    for(unsigned i=0;i<2000;i++){transfer[i*2]=' ';v->screen[i]=' ';}put(id,0xa0000,transfer,4000);
    for(unsigned i=0;i<2000;i++){transfer[i*2]=0xe1;transfer[i*2+1]=0;}put(id,0xa2000,transfer,4000);
    for(int i=0;i<5;i++)s->handles[i]=0xff01+i;
    mapping(id);error=load_program(id,h,&plan,tail,path,0,1);p_close(h);return error;
}
static void restore_parent(unsigned id,int status){
    K386Runtime *r=&runtime[id];K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];k16 ended=r->process.psp;
    close_files(id);arena_release(&r->arena,ended);r->depth--;r->process=r->parent[r->depth];
    backing_copy(&r->backing,0,0xa4000+r->depth*1024,1024);
    for(int i=0;i<K386_HANDLES;i++)s->handles[i]=r->process.handles[i];
    s->frame=r->process.frame;s->frame.eax=0;s->frame.eflags&=~1u;r->process.last_exit=status;s->line_count=s->request=0;
    if(r->timer_owner==ended)r->timer_armed=r->timer_pending=0;
    v->state=K386_RUNNING;v->fault=0;v->psp=r->process.psp;v->physical=r->backing.physical+(k32)r->process.psp*16;v->eip=s->frame.eip;v->eflags=s->frame.eflags;v->cr2=v->counter=0;v->flags++;
    arenas(id);jft(id);snapshot(id);
}
void k386_reap(unsigned id){K386Runtime *r=&runtime[id];K386View *v=&k386_mailbox.view[id];if(v->state!=K386_ENDED&&v->state!=K386_FAULTED)return;if(r->depth)restore_parent(id,v->state==K386_FAULTED?0x200|(v->fault&255):v->exit_code);else close_files(id);}
void k386_release(unsigned id){discard(id);backing_free(&runtime[id].backing);runtime[id]=(K386Runtime){0};sessions[id]=(K386Session){0};}
static int attributes(const char *path,k16 *attrs){if(!fd_cmp(path,"A:\\")){*attrs=0x10;return 0;}unsigned a=0x4300,c=0,d=(unsigned)path;__asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:":"+a"(a),"+c"(c):"d"(d):"cc","memory");*attrs=c;return (int)a;}
static int directory(unsigned id,int next,const char *path){
    K386Runtime *r=&runtime[id];K386Process *p=&r->process;K386Session *s=&sessions[id];unsigned a=0x2f00,b; k16 seg;
    k32 at=(p->dta>>16)*16+(p->dta&65535);if(!get(id,at,host_dta,43))return -13;
    if(next){if(!p->search_valid)return -18;for(int i=0;i<21;i++)if(host_dta[i]!=p->search[i])return -18;}
    __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"cc","memory");
    unsigned set=0x1a00,d=(unsigned)host_dta;__asm__ volatile("int $0x21":"+a"(set):"d"(d):"cc","memory");
    a=next?0x4f00:0x4e00;unsigned c=s->frame.ecx&0x37;d=(unsigned)path;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:":"+a"(a):"c"(c),"d"(d):"cc","memory");int result=(int)a;
    p->search_valid=!result;if(!result)for(int i=0;i<21;i++)p->search[i]=host_dta[i];put(id,at,host_dta,43);
    set=0x1a00;d=b&65535;__asm__ volatile("pushw %%ds; movw %w2,%%ds; int $0x21; popw %%ds":"+a"(set):"d"(d),"r"(seg):"cc","memory");return result;
}
void k386_broker(unsigned id){
    K386Session *s=&sessions[id];K386Frame *f=&s->frame;K386Runtime *r=&runtime[id];K386Process *p=&r->process;
    int result=0;unsigned handle=f->ebx&65535;char path[128];f->eflags&=~1u;
    if(s->request==0x4b){
        char name[128];k8 tail[128];for(int i=0;i<128;i++){name[i]=transfer[i];tail[i]=transfer[128+i];}k16 env=word(transfer+256);
        int h,error;K386Image plan;if(r->depth==K386_DEPTH-1)result=-8;
        else if((error=prepare(id,name,&h,&plan,path)))result=-error;
        else{
            k16 handles[K386_HANDLES];error=inherit(id,handles);
            if(!error){
                p->frame=*f;for(int i=0;i<K386_HANDLES;i++)p->handles[i]=s->handles[i];r->parent[r->depth]=*p;
                if(!env)env=getword(id,(k32)p->psp*16+0x2c);
                backing_copy(&r->backing,0xa4000+r->depth*1024,0,1024);
                error=load_program(id,h,&plan,tail,path,env,0);
                if(!error){r->depth++;for(int i=0;i<K386_HANDLES;i++)s->handles[i]=handles[i];jft(id);}
                else for(int i=0;i<K386_HANDLES;i++)close_token(handles[i]);
            }
            p_close(h);result=-error;
        }
        if(!result){s->request=0;return;}
    }else if(s->request==0x29){
        char input[130]={0};k8 fcb[37];for(int i=0;i<128;i++)input[i]=transfer[i];
        k32 at=(f->es&65535)*16+(f->edi&65535);
        if(!get(id,at,fcb,sizeof fcb))result=-13;
        else{
            unsigned a=f->eax&65535,si=(unsigned)input,di=(unsigned)fcb;
            __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x21; popw %%es":"+a"(a),"+S"(si),"+D"(di)::"ebx","ecx","edx","cc","memory");
            if(!put(id,at,fcb,sizeof fcb))result=-13;
            else{f->eax=(f->eax&0xff00)|(a&255);f->esi=(f->esi+si-(unsigned)input)&65535;s->request=0;return;}
        }
    }else if(s->request==0x1c){
        k8 date[6];unsigned a=0;unsigned b=(unsigned)date;
        __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x1c; popw %%es":"+a"(a),"+b"(b)::"ecx","edx","esi","edi","cc","memory");
        result=put(id,(f->es&65535)*16+(f->ebx&65535),date,6)?0:-13;
    }else if(s->request==0x48||s->request==0x49||s->request==0x4a){
        k16 available=0;
        if(s->request==0x48){result=arena_alloc(&r->arena,f->ebx&65535,p->psp,0);if(!result){f->ebx=arena_largest(&r->arena);result=-8;}}
        else if(s->request==0x49)result=-arena_free(&r->arena,f->es&65535,p->psp);
        else{result=-arena_resize(&r->arena,f->es&65535,f->ebx&65535,p->psp,&available);if(result)f->ebx=available;else if((f->es&65535)==p->psp)putword(id,(k32)p->psp*16+2,p->psp+(f->ebx&65535));}
        arenas(id);
    }else if(s->request==0x3d){
        int at;for(at=5;at<K386_HANDLES&&s->handles[at];at++);
        if(at==K386_HANDLES)result=-4;
        else if(!fd_cmp((char *)transfer,"CON")||!fd_cmp((char *)transfer,"CON:")){s->handles[at]=0xff02;result=at;}
        else if(!fd_cmp((char *)transfer,"NUL")){s->handles[at]=0xff06;result=at;}
        else if(f->eax&3)result=-5;
        else if((result=disk_path(id,(char *)transfer,path,0)))result=-result;
        else{result=p_open(path);if(result>=0){s->handles[at]=result+1;result=at;}}
        jft(id);
    }else if(s->request==0x4e||s->request==0x4f){if(s->request==0x4e&&(result=disk_path(id,(char *)transfer,path,1)))result=-result;else result=directory(id,s->request==0x4f,path);
    }else if(s->request==0x3b||s->request==0x43){if((result=disk_path(id,(char *)transfer,path,0)))result=-result;else{k16 attrs;result=attributes(path,&attrs);if(!result){if(s->request==0x3b){if(!(attrs&0x10)||fd_len(path)>66)result=-3;else fd_text(r->cwd,path,sizeof r->cwd);}else f->ecx=attrs;}}}
    else if(s->request==0x2a||s->request==0x2c){unsigned a=(unsigned)s->request<<8,c,d;__asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"ebx","cc","memory");f->eax=a&65535;f->ecx=c&65535;f->edx=d&65535;s->request=0;return;}
    else if(handle>=K386_HANDLES||!s->handles[handle])result=-6;
    else if(s->request==0x3e){close_token(s->handles[handle]);s->handles[handle]=0;jft(id);}
    else if(s->request==0x45||s->request==0x46){
        unsigned dest=f->ecx&65535;if(s->request==0x45){for(dest=0;dest<K386_HANDLES&&s->handles[dest];dest++);}
        if(dest>=K386_HANDLES)result=-4;else{result=duplicate(s->handles[handle]);if(result>=0){close_token(s->handles[dest]);s->handles[dest]=result;result=dest;jft(id);}}
    }else if(s->request==0x3f){
        if(s->handles[handle]==0xff06)result=0;
        else if(s->handles[handle]>=0xff00)result=-6;
        else{unsigned wanted=f->ecx&65535;k32 at=(f->ds&65535)*16+(f->edx&65535);while((unsigned)result<wanted){unsigned chunk=wanted-result;if(chunk>sizeof transfer)chunk=sizeof transfer;int n=p_read(s->handles[handle]-1,transfer,chunk);if(n<0){if(!result)result=n;break;}if(n&&!put(id,at+result,transfer,n)){result=-13;break;}result+=n;if(n<(int)chunk)break;}}
    }else if(s->request==0x42){
        if(s->handles[handle]>=0xff00){f->eax=f->edx=0;s->request=0;return;}
        unsigned a=0x4200|(f->eax&255),b=s->handles[handle]-1,c=f->ecx&65535,d=f->edx&65535;
        __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:":"+a"(a),"+d"(d):"b"(b),"c"(c):"cc","memory");if((int)a<0)result=(int)a;else{f->eax=a&65535;f->edx=d&65535;s->request=0;return;}
    }
    if(result<0){f->eax=-result;f->eflags|=1;}else f->eax=result;s->request=0;
}
