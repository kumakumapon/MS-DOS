/* Strict bounded BMP24/BI_RGB decoder. Output is private RGB565, not VRAM. */
#include "bitmap.h"
static u32 dword(const u8 *p){return p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
int kg_bmp_header(const u8 *p,unsigned n,u32 filebytes,KgBmp *out){
 if(!p||n<54||p[0]!='B'||p[1]!='M'||dword(p+2)!=filebytes||dword(p+6)||dword(p+14)!=40||p[26]!=1||p[27]||p[29]||dword(p+30))return -22;
 unsigned bits=p[28],w=dword(p+18);int h=(int)dword(p+22);
 if((bits!=8&&bits!=16&&bits!=24&&bits!=32)||!w||w>KG_BITMAP_WIDTH||!h||h < -(int)KG_BITMAP_HEIGHT||h>(int)KG_BITMAP_HEIGHT)return -22;
 unsigned height=h<0?(unsigned)-h:(unsigned)h,stride=((w*(bits/8)+3)&~3u),offset=dword(p+10),bytes=stride*height;
 unsigned palette=bits==8?(dword(p+46)?dword(p+46):256):0;
 if(palette>256||offset<54+palette*4||offset>4096||offset>filebytes||bytes!=filebytes-offset||(dword(p+34)&&dword(p+34)!=bytes))return -22;
 *out=(KgBmp){w,height,stride,offset,bytes,h<0,bits,palette};return 0;
}
static u16 color555(unsigned v){unsigned g=v>>5&31;return (u16)((v>>10&31)<<11|(g<<1|g>>4)<<5|(v&31));}
int kg_bmp_row(const KgBmp *b,unsigned row,const u8 *p,unsigned n,u16 *out,unsigned pixels){
 unsigned bits=b->bits?b->bits:24;
 if(!b->width||b->width>KG_BITMAP_WIDTH||!b->height||b->height>KG_BITMAP_HEIGHT||(bits!=16&&bits!=24&&bits!=32)||b->stride!=((b->width*(bits/8)+3)&~3u)||row>=b->height||n<b->stride||pixels<b->width*b->height)return -22;
 unsigned y=b->topdown?row:b->height-1-row;
 for(unsigned x=0;x<b->width;x++){const u8 *q=p+x*(bits/8);out[y*b->width+x]=bits==16?color555(q[0]|(unsigned)q[1]<<8):((u16)(q[2]>>3)<<11)|((u16)(q[1]>>2)<<5)|(q[0]>>3);}
 return 0;
}
int kg_bmp_decode(const u8 *data,unsigned bytes,KgBmp *out,u16 *pixels,unsigned capacity){
 KgBmp b;int e=kg_bmp_header(data,bytes,bytes,&b);if(e)return e;if(!pixels||capacity<b.width*b.height)return -22;
 if(b.bits==8){
  for(unsigned y=0;y<b.height;y++)for(unsigned x=0;x<b.width;x++)if(data[b.offset+y*b.stride+x]>=b.palette)return -22;
  for(unsigned y=0;y<b.height;y++)for(unsigned x=0;x<b.width;x++){const u8 *p=data+54+data[b.offset+y*b.stride+x]*4;pixels[(b.topdown?y:b.height-1-y)*b.width+x]=(u16)((p[2]>>3)<<11|(p[1]>>2)<<5|(p[0]>>3));}
 }else for(unsigned y=0;y<b.height;y++){e=kg_bmp_row(&b,y,data+b.offset+y*b.stride,b.stride,pixels,capacity);if(e)return e;}
 *out=b;return 0;
}
void kg_bitmap_invert(u16 *pixels,unsigned n){for(unsigned i=0;i<n;i++)pixels[i]^=65535;}
void kg_bitmap_flip(u16 *pixels,unsigned w,unsigned h){for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w/2;x++){u16 v=pixels[y*w+x];pixels[y*w+x]=pixels[y*w+w-1-x];pixels[y*w+w-1-x]=v;}}

int kg_bitmap_fill(u16 *p,unsigned width,unsigned height,int x,int y,int w,int h,u16 color){
 if(!p||!width||width>KG_BITMAP_WIDTH||!height||height>KG_BITMAP_HEIGHT)return -22;
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
 if(!p||!w||w>KG_BITMAP_WIDTH||!h||h>KG_BITMAP_HEIGHT||x < -1024||x>1024||y < -1024||y>1024||endx < -1024||endx>1024||endy < -1024||endy>1024)return -22;
 int dx=endx>x?endx-x:x-endx,dy=endy>y?y-endy:endy-y,sx=x<endx?1:-1,sy=y<endy?1:-1,error=dx+dy;
 for(;;){kg_bitmap_fill(p,w,h,x-1,y-1,3,3,color);if(x==endx&&y==endy)break;int twice=error*2;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}}
 return 0;
}

int kg_bmp_encode(const u16 *pixels,unsigned w,unsigned h,u8 *out,unsigned capacity){
 if(!pixels||!out||!w||w>KG_BITMAP_WIDTH||!h||h>KG_BITMAP_HEIGHT)return -22;
 unsigned stride=(w*3+3)&~3u,bytes=54+stride*h;
 if(capacity<bytes)return -28;
 for(unsigned i=0;i<bytes;i++)out[i]=0;
 out[0]='B';out[1]='M';
 unsigned at[]={2,10,14,18,22,26,28,34},value[]={bytes,54,40,w,h,1,24,stride*h};
 for(unsigned j=0;j<8;j++)for(unsigned i=0;i<(at[j]==26||at[j]==28?2u:4u);i++)out[at[j]+i]=value[j]>>(i*8);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  unsigned v=pixels[(h-1-y)*w+x],b=v&31,g=v>>5&63,r=v>>11;
  unsigned at=54+y*stride+x*3;out[at]=(b<<3)|(b>>2);out[at+1]=(g<<2)|(g>>4);out[at+2]=(r<<3)|(r>>2);
 }
 return bytes;
}
