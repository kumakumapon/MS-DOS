/* Ring-0 monitor. All ordinary DOS/BIOS and file I/O stays in real mode. */
#define K386_MONITOR
#include "runtime.h"
static k8 read8(k32 address) {
    k8 b;__asm__ volatile("movb %%fs:(%1),%0":"=q"(b):"r"(address):"memory");return b;
}
static void write8(k32 address,k8 b) {
    __asm__ volatile("movb %b0,%%fs:(%1)"::"q"(b),"r"(address):"memory");
}
static k32 address(k32 seg,k32 off,k32 size) {
    k32 at=(seg&0xffff)*16+(off&0xffff);
    return size<=0x10000-(off&0xffff)&&at<K386_RAM&&size<=K386_RAM-at?at:0;
}
static k32 readn(k32 at,int n){k32 v=0;for(int i=0;i<n;i++)v|=(k32)read8(at+i)<<(i*8);return v;}
static void writen(k32 at,k32 v,int n){for(int i=0;i<n;i++)write8(at+i,v>>(i*8));}
static int name(k32 seg,k32 off){for(k32 i=0;i<128;i++){if(i>0xffff-(off&0xffff))return 0;k32 at=address(seg,off+i,1);if(!at)return 0;transfer[i]=read8(at);if(!transfer[i])return 1;}return 0;}
static k32 dta_address(K386Process *r){return address(r->dta>>16,r->dta&65535,43);}
static k32 flags(K386Frame *f){return (f->eflags&0xcd5)|2|(runtime[selected].process.vif?0x200:0);}
static void set_flags(K386Frame *f,k32 value){runtime[selected].process.vif=(value&0x200)!=0;f->eflags=(value&0xcd5)|0x20202;}
int kernel_interrupt(K386Frame *f,k16 cs,k16 ip) {
    k32 sp=((f->esp&65535)-6)&65535,at=address(f->ss,sp,6);
    if(!at||!address(cs,ip,1))return 0;
    writen(at,f->eip,2);writen(at+2,f->cs,2);writen(at+4,flags(f),2);
    f->esp=(f->esp&0xffff0000)|sp;f->cs=cs;f->eip=ip;
    runtime[selected].process.vif=0;return 1;
}
int kernel_deliver(void) {
    K386Runtime *r=&runtime[selected];K386View *v=&k386_mailbox.view[selected];
    if(!r->timer_pending||!r->process.vif)return 0;
    r->timer_pending=0;
    if(kernel_interrupt(&sessions[selected].frame,readn(0x1e,2),readn(0x1c,2)))return 0;
    v->state=K386_FAULTED;v->fault=13;return 1;
}
static void error(K386Frame *f,k16 code) {f->eax=code;f->eflags|=1;}
static void output(k8 c) {
    K386View *v=&k386_mailbox.view[selected];
    if(c=='\r')v->column=0;
    else if(c=='\n')v->row++;
    else if(c==8){if(v->column)v->column--;else if(v->row){v->row--;v->column=79;}}
    else if(c==9){int n=8-(v->column&7);while(n--)output(' ');}
    else if(c>=32){write8(0xa0000+(v->row*80+v->column)*2,c);write8(0xa0001+(v->row*80+v->column)*2,0);if(++v->column==80){v->column=0;v->row++;}}
    if(v->row>=25){for(k32 i=0;i<1920;i++)write8(0xa0000+i*2,read8(0xa0000+(i+80)*2));for(k32 i=1920;i<2000;i++)write8(0xa0000+i*2,' ');v->row=24;}
}
void kernel_map(void) {
    k32 pd=page_directory[selected],physical=runtime[selected].backing.physical;
    for(k32 i=0;i<0xa4;i++)writen(0x400000+pd+4096+i*4,physical+i*4096+7,4);
    /* COMMAND probes the model byte at FFFF:000Eh. Expose the actual PC-98
       firmware read-only; never map host DOS RAM or writable device apertures. */
    for(k32 i=0xf0;i<0x100;i++)writen(0x400000+pd+4096+i*4,i*4096+5,4);
    __asm__ volatile("movl %0,%%cr3"::"r"(pd):"memory");
}
void kernel_unmap(void) {
    k32 pd=page_directory[selected],first=monitor_base>>12,last=(monitor_base+0xffff)>>12;
    for(k32 i=first;i<=last;i++)writen(0x400000+pd+4096+i*4,i*4096+3,4);
    __asm__ volatile("movl %0,%%cr3"::"r"(pd):"memory");
}
static int input(void) {
    K386Session *s=&sessions[selected];if(!s->count)return -1;
    k8 c=s->keys[s->head];s->head=(s->head+1)%K386_KEY_CAPACITY;s->count--;return c;
}
static int service(K386Frame *f,k8 number) {
    K386Session *s=&sessions[selected];K386View *v=&k386_mailbox.view[selected];K386Process *r=&runtime[selected].process;
    k32 ptr,n;int c;unsigned ah=(f->eax>>8)&255;
    k386_diagnostics.calls[selected][k386_diagnostics.count[selected]++&15]=(K386Call){number,f->eax,f->ebx,f->ecx,f->edx,f->ds,f->es,f->cs,f->eip};
    if(number==0x20||(number==0x21&&ah==0x4c)){v->state=K386_ENDED;v->exit_code=number==0x20?0:f->eax&255;return 1;}
    if(number==0x29){output(f->eax);return 0;}
    if(number==0x1c) {
        K386Runtime *clock=&runtime[selected];
        if(ah==0){if(!address(f->es,f->ebx,6)){error(f,13);return 0;}s->request=0x1c;return 1;}
        if(ah==2){
            ptr=address(f->es,f->ebx,1);if(!ptr){error(f,13);return 0;}
            writen(0x1c,f->ebx,2);writen(0x1e,f->es,2);writen(0x58a,f->ecx,2);
        }else if(ah!=3){error(f,5);return 0;}
        clock->timer_count=readn(0x58a,2);if(!clock->timer_count)clock->timer_count=65536;
        clock->timer_armed=1;clock->timer_pending=0;clock->timer_owner=r->psp;
        clock->clock=clock->accumulator=0;return 0;
    }
    if(number==0x2f) {
        if((f->eax&65535)==0x122e&&(f->edx&255)<6){
            ptr=0x8400+(f->edx&254)*2;
            if(f->edx&1){writen(ptr,f->edi,2);writen(ptr+2,f->es,2);}
            else{f->edi=readn(ptr,2);f->es=readn(ptr+2,2);}
        }else f->eax&=0xff00; /* Uninstalled multiplex services return AL=0. */
        f->eflags&=~1u;return 0;
    }
    if(number==0x18) {
        if(ah==1){
            f->ebx=(f->ebx&~0xff00u)|(s->count?0xff00:0);
            if(s->count){f->eax=s->keys[s->head];f->eflags&=~0x40u;}else f->eflags|=0x40;
            return 0;
        }
        if(ah==0){c=input();if(c<0){v->state=K386_WAITING;return 2;}f->eax=c;f->eflags&=~0x40u;return 0;}
        if(ah==0x0c){for(k32 i=0;i<2000;i++)write8(0xa0000+i*2,' ');v->row=v->column=0;return 0;}
        error(f,1);return 0;
    }
    if(number!=0x21){error(f,1);return 0;}
    f->eflags&=~1;
    switch(ah) {
    case 0: v->state=K386_ENDED;return 1;
    case 1:case 7:case 8:
        c=input();if(c<0){v->state=K386_WAITING;return 2;}
        f->eax=(f->eax&0xff00)|c;if(ah==1)output(c);break;
    case 2:output(f->edx);f->eax=(f->eax&0xff00)|(f->edx&255);break;
    case 6:
        if((f->edx&255)!=255){output(f->edx);break;}
        c=input();if(c<0){f->eax&=0xff00;f->eflags|=0x40;}else{f->eax=(f->eax&0xff00)|c;f->eflags&=~0x40;}break;
    case 9:
        ptr=address(f->ds,f->edx,1);if(!ptr){error(f,13);break;}
        for(n=0;n<4096&&ptr+n<K386_RAM;n++){c=read8(ptr+n);if(c=='$')break;output(c);}if(n==4096)error(f,13);break;
    case 0x0a:
        ptr=address(f->ds,f->edx,2);if(!ptr){error(f,13);break;}
        n=read8(ptr);if(!address(f->ds,f->edx,n+2)){error(f,13);break;}
        if(!n){write8(ptr+1,0);s->line_count=0;break;}
        while((c=input())>=0){if(c==13){write8(ptr+1,s->line_count);write8(ptr+2+s->line_count,13);s->line_count=0;output('\r');output('\n');return 0;}if(c==8){if(s->line_count){s->line_count--;output(8);output(' ');output(8);}}else if(c>=32&&(k32)s->line_count+1<n){write8(ptr+2+s->line_count++,c);output(c);}}
        v->state=K386_WAITING;return 2;
    case 0x0b:f->eax=(f->eax&0xff00)|(s->count?255:0);break;
    case 0x0e:if((f->edx&255)!=0)error(f,15);else f->eax=1;break;
    case 0x19:f->eax&=0xff00;break;
    case 0x1a:
        if(!address(f->ds,f->edx,43))error(f,13);else r->dta=(f->ds<<16)|(f->edx&65535);break;
    case 0x2f:f->es=r->dta>>16;f->ebx=r->dta&65535;break;
    case 0x29:
        if(!address(f->es,f->edi,37)){error(f,13);break;}
        for(n=0;n<127&&n<=65535-(f->esi&65535);n++){
            ptr=address(f->ds,f->esi+n,1);if(!ptr)break;
            transfer[n]=read8(ptr);if(!transfer[n]||transfer[n]==13)break;
        }
        transfer[n]=0;s->request=0x29;return 1;
    case 0x25:
        c=f->eax&255;
        if(c==0x18||c==0x1c||c==0x20||c==0x21||c==0x29||c==0x2f||!address(f->ds,f->edx,1))error(f,5);
        else {writen(c*4,f->edx,2);writen(c*4+2,f->ds,2);}break;
    case 0x35:ptr=(f->eax&255)*4;f->ebx=readn(ptr,2);f->es=readn(ptr+2,2);break;
    case 0x30:f->eax=4;f->ebx=0;f->ecx=0;break;
    case 0x37:if((f->eax&255)==0){f->eax&=0xff00;f->edx=(f->edx&0xff00)|'/';}else error(f,1);break;
    case 0x38:
        ptr=address(f->ds,f->edx,34);if(!ptr||(f->edx&65535)==65535){error(f,5);break;}
        for(n=0;n<34;n++)write8(ptr+n,0);
        write8(ptr,2);write8(ptr+2,0x5c);write8(ptr+7,',');write8(ptr+9,'.');
        write8(ptr+11,'/');write8(ptr+13,':');write8(ptr+17,2);write8(ptr+18,1);write8(ptr+24,',');
        f->eax=f->ebx=81;break;
    case 0x63:f->ds=0x820;f->esi=0;f->eax&=0xff00;break;
    case 0x65:
        c=f->eax&255;
        if(c==2||c==4||c==7){
            ptr=address(f->es,f->edi,5);if(!ptr||(f->ecx&65535)<5){error(f,13);break;}
            write8(ptr,c);writen(ptr+1,0,2);writen(ptr+3,c==7?0x824:0x800,2);f->ecx=5;
        }else if(c==0x20){c=f->edx&255;if(c>='a'&&c<='z')f->edx-=32;}
        else if(c==0x21||c==0x22){
            n=c==0x21?65536-(f->edx&65535):f->ecx&65535;ptr=address(f->ds,f->edx,n);if(!ptr){error(f,13);break;}
            for(k32 i=0;i<n;i++){c=read8(ptr+i);if(!c&&(f->eax&255)==0x21)break;if(c>='a'&&c<='z')write8(ptr+i,c-32);}
        }else error(f,1);break;
    case 0x44:
        n=f->ebx&65535;c=f->eax&255;
        if(n>=K386_HANDLES||!s->handles[n]){error(f,6);break;}
        if(c==0)f->edx=s->handles[n]>=0xff00?(s->handles[n]==0xff06?0x80c4:0x80d3):0;
        else if(c==1&&s->handles[n]>=0xff00){} /* Private CON/NUL attributes. */
        else if(c==6||c==7)f->eax=(f->eax&0xff00)|((c==7||s->count)?255:0);
        else error(f,1);
        break;
    case 0x47:
        if((f->edx&255)>1){error(f,15);break;}
        n=0;while(runtime[selected].cwd[0]&&runtime[selected].cwd[n+3]&&n<124)n++;
        ptr=address(f->ds,f->esi,n+1);if(!ptr){error(f,13);break;}
        for(k32 i=0;i<=n;i++)write8(ptr+i,runtime[selected].cwd[0]?runtime[selected].cwd[i+3]:0);
        break;
    case 0x4d:f->eax=r->last_exit;r->last_exit=0;break;
    case 0x50:if((f->ebx&65535)!=r->psp)error(f,5);break;
    case 0x51:case 0x62:f->ebx=r->psp;break;
    case 0x40:
        c=f->ebx&65535;
        if(c>=K386_HANDLES||!s->handles[c]){error(f,6);break;}
        if(s->handles[c]<0xff00){error(f,5);break;}
        n=f->ecx&0xffff;
        ptr=address(f->ds,f->edx,n);if(!ptr){error(f,13);break;}
        if(s->handles[c]!=0xff06)for(k32 i=0;i<n;i++)output(read8(ptr+i));
        f->eax=n;break;
    case 0x3f:
        c=f->ebx&65535;
        if(c>=K386_HANDLES||!s->handles[c]){error(f,6);break;}
        if(s->handles[c]>=0xff00){
            n=f->ecx&65535;ptr=address(f->ds,f->edx,n);if(!ptr){error(f,13);break;}
            if(s->handles[c]==0xff06||!n){f->eax=0;break;}
            while((c=input())>=0){
                if(c==8&&s->line_count){s->line_count--;output(8);output(' ');output(8);continue;}
                if(c<32&&c!=13)continue;
                write8(ptr+s->line_count++,c);
                if(c==13){output('\r');output('\n');if(s->line_count<n)write8(ptr+s->line_count++,10);}
                else output(c);
                if(c==13||s->line_count>=n){f->eax=s->line_count;s->line_count=0;return 0;}
            }
            v->state=K386_WAITING;return 2;
        }
        s->request=ah;if(!address(f->ds,f->edx,f->ecx&65535)){error(f,13);s->request=0;break;}return 1;
    case 0x39:case 0x3a:case 0x3c:case 0x41:case 0x56:case 0x5b:error(f,5);break;

    case 0x3d:case 0x3e:case 0x42:case 0x2a:case 0x2c:case 0x3b:case 0x43:case 0x4e:case 0x4f:case 0x4b:case 0x48:case 0x49:case 0x4a:case 0x45:case 0x46:
        s->request=ah;
        if(ah==0x43&&(f->eax&255)){error(f,5);s->request=0;break;}
        if(ah==0x3d||ah==0x3b||ah==0x43||ah==0x4e||ah==0x4b){if(!name(f->ds,f->edx)){error(f,3);s->request=0;break;}}
        if((ah==0x4e||ah==0x4f)&&!dta_address(r)){error(f,13);s->request=0;break;}
        if(ah==0x4b){
            ptr=address(f->es,f->ebx,14);
            if((f->eax&255)||!ptr){error(f,10);s->request=0;break;}
            transfer[256]=readn(ptr,2);transfer[257]=readn(ptr,2)>>8;
            k32 tail=address(readn(ptr+4,2),readn(ptr+2,2),1);n=tail?read8(tail):127;
            if(n>126||!address(readn(ptr+4,2),readn(ptr+2,2),n+2)){error(f,13);s->request=0;break;}
            for(k32 i=0;i<n+2;i++)transfer[128+i]=read8(tail+i);
            transfer[128+n+1]=13;
        }
        if(ah==0x3f&&!address(f->ds,f->edx,f->ecx&0xffff)){error(f,13);s->request=0;break;}
        return 1;
    default:error(f,1);break;
    }
    return 0;
}
static int stack_flags(K386Frame *f,k8 op,int wide,k32 length) {
    int size=wide?4:2;k32 sp=f->esp&65535,at;
    if(op==0x9c){sp=(sp-size)&65535;at=address(f->ss,sp,size);if(!at)return 0;writen(at,flags(f),size);}
    else {
        at=address(f->ss,sp,op==0xcf?size*3:size);if(!at)return 0;
        if(op==0xcf){
            k32 ip=readn(at,size),cs=readn(at+size,size);
            if(ip>65535||cs>65535||!address(cs,ip,1))return 0;
            set_flags(f,readn(at+size*2,size));f->eip=ip;f->cs=cs;sp=(sp+size*3)&65535;
        }else{set_flags(f,readn(at,size));sp=(sp+size)&65535;}
    }
    f->esp=(f->esp&0xffff0000)|sp;if(op!=0xcf)f->eip+=length;return 1;
}
static int user_interrupt(K386Frame *f,k8 number,k32 length,int wide) {
    k32 ip=readn(number*4,2),cs=readn(number*4+2,2);
    if(!cs&&!ip)return 0;
    if(!address(cs,ip,1))return -1;
    int size=wide?4:2;k32 sp=((f->esp&65535)-size*3)&65535,at=address(f->ss,sp,size*3);
    if(!at)return -1;
    writen(at,f->eip+length,size);writen(at+size,f->cs,size);writen(at+size*2,flags(f),size);
    f->esp=(f->esp&0xffff0000)|sp;f->cs=cs;f->eip=ip;runtime[selected].process.vif=0;return 1;
}
int kernel_trap(K386Frame *f) {
    K386Session *s=&sessions[selected];K386View *v=&k386_mailbox.view[selected];
    if(!(f->eflags&0x20000)){v->state=K386_FAULTED;v->fault=0x8000|f->vector;v->eip=f->eip;v->eflags=f->eflags;v->exit_code=f->error;return 1;}
    v->eflags=f->eflags;v->eip=f->eip;__asm__ volatile("movl %%cr3,%0":"=r"(v->cr3));
    if(f->vector==32){v->ticks++;s->frame=*f;return 1;}
    if(f->vector==13) {
        k32 pc=address(f->cs,f->eip,1),length=1;int wide=0;k8 op=pc?read8(pc):0;
        if(op==0x66&&address(f->cs,f->eip,2)){wide=1;length++;op=read8(++pc);}
        if(op==0xcd&&address(f->cs,f->eip,length+1)){
            k8 number=read8(pc+1);length++;
            if(number!=0x18&&number!=0x1c&&number!=0x20&&number!=0x21&&number!=0x29&&number!=0x2f){int u=user_interrupt(f,number,length,wide);if(u>0){s->frame=*f;return 0;}if(u<0)goto fault;}
            int r=service(f,number);if(r!=2)f->eip+=length;s->frame=*f;return r!=0;
        }
        if(op==0x9c||op==0x9d||op==0xcf){if(!stack_flags(f,op,wide,length))goto fault;s->frame=*f;return 0;}
        if(op==0xfa||op==0xfb){runtime[selected].process.vif=op==0xfb;f->eip+=length;s->frame=*f;return 0;}
        if(op==0xf4){f->eip+=length;s->frame=*f;return 1;}
    }
fault:
    v->state=K386_FAULTED;v->fault=f->vector;v->eip=f->eip;
    if(f->vector==14)__asm__ volatile("movl %%cr2,%0":"=r"(v->cr2));
    s->frame=*f;return 1;
}
void kernel_finish(void) {
    K386View *v=&k386_mailbox.view[selected];
    int changed=0;
    for(k32 i=0;i<2000;i++){k8 c=read8(0xa0000+i*2);if((k8)v->screen[i]!=c)changed=1;v->screen[i]=c;}
    if(changed)v->flags++;
    v->counter=readn((k32)runtime[selected].process.psp*16+0x800,4);
    v->keys=sessions[selected].count;
    k386_diagnostics.frame[selected]=sessions[selected].frame;
    k32 base=(k32)runtime[selected].process.psp*16;
    for(k32 i=0;i<512;i++)k386_diagnostics.memory[selected][i]=i<K386_RAM-base?read8(base+i):0;
}
