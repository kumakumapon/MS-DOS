#include "terminal-video.h"
#include "../k386/text.h"
int kt_video_extra(const KvMeta *m){return m->version==1&&((m->flags&(KV_GRAPH|KV_SEMI))||m->font_writes||m->columns!=80||m->rows!=25||m->text_pitch!=80||m->text_start||!(m->flags&KV_TEXT));}
/* Indexed canvas: 0..15 private graphic palette; 16..23 PC-98 text RGB;
 * 24 black. The caller clips and scales this bounded native display. */
void kt_video_compose(const KvCanvas *v,unsigned char *out,unsigned blink,
 const unsigned char *(*glyph)(void *,unsigned),void *context){
 const KvMeta *m=&v->meta;unsigned width=m->flags&KV_GRAPH?m->width:640,height=m->flags&KV_GRAPH?m->height:400;
 if(!width||width>640||!height||height>400||!m->columns||m->columns>80||!m->rows||m->rows>25)return;
 unsigned short codes[80],chars[80];unsigned char halves[80];
 static unsigned char rowfonts[80][32];
 unsigned last=~0u;
 static const unsigned char blank[32];
 for(unsigned y=0;y<height;y++){
  unsigned ty=y*m->rows*16/height,row=ty/16,scan=ty&15;
  if(last!=row){for(unsigned i=0;i<80;i++)codes[i]=i<m->columns?v->codes[(m->text_start+row*m->text_pitch+i)&4095]:32;
   if(m->columns==40&&(codes[39]>>8))codes[39]='?';
   k98_row(codes,chars,halves);
   for(unsigned col=0;col<m->columns;col++){
    unsigned raw=codes[col-halves[col]],ku=raw&127,ten=(raw>>8)&127;
    const unsigned char *font;
    if(ku>=0x56&&ku<=0x57&&ten>=0x21&&ten<=0x7e){unsigned i=(ku-0x56)*94+ten-0x21;font=m->valid[i/8]&(1u<<(i&7))?v->font+i*32:blank;}
    else font=glyph(context,chars[col]);
    for(unsigned i=0;i<(chars[col]>255?32u:16u);i++)rowfonts[col][i]=font[i];
   }
   last=row;
  }
  for(unsigned x=0;x<width;x++){
   unsigned index=24;
   if(m->flags&KV_GRAPH){unsigned at=(m->start+y*m->pitch+x/8)&32767,mask=128>>(x&7);index=0;for(unsigned p=0;p<4;p++)if(v->planes[p][at]&mask)index|=1u<<p;}
   if(m->flags&KV_TEXT){unsigned tx=x*m->columns*8/width,col=tx/8,bit=tx&7;
    unsigned attr=v->attributes[(m->text_start+row*m->text_pitch+col)&4095],bits;
    if((m->flags&KV_SEMI)&&(attr&16)){unsigned b=codes[col]&255;bits=((b>>(scan/4))&1?240:0)|((b>>(4+scan/4))&1?15:0);attr&=~16u;}
    else bits=rowfonts[col][halves[col]*16+scan];
    if(k98_ink(bits,attr,scan,blink,0)&(128>>bit))index=16+((attr>>5)&7);
    if((m->flags&KV_CURSOR)&&blink&&scan>=14&&((m->text_start+row*m->text_pitch+col)&4095)==m->text_cursor)index=22;
   }
   out[y*width+x]=(unsigned char)index;
  }
 }
}
