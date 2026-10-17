/* Ring-0 monitor. All ordinary DOS/BIOS and file I/O stays in real mode. */
#define K386_MONITOR
#include "abi.h"
static k8 read8(k32 address) {
    k8 b;__asm__ volatile("movb %%fs:(%1),%0":"=q"(b):"r"(address):"memory");return b;
}
static void write8(k32 address,k8 b) {
    __asm__ volatile("movb %b0,%%fs:(%1)"::"q"(b),"r"(address):"memory");
}
static k32 address(k32 seg,k32 off,k32 size) {
    k32 at=(seg&0xffff)*16+(off&0xffff);
    return at>=0x80000&&at<=0x90000&&size<=0x90000-at?at:0;
}
static void error(K386Frame *f,k16 code) {f->eax=code;f->eflags|=1;}
static void output(k8 c) {
    K386View *v=&k386_mailbox.view[selected];
    if(c=='\r')v->column=0;
    else if(c=='\n')v->row++;
    else if(c==8){if(v->column)v->column--;}
    else if(c>=32){write8(0xa0000+(v->row*80+v->column)*2,c);write8(0xa0001+(v->row*80+v->column)*2,0);if(++v->column==80){v->column=0;v->row++;}}
    if(v->row>=25){for(k32 i=0;i<1920;i++)write8(0xa0000+i*2,read8(0xa0000+(i+80)*2));for(k32 i=1920;i<2000;i++)write8(0xa0000+i*2,' ');v->row=24;}
}
static int input(void) {
    K386Session *s=&sessions[selected];if(!s->count)return -1;
    k8 c=s->keys[s->head];s->head=(s->head+1)%32;s->count--;return c;
}
static int service(K386Frame *f,k8 number) {
    K386Session *s=&sessions[selected];K386View *v=&k386_mailbox.view[selected];
    k32 ptr,n;int c;unsigned ah=(f->eax>>8)&255;
    if(number==0x20||(number==0x21&&ah==0x4c)){v->state=K386_ENDED;v->exit_code=f->eax&255;return 1;}
    if(number==0x29){output(f->eax);return 0;}
    if(number==0x18) {
        if(ah==0||ah==1){c=input();if(c<0){if(ah==1){f->eflags|=0x40;return 0;}v->state=K386_WAITING;return 2;}f->eax=c;f->eflags&=~0x40;return 0;}
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
        for(n=0;n<4096&&ptr+n<0x90000;n++){c=read8(ptr+n);if(c=='$')break;output(c);}if(n==4096)error(f,13);break;
    case 0x0a:
        ptr=address(f->ds,f->edx,2);if(!ptr){error(f,13);break;}
        n=read8(ptr);if(!address(f->ds,f->edx,n+2)){error(f,13);break;}
        while((c=input())>=0){if(c==13){write8(ptr+1,s->line_count);write8(ptr+2+s->line_count,13);s->line_count=0;output('\r');output('\n');return 0;}if(c==8){if(s->line_count){s->line_count--;output(8);output(' ');output(8);}}else if(c>=32&&(k32)s->line_count+1<n){write8(ptr+2+s->line_count++,c);output(c);}}
        v->state=K386_WAITING;return 2;
    case 0x0b:f->eax=(f->eax&0xff00)|(s->count?255:0);break;
    case 0x0e:if((f->edx&255)!=0)error(f,15);else f->eax=1;break;
    case 0x19:f->eax&=0xff00;break;
    case 0x30:f->eax=4;f->ebx=0;f->ecx=0;break;
    case 0x40:
        if((f->ebx&0xffff)!=1&&(f->ebx&0xffff)!=2){error(f,5);break;}
        n=f->ecx&0xffff;if(n>4096)n=4096;
        ptr=address(f->ds,f->edx,n);if(!ptr){error(f,13);break;}
        for(k32 i=0;i<n;i++)output(read8(ptr+i));
        f->eax=n;break;
    case 0x39:case 0x3a:case 0x3c:case 0x41:case 0x43:case 0x56:case 0x5b:error(f,5);break;
    case 0x4a:if((f->es&0xffff)!=K386_SEGMENT||(f->ebx&0xffff)>0x1000)error(f,8);break;
    case 0x3d:case 0x3f:case 0x3e:case 0x42:case 0x2a:case 0x2c:
        s->request=ah;
        if(ah==0x3d){if(f->eax&3){error(f,5);s->request=0;break;}ptr=address(f->ds,f->edx,128);if(!ptr){error(f,13);s->request=0;break;}for(n=0;n<128;n++){transfer[n]=read8(ptr+n);if(!transfer[n])break;}if(n==128){error(f,3);s->request=0;break;}}
        if(ah==0x3f&&!address(f->ds,f->edx,f->ecx&0xffff)){error(f,13);s->request=0;break;}
        return 1;
    default:error(f,1);break;
    }
    return 0;
}
int kernel_trap(K386Frame *f) {
    K386Session *s=&sessions[selected];K386View *v=&k386_mailbox.view[selected];
    if(!(f->eflags&0x20000)){v->state=K386_FAULTED;v->fault=0x8000|f->vector;v->eip=f->eip;v->eflags=f->eflags;v->exit_code=f->error;return 1;}
    v->eflags=f->eflags;v->eip=f->eip;__asm__ volatile("movl %%cr3,%0":"=r"(v->cr3));
    if(f->vector==32){v->ticks++;s->frame=*f;return 1;}
    if(f->vector==13) {
        k32 pc=address(f->cs,f->eip,2);k8 op=pc?read8(pc):0;
        if(op==0xcd){int r=service(f,read8(pc+1));if(r!=2)f->eip+=2;s->frame=*f;return r!=0;}
        if(op==0xfa||op==0xfb){f->eip++;s->frame=*f;return 0;}
        if(op==0xf4){f->eip++;s->frame=*f;return 1;}
    }
    v->state=K386_FAULTED;v->fault=f->vector;v->eip=f->eip;
    if(f->vector==14)__asm__ volatile("movl %%cr2,%0":"=r"(v->cr2));
    s->frame=*f;return 1;
}
void kernel_finish(void) {
    K386View *v=&k386_mailbox.view[selected];
    int changed=0;
    for(k32 i=0;i<2000;i++){k8 c=read8(0xa0000+i*2);if((k8)v->screen[i]!=c)changed=1;v->screen[i]=c;}
    if(changed)v->flags++;
    v->counter=(k32)read8(0x80800)|(k32)read8(0x80801)<<8|(k32)read8(0x80802)<<16|(k32)read8(0x80803)<<24;
    v->keys=sessions[selected].count;
}
