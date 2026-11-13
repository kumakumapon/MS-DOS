/* Strict bounded BMP24/BI_RGB decoder. Output is private RGB565, not VRAM. */
#include "bitmap.h"
static u32 word(const u8 *p){return p[0]|(u32)p[1]<<8;}
static u32 dword(const u8 *p){return word(p)|word(p+2)<<16;}
int kg_bmp_header(const u8 *p,unsigned n,u32 filebytes,KgBmp *out){
 if(n<54||p[0]!='B'||p[1]!='M'||dword(p+2)!=filebytes||dword(p+6)||dword(p+14)!=40||word(p+26)!=1||word(p+28)!=24||dword(p+30))return -22;
 unsigned w=dword(p+18);int h=(int)dword(p+22);
 if(!w||w>256||!h||h < -192||h>192)return -22;
 unsigned height=h<0?-h:h,stride=(w*3+3)&~3u,offset=dword(p+10),bytes=stride*height;
 if(offset<54||offset>4096||offset>filebytes||bytes!=filebytes-offset||(dword(p+34)&&dword(p+34)!=bytes))return -22;
 *out=(KgBmp){w,height,stride,offset,bytes,h<0};return 0;
}
int kg_bmp_row(const KgBmp *b,unsigned row,const u8 *p,unsigned n,u16 *out,unsigned pixels){
 if(!b->width||b->width>256||!b->height||b->height>192||b->stride!=((b->width*3+3)&~3u)||row>=b->height||n<b->stride||pixels<b->width*b->height)return -22;
 unsigned y=b->topdown?row:b->height-1-row;
 for(unsigned x=0;x<b->width;x++)out[y*b->width+x]=((u16)(p[x*3+2]>>3)<<11)|((u16)(p[x*3+1]>>2)<<5)|(p[x*3]>>3);
 return 0;
}
void kg_bitmap_invert(u16 *pixels,unsigned n){for(unsigned i=0;i<n;i++)pixels[i]^=65535;}
void kg_bitmap_flip(u16 *pixels,unsigned w,unsigned h){for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w/2;x++){u16 v=pixels[y*w+x];pixels[y*w+x]=pixels[y*w+w-1-x];pixels[y*w+w-1-x]=v;}}

int kg_bitmap_fill(u16 *p,unsigned width,unsigned height,int x,int y,int w,int h,u16 color){
 if(!p||!width||width>256||!height||height>192)return -22;
 if(w<=0||h<=0)return 0;
 long long right=(long long)x+w,bottom=(long long)y+h;
 if(x<0)x=0;
 if(y<0)y=0;
 if(right>(int)width)right=width;
 if(bottom>(int)height)bottom=height;
 for(int row=y;row<bottom;row++)for(int col=x;col<right;col++)p[(unsigned)row*width+col]=color;
 return 0;
}
int kg_bitmap_line(u16 *p,unsigned w,unsigned h,int x,int y,int endx,int endy,u16 color){
 if(!p||!w||w>256||!h||h>192||x < -1024||x>1024||y < -1024||y>1024||endx < -1024||endx>1024||endy < -1024||endy>1024)return -22;
 int dx=endx>x?endx-x:x-endx,dy=endy>y?y-endy:endy-y,sx=x<endx?1:-1,sy=y<endy?1:-1,error=dx+dy;
 for(;;){kg_bitmap_fill(p,w,h,x-1,y-1,3,3,color);if(x==endx&&y==endy)break;int twice=error*2;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}}
 return 0;
}
