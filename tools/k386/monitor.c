#define K386_MONITOR
#include "abi.h"
#include "../window/hardware.h"
K386Mailbox k386_mailbox={{'K','3','8','6','A','B','I','1'},1,sizeof(K386Mailbox),1,0,0,0,0,0,{0},{{0}}};
K386Session sessions[2];
k32 monitor_base,page_directory[2],selected,saved_cr0,saved_cr3;
k8 transfer[4096];
static k8 pages[20480];
static k16 ds_segment,indos_segment,indos_offset;
static void physical32(k32 at,k32 value) {for(int i=0;i<4;i++)mem_put(at>>4,(at&15)+i,value>>(i*8));}
static void close_files(K386Session *s) {for(int i=0;i<8;i++)if(s->handles[i]){p_close(s->handles[i]-1);s->handles[i]=0;}}
/* A read-only host broker must never block on a real CON/serial/printer device. */
static int safe_path(const k8 *path) {
    int start=0;
    if(path[0]&&path[1]==':'){if(path[0]!='A'&&path[0]!='a')return 0;start=2;}
    char base[9];int length=0,extension=0;
    for(int i=start;i<128;i++) {
        unsigned c=path[i];if(c>='a'&&c<='z')c-=32;
        if(!c||c=='\\') {
            base[length]=0;
            int device=(length==3&&((base[0]=='C'&&base[1]=='O'&&base[2]=='N')||(base[0]=='P'&&base[1]=='R'&&base[2]=='N')||(base[0]=='A'&&base[1]=='U'&&base[2]=='X')||(base[0]=='N'&&base[1]=='U'&&base[2]=='L')))
                ||(length==4&&((base[0]=='C'&&base[1]=='O'&&base[2]=='M')||(base[0]=='L'&&base[1]=='P'&&base[2]=='T'))&&base[3]>='0'&&base[3]<='9');
            if(device)return 0;
            if(!c)return i>start;
            length=extension=0;continue;
        }
        if(c=='.'){extension=1;continue;}
        if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='~'))return 0;
        if(!extension){if(length==8)return 0;base[length++]=c;}
    }
    return 0;
}
static int create(unsigned id) {
    K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    if(!k386_mailbox.path[0])return 2;
    /* Reject EXE and truncated/unbounded names; the loader is deliberately COM-only. */
    int len=0;while(len<128&&k386_mailbox.path[len])len++;
    if(len>=128||!safe_path((k8 *)k386_mailbox.path))return 3;
    int h=p_open(k386_mailbox.path);if(h<0)return -h;
    v->state=K386_ENDED;
    if(!s->allocation)s->allocation=mem_alloc(0x16000);
    if(!s->allocation){p_close(h);return 8;}
    close_files(s);s->physical=((k32)s->allocation*16+4095)&~4095u;
    mem_fill(s->physical>>4,0,0x15000,0);
    k32 used=0;int n;
    while((n=p_read(h,transfer,sizeof transfer))>0) {
        if(!used&&n>=2&&transfer[0]=='M'&&transfer[1]=='Z'){p_close(h);return 11;}
        if(used+n>0xf000){p_close(h);return 8;}
        mem_copy(s->physical>>4,0x100+used,ds_segment,(unsigned)transfer,n);used+=n;
    }
    p_close(h);if(n<0||!used)return n<0?-n:11;
    s->frame=(K386Frame){0};s->frame.cs=s->frame.ds=s->frame.es=s->frame.ss=K386_SEGMENT;
    s->frame.eip=0x100;s->frame.esp=0xfffc;s->frame.eflags=0x20202;
    s->request=s->head=s->count=s->line_count=0;
    physical32(s->physical,0x900020cd); /* INT 20h PSP and end segment. */
    mem_put(s->physical>>4,0x80,0);mem_put(s->physical>>4,0x81,13);
    for(k32 i=0;i<2000;i++){mem_put(s->physical>>4,0x10000+i*2,' ');mem_put(s->physical>>4,0x12000+i*2,0xe1);}
    *v=(K386View){0};for(int i=0;i<2000;i++)v->screen[i]=' ';v->state=K386_RUNNING;v->physical=s->physical;
    /* Two page directories with private conventional RAM, zero page and TVRAM. */
    k32 pd=page_directory[id];s->pd=pd;v->cr3=pd;
    mem_fill(pd>>4,0,8192,0);physical32(pd,pd+4096+7);
    for(k32 i=0;i<1024;i++)physical32(pd+4096+i*4,i*4096+3);
    physical32(pd+4096,s->physical+0x14000+7);
    for(k32 i=0;i<16;i++)physical32(pd+4096+(0x80+i)*4,s->physical+i*4096+7);
    for(k32 i=0;i<4;i++)physical32(pd+4096+(0xa0+i)*4,s->physical+0x10000+i*4096+7);
    return 0;
}
static void broker(unsigned id) {
    K386Session *s=&sessions[id];K386Frame *f=&s->frame;int result=0;unsigned handle=f->ebx&0xffff;
    f->eflags&=~1;
    if(s->request==0x3d) {
        int at;for(at=3;at<8&&s->handles[at];at++);
        if(at==8)result=-4;
        else if(!safe_path(transfer))result=-5;
        else {result=p_open((char *)transfer);if(result>=0){s->handles[at]=result+1;result=at;}}
    } else if(s->request==0x2a||s->request==0x2c) {
        unsigned a=(unsigned)s->request<<8,c,d;
        __asm__ volatile("int $0x21":"+a"(a),"=c"(c),"=d"(d)::"ebx","cc","memory");
        f->eax=a&0xffff;f->ecx=c&0xffff;f->edx=d&0xffff;s->request=0;return;
    } else if(handle<3||handle>=8||!s->handles[handle])result=-6;
    else if(s->request==0x3e){result=p_close(s->handles[handle]-1);if(result>=0)s->handles[handle]=0;}
    else if(s->request==0x3f) {
        unsigned n=f->ecx&0xffff;if(n>sizeof transfer)n=sizeof transfer;
        result=p_read(s->handles[handle]-1,transfer,n);
        if(result>0)mem_copy(s->physical>>4,((f->ds&0xffff)*16+(f->edx&0xffff))-0x80000,ds_segment,(unsigned)transfer,result);
    } else if(s->request==0x42) {
        unsigned a=0x4200|(f->eax&255),b=s->handles[handle]-1,c=f->ecx&0xffff,d=f->edx&0xffff;
        __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:":"+a"(a),"+d"(d):"b"(b),"c"(c):"cc","memory");
        if((int)a<0)result=(int)a;else{f->eax=a&0xffff;f->edx=d&0xffff;s->request=0;return;}
    }
    if(result<0){f->eax=-result;f->eflags|=1;}else f->eax=result;
    s->request=0;
}
void api_dispatch(void) {
    unsigned id=k386_mailbox.session;k386_mailbox.error=0;
    if(mem_get(indos_segment,indos_offset)){k386_mailbox.error=5;return;}
    k16 msw;unsigned char mask;
    __asm__ volatile("smsw %0; inb $2,%1":"=r"(msw),"=a"(mask));
    if((msw&1)||!(mask&1)){k386_mailbox.error=5;return;}

    if(id>=2){k386_mailbox.error=87;return;}
    unsigned caller,service=0x6200;
    __asm__ volatile("int $0x21":"+a"(service),"=b"(caller)::"cc","memory");
    service=0x5000;__asm__ volatile("int $0x21":"+a"(service):"b"((unsigned)ds_segment):"cc","memory");
    selected=id;K386Session *s=&sessions[id];K386View *v=&k386_mailbox.view[id];
    switch(k386_mailbox.command) {
    case K386_CREATE:k386_mailbox.error=create(id);break;
    case K386_STEP:
        if(v->state!=K386_RUNNING)break;
        pm_run();if(s->request)broker(id);if(v->state==K386_ENDED||v->state==K386_FAULTED)close_files(s);
        break;
    case K386_KEY:
        if(s->count==32){k386_mailbox.error=8;break;}
        s->keys[(s->head+s->count++)%32]=k386_mailbox.key;
        if(v->state==K386_WAITING)v->state=K386_RUNNING;
        break;
    case K386_STOP:if(v->state==K386_RUNNING||v->state==K386_WAITING)v->state=K386_STOPPED;break;
    case K386_RESUME:if(v->state==K386_STOPPED)v->state=K386_RUNNING;break;
    case K386_END:close_files(s);if(s->allocation)mem_free(s->allocation);*s=(K386Session){0};v->state=K386_ENDED;break;
    default:k386_mailbox.error=1;
    }
    service=0x5000;__asm__ volatile("int $0x21":"+a"(service):"b"(caller):"cc","memory");
}
int monitor_init(void) {
    __asm__ volatile("movw %%ds,%0":"=r"(ds_segment));monitor_base=(k32)ds_segment*16;
    if(monitor_base+0x10000>=0x80000)return 0;
    unsigned a=0x3400,b; k16 seg;
    __asm__ volatile("pushw %%es; int $0x21; movw %%es,%1; popw %%es":"+a"(a),"=c"(seg),"=b"(b)::"cc","memory");
    indos_segment=seg;indos_offset=b;
    k32 at=(monitor_base+(unsigned)pages+4095)&~4095u;
    page_directory[0]=at;page_directory[1]=at+8192;
    return 1;
}
_Static_assert(sizeof(K386Frame)==76,"interrupt frame ABI");
_Static_assert(sizeof(K386Session)==144,"assembly session stride");
