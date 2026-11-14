#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "text.h"
#include "../filer/sjis.h"
static unsigned short plane[2][2000];static unsigned scrolls;
static unsigned get(void *p,unsigned which,unsigned cell){(void)p;assert(which<2&&cell<2000);return plane[which][cell];}
static void set(void *p,unsigned which,unsigned cell,unsigned value){(void)p;assert(which<2&&cell<2000);plane[which][cell]=value;}
static void fill(void *p,unsigned at,unsigned n,unsigned code,unsigned attr){assert(at<=2000&&n<=2000-at);for(unsigned i=at;i<at+n;i++){set(p,0,i,code);set(p,1,i,attr);}}
static void scroll(void *p,unsigned a){scrolls++;memmove(plane[0],plane[0]+80,1920*2);memmove(plane[1],plane[1]+80,1920*2);fill(p,1920,80,32,a);}
static const K98Surface surface={0,get,set,fill,scroll};
static void out(K98Text *t,const char *s){for(;*s;s++)k98_output(t,(unsigned char)*s,&surface);}
static void fresh(K98Text *t){k98_reset(t);fill(0,0,2000,32,K98_WHITE);scrolls=0;}
int main(void){
 K98Text a,b;fresh(&a);out(&a,"ABC\r\nDEF");assert(a.row==1&&a.column==3&&plane[0][80]=='D');
 fresh(&a);k98_output(&a,0x93,&surface);assert(!a.column&&a.pending==0x93);k98_output(&a,0xfa,&surface);assert(a.column==2&&plane[0][0]==sjis_vram(0x93,0xfa)&&!plane[0][1]);
 unsigned half;assert(k98_glyph(plane[0],0,&half)==0x93fa&&half==0);assert(k98_glyph(plane[0],1,&half)==0x93fa&&half==1);
 k98_reset(&b);out(&b,"z");assert(!b.pending);assert(!a.pending);/* per-session parser state */
 fresh(&a);k98_output(&a,0x93,&surface);k98_output(&a,'\r',&surface);assert(plane[0][0]=='?'&&a.column==0&&!a.pending);
 fresh(&a);a.column=79;k98_output(&a,0x93,&surface);k98_output(&a,0xfa,&surface);assert(plane[0][79]==32&&plane[0][80]==sjis_vram(0x93,0xfa)&&a.row==1&&a.column==2);
 fresh(&a);a.row=24;a.column=79;k98_output(&a,0x93,&surface);k98_output(&a,0xfa,&surface);assert(scrolls==1&&a.row==24&&a.column==2&&plane[0][1920]==sjis_vram(0x93,0xfa));
 fresh(&a);out(&a,"\033[31;4;7mX");assert(plane[1][0]==(0x41|8|4));out(&a,"\033[24;27;5mY");assert(plane[1][1]==0x43);out(&a,"\033[0mZ");assert(plane[1][2]==0xe1);
 out(&a,"\033[8mQ");assert(!(plane[1][3]&1));out(&a,"\033[28mR");assert(plane[1][4]&1);
 assert(k98_rgb(0x41)==0xff0000);assert(k98_rgb(0x81)==0x00ff00);assert(k98_rgb(0x21)==0x0000ff);assert(k98_rgb(0xe1)==0xffffff);
 assert(k98_ink(0x42,0xe1,0,1,0)==0x42);assert(k98_ink(0x42,0xe5,0,1,0)==0xbd);assert(!k98_ink(0xff,0xe0,0,1,0));assert(!k98_ink(0xff,0xe3,0,0,0));assert(k98_ink(0,0xe9,15,1,0)==255);assert(k98_ink(0,0xf1,3,1,0)==8);
 fresh(&a);out(&a,"\033[25;80H!");assert(a.row==24&&a.column==0&&scrolls==1);out(&a,"\033[999;999H");assert(a.row==24&&a.column==79);
 out(&a,"\033[1;1H\033[s\033[9;7H\033[u");assert(!a.row&&!a.column);
 fresh(&a);k98_output(&a,0x93,&surface);k98_output(&a,0xfa,&surface);out(&a,"\b");assert(!a.column);out(&a,"A");assert(plane[0][0]=='A'&&plane[0][1]==' ');
 fresh(&a);k98_output(&a,0x93,&surface);k98_output(&a,0xfa,&surface);out(&a,"\033[1;2H\033[K");assert(plane[0][0]==' '&&plane[0][1]==' ');
 fresh(&a);plane[1][80]=0x45;plane[0][80]='R';a.row=24;out(&a,"\n");assert(plane[0][0]=='R'&&plane[1][0]==0x45&&plane[1][1920]==0xe1);
 fresh(&a);out(&a,"\033[31m\033[2J");for(unsigned i=0;i<2000;i++)assert(plane[0][i]==' '&&plane[1][i]==0x41);
 fresh(&a);out(&a,"\033[1;2;3;4;99999mX");assert(plane[0][0]=='X'&&a.column==1&&!a.escape);
 for(unsigned lead=0x81;lead<=0xef;lead++){if(!sjis_lead(lead))continue;for(unsigned trail=0x40;trail<=0xfc;trail++)if(sjis_trail(trail))assert(k98_sjis(sjis_vram(lead,trail))==lead*256+trail);}
 fresh(&a);plane[0][79]=sjis_vram(0x93,0xfa);assert(k98_glyph(plane[0],79,&half)=='?');assert(k98_glyph(plane[0],80,&half)==' ');
 fresh(&a);a.columns=a.pitch=40;a.rows=20;out(&a,"\033[999;999H");assert(a.column==39&&a.row==19);out(&a,"\033[1;40H!");assert(a.row==1&&!a.column&&plane[0][39]=='!');
 out(&a,"\033[31;4m\033[24mX");assert(plane[1][40]==0x41);out(&a,"\033[2J");assert(plane[0][799]==' ');
 fresh(&a);k98_output(&a,0xeb,&surface);k98_output(&a,0x9f,&surface);assert(plane[0][0]==0xa156&&plane[0][1]==0xa156);
 puts("PC-98 terminal: SJIS, planes, colours/effects, split input, wrap, erase, scroll and malformed CSI passed");return 0;
}
