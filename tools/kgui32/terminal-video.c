#include "terminal-video.h"
#include "../k386/text.h"
int kt_video_extra(const KvMeta *m){return m->version==1&&((m->flags&(KV_GRAPH|KV_SEMI))||m->font_writes||m->columns!=80||m->rows!=25||m->text_pitch!=80||m->text_start||!(m->flags&KV_TEXT));}
const unsigned char *kt_video_gaiji(const KvCanvas *v,unsigned raw){
 static const unsigned char blank[32];unsigned ku=raw&127,ten=(raw>>8)&127;
 if(ku<0x56||ku>0x57||ten<0x21||ten>0x7e)return 0;
 unsigned i=(ku-0x56)*94+ten-0x21;
 return v&&(v->meta.valid[i/8]&(1u<<(i&7)))?v->font+i*32:blank;
}
/* 0..15 private graphics palette; 16..23 PC-98 text RGB; 24 black.
 * Decode each planar byte once and draw nonblank cells, keeping division and
 * glyph lookup outside the pixel loop. Maps follow nearest-neighbour scaling. */
void kt_video_compose(const KvCanvas *v,unsigned char *out,unsigned blink,
 const unsigned char *(*glyph)(void *,unsigned),void *context){
 const KvMeta *m=&v->meta;unsigned width=m->flags&KV_GRAPH?m->width:640,height=m->flags&KV_GRAPH?m->height:400;
 if(!width||width>640||!height||height>400||!m->columns||m->columns>80||!m->rows||m->rows>25||!m->text_pitch)return;
 if(m->flags&KV_GRAPH){
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x+=8){
   unsigned at=(m->start+y*m->pitch+x/8)&32767;
   unsigned a=v->planes[0][at],b=v->planes[1][at],c=v->planes[2][at],d=v->planes[3][at];
   for(unsigned bit=0;bit<8&&x+bit<width;bit++){
    unsigned mask=128>>bit;out[y*width+x+bit]=(a&mask?1:0)|(b&mask?2:0)|(c&mask?4:0)|(d&mask?8:0);
   }
  }
 }else{unsigned char *p=out;unsigned n=width*height;__asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(24):"memory","cc");}
 if(!(m->flags&KV_TEXT))return;
 static unsigned short left[81],top[26];static unsigned char xbit[640],yscan[400];
 static unsigned old_width,old_height,old_columns,old_rows;
 if(width!=old_width||height!=old_height||m->columns!=old_columns||m->rows!=old_rows){
  for(unsigned x=0;x<width;x++)xbit[x]=(x*m->columns*8/width)&7;
  for(unsigned y=0;y<height;y++)yscan[y]=(y*m->rows*16/height)&15;
  for(unsigned c=0;c<=m->columns;c++)left[c]=(c*width+m->columns-1)/m->columns;
  for(unsigned r=0;r<=m->rows;r++)top[r]=(r*height+m->rows-1)/m->rows;
  old_width=width;old_height=height;old_columns=m->columns;old_rows=m->rows;
 }
 unsigned short codes[80],chars[80];unsigned char halves[80];
 for(unsigned row=0;row<m->rows;row++){
  for(unsigned c=0;c<80;c++)codes[c]=c<m->columns?v->codes[(m->text_start+row*m->text_pitch+c)&4095]:32;
  if(m->columns==40&&(codes[39]>>8))codes[39]='?';
  k98_row(codes,chars,halves);
  for(unsigned col=0;col<m->columns;col++){
   unsigned attr=v->attributes[(m->text_start+row*m->text_pitch+col)&4095],semi=(m->flags&KV_SEMI)&&(attr&16);
   if(chars[col]==32&&!(attr&28))continue;
   const unsigned char *font=0;
   if(!semi){font=kt_video_gaiji(v,codes[col-halves[col]]);if(!font)font=glyph(context,chars[col]);}
   unsigned style=semi?attr&~16u:attr,ink=16+((attr>>5)&7);
   for(unsigned y=top[row];y<top[row+1];y++){
    unsigned scan=yscan[y],bits;
    if(semi){unsigned b=codes[col]&255;bits=((b>>(scan/4))&1?240:0)|((b>>(4+scan/4))&1?15:0);}
    else bits=font[halves[col]*16+scan];
    bits=k98_ink(bits,style,scan,blink,0);if(!bits)continue;
    for(unsigned x=left[col];x<left[col+1];x++)if(bits&(128>>xbit[x]))out[y*width+x]=(unsigned char)ink;
   }
  }
 }
 if((m->flags&KV_CURSOR)&&blink){
  unsigned cursor=(m->text_cursor-m->text_start)&4095,row=cursor/m->text_pitch,col=cursor%m->text_pitch;
  if(row<m->rows&&col<m->columns)for(unsigned y=top[row];y<top[row+1];y++)if(yscan[y]>=14)for(unsigned x=left[col];x<left[col+1];x++)out[y*width+x]=22;
 }
}
