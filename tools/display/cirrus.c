/* Original PC-9821 Cirrus backend. Port mapping follows NP2kai's native board
 * interface (CLGDTYPE=90); no VGA BIOS, firmware or Windows resources used. */
#include "cirrus.h"
#include "../window/hardware.h"
static u8 seq[32],gr[64],cr[64],ar[21],dac,hidden,misc,unlock,window,relay,video;
static unsigned width,height,active,identity;
unsigned cirrus_identity(void){return identity;}
static int bank=-1;
static void out(unsigned port,u8 value){__asm__ volatile("outb %b0,%w1"::"a"(value),"d"(port));}
static u8 in(unsigned port){u8 v;__asm__ volatile("inb %w1,%0":"=a"(v):"d"(port));return v;}
static u8 readreg(unsigned port,unsigned index){out(port,index);return in(port+1);}
static void reg(unsigned port,unsigned index,unsigned value){out(port,index);out(port+1,value);}
static u8 attribute(unsigned index){in(0xdaa);out(0xca0,index);return in(0xca1);}
static void attr(unsigned index,unsigned value){in(0xdaa);out(0xca0,index);out(0xca0,value);}
static u8 hiddendac(void){in(0xca8);for(unsigned i=0;i<4;i++)in(0xca6);return in(0xca6);}
static void setdac(unsigned value){in(0xca8);for(unsigned i=0;i<4;i++)in(0xca6);out(0xca6,value);}
unsigned cirrus_width(void){return width;}
unsigned cirrus_height(void){return height;}
int cirrus_open(unsigned w,unsigned h){
    if(active||!((w==800&&h==600)||(w==1024&&h==768)))return 0;
    out(0xfaa,0);unsigned board=in(0xfab);
    identity=board<<8;
    if(board!=0x5a)return 0;
    misc=in(0xcac);out(0xca2,misc|1);
    unlock=readreg(0xca4,6);reg(0xca4,6,0x12);
    unsigned chip=readreg(0xda4,0x27);identity|=chip;
    if(chip!=0xa0&&chip!=0xa8){reg(0xca4,6,unlock);out(0xca2,misc);return 0;}
    for(unsigned i=0;i<32;i++)seq[i]=readreg(0xca4,i);
    for(unsigned i=0;i<64;i++){gr[i]=readreg(0xcae,i);cr[i]=readreg(0xda4,i);}
    for(unsigned i=0;i<21;i++)ar[i]=attribute(i);
    video=in(0xff82);in(0xca8);dac=in(0xca6);hidden=hiddendac();
    out(0xfa2,1);window=in(0xfa3);out(0xfaa,3);relay=in(0xfab);
    reg(0xca4,0,1);reg(0xca4,1,0x21);reg(0xca4,2,15);reg(0xca4,4,0x0e);
    reg(0xca4,7,7);reg(0xcae,5,0x40);reg(0xcae,6,5);reg(0xcae,0x0b,0x20);
    reg(0xda4,0x11,0);reg(0xda4,1,w/8-1);
    unsigned lines=h-1;
    reg(0xda4,0x12,lines&255);reg(0xda4,7,0x10|((lines>>7)&2)|((lines>>3)&0x40));
    reg(0xda4,9,0x40);reg(0xda4,0x13,w/4);reg(0xda4,0x1b,((w/4)&256)?0x10:0);
    reg(0xda4,0x0c,0);reg(0xda4,0x0d,0);reg(0xda4,0x18,255);reg(0xda4,0x1a,0);
    reg(0xda4,0x17,0xa3);attr(0x10,1);attr(0x12,15);attr(0x13,0);
    setdac(0xc1);out(0xca6,255);out(0xfa2,1);out(0xfa3,0x10);
    reg(0xca4,0,3);reg(0xca4,1,1);in(0xdaa);out(0xca0,0x20);
    width=w;height=h;active=1;bank=-1;
    /* Composition and wallpaper occupy three disjoint VRAM megabyte ranges.
       Reject small/aliased boards before selecting their visible output. */
    u16 first=cirrus_pixel(0),third=cirrus_pixel(0x200000ul);
    cirrus_fill(0,1,0x1357);cirrus_fill(0x200000ul,1,0x2468);
    int capacity=cirrus_pixel(0)==0x1357&&cirrus_pixel(0x200000ul)==0x2468;
    cirrus_fill(0,1,first);cirrus_fill(0x200000ul,1,third);
    if(!capacity){cirrus_close();return 0;}
    out(0xff82,1);out(0xfaa,3);out(0xfab,2);
    return 1;
}
static unsigned select(u32 offset){
    unsigned b=(offset>>15)*2;
    if(bank!=(int)b){reg(0xcae,9,b);bank=b;}
    return offset&32767;
}
void cirrus_write(u32 offset,unsigned segment,unsigned offset16,unsigned count){
    if(!active||offset>=0x400000ul||count>0x400000ul-offset)return;
    while(count){unsigned at=select(offset),n=32768-at;if(n>count)n=count;
        mem_copy(0xb000,at,segment,offset16,n);offset+=n;offset16+=n;count-=n;}
}
void cirrus_read(u32 offset,unsigned segment,unsigned offset16,unsigned count){
    if(!active||offset>=0x400000ul||count>0x400000ul-offset)return;
    while(count){unsigned at=select(offset),n=32768-at;if(n>count)n=count;
        mem_copy(segment,offset16,0xb000,at,n);offset+=n;offset16+=n;count-=n;}
}
void cirrus_fill(u32 offset,unsigned count,u16 color){
    if(!active||offset>=0x400000ul||count>(0x400000ul-offset)/2)return;
    while(count){unsigned at=select(offset),n=(32768-at)/2;if(n>count)n=count;
        unsigned ptr=at,words=n,seg=0xb000;
        __asm__ volatile("pushw %%es; movw %w3,%%es; rep stosw; popw %%es":"+D"(ptr),"+c"(words):"a"(color),"r"(seg):"memory");
        offset+=n*2;count-=n;}
}
u16 cirrus_pixel(u32 offset){u16 value=0;unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));cirrus_read(offset,ds,(unsigned)&value,2);return value;}
int cirrus_blit(u32 source,u32 destination,unsigned bytes,unsigned rows,unsigned pitch){
    if(!active||!bytes||bytes>8192||!rows||rows>2048||pitch<bytes)return 0;
    u32 length=(u32)(rows-1)*pitch+bytes;
    if(source>=0x400000ul||destination>=0x400000ul||length>0x400000ul-source||length>0x400000ul-destination)return 0;
    if(readreg(0xcae,0x31)&1)return 0;
    reg(0xcae,0x31,0);reg(0xcae,0x20,bytes-1);reg(0xcae,0x21,(bytes-1)>>8);
    reg(0xcae,0x22,rows-1);reg(0xcae,0x23,(rows-1)>>8);
    reg(0xcae,0x24,pitch);reg(0xcae,0x25,pitch>>8);reg(0xcae,0x26,pitch);reg(0xcae,0x27,pitch>>8);
    reg(0xcae,0x28,destination);reg(0xcae,0x29,destination>>8);reg(0xcae,0x2a,destination>>16);
    reg(0xcae,0x2c,source);reg(0xcae,0x2d,source>>8);reg(0xcae,0x2e,source>>16);
    reg(0xcae,0x2f,0);reg(0xcae,0x30,0x10);reg(0xcae,0x32,0x0d);reg(0xcae,0x33,0);reg(0xcae,0x31,2);
    for(unsigned n=0;n<65535;n++)if(!(readreg(0xcae,0x31)&1))return 1;
    reg(0xcae,0x31,4);reg(0xcae,0x31,0);return 0;
}
void cirrus_close(void){
    if(!active)return;
    out(0xfaa,3);out(0xfab,relay);out(0xff82,video);
    reg(0xcae,0x31,4);reg(0xcae,0x31,0);
    for(unsigned i=0x20;i<=0x33;i++)if(i!=0x31)reg(0xcae,i,gr[i]);
    reg(0xca4,1,0x21);reg(0xda4,0x11,0);
    static const u8 ci[]={1,7,9,12,13,0x12,0x13,0x17,0x18,0x1a,0x1b,0x11};
    static const u8 gi[]={5,6,9,0x0b},si[]={2,4,7,0,1},ai[]={0x10,0x12,0x13};
    for(unsigned i=0;i<sizeof ci;i++)reg(0xda4,ci[i],cr[ci[i]]);
    for(unsigned i=0;i<sizeof gi;i++)reg(0xcae,gi[i],gr[gi[i]]);
    for(unsigned i=0;i<sizeof si;i++)reg(0xca4,si[i],seq[si[i]]);
    for(unsigned i=0;i<sizeof ai;i++)attr(ai[i],ar[ai[i]]);
    setdac(hidden);out(0xca6,dac);out(0xca2,misc);out(0xfa2,1);out(0xfa3,window);
    reg(0xca4,6,unlock);in(0xdaa);out(0xca0,0x20);active=0;width=height=0;bank=-1;
}
