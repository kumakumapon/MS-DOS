#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "terminal-video.h"
static KvCanvas v,peer;
static unsigned char pixels[640*400],bits[32];
static const unsigned char *glyph(void *p,unsigned c){(void)p;memset(bits,c=='X'?255:0,sizeof bits);return bits;}
int main(void){
 KvVideo model;kv_reset(&model);v.meta=model.meta;
 assert(!kt_video_extra(&v.meta));v.meta.flags=KV_GRAPH;assert(kt_video_extra(&v.meta));
 for(unsigned x=0;x<16;x++)for(unsigned p=0;p<4;p++)if(x&(1u<<p))v.planes[p][x/8]|=128>>(x&7);
 kt_video_compose(&v,pixels,1,glyph,0);for(unsigned i=0;i<16;i++)assert(pixels[i]==i);
 v.meta.start=32767;v.planes[0][32767]=128;kt_video_compose(&v,pixels,1,glyph,0);assert(pixels[0]==1);
 v.meta=model.meta;v.meta.flags|=KV_SEMI;v.codes[0]=1;v.attributes[0]=0xf1;
 kt_video_compose(&v,pixels,1,glyph,0);assert(pixels[0]==23&&pixels[3]==23&&pixels[4]==24&&pixels[4*640]==24);
 /* The same gaiji word in two owners must select their private bytes. */
 memset(&v,0,sizeof v);v.meta=model.meta;v.codes[0]=v.codes[1]=0x2156;v.attributes[0]=v.attributes[1]=0xe1;
 v.meta.font_writes=32;v.meta.valid[0]=1;v.font[0]=128;v.font[16]=1;peer=v;peer.font[0]=64;
 kt_video_compose(&v,pixels,1,glyph,0);assert(pixels[0]==23&&pixels[1]==24&&pixels[15]==23);
 kt_video_compose(&peer,pixels,1,glyph,0);assert(pixels[0]==24&&pixels[1]==23);
 v.meta.valid[0]=0;kt_video_compose(&v,pixels,1,glyph,0);assert(pixels[0]==24&&pixels[15]==24);
 assert(kt_video_gaiji(0,0xa156)&&!kt_video_gaiji(0,0xa156)[0]);
 assert(!kt_video_gaiji(&v,0xa156)[0]&&!kt_video_gaiji(&v,'V'));
 /* 40x20 characters, physical pitch, ring start and 20-line raster. */
 memset(&v,0,sizeof v);v.meta=model.meta;v.meta.columns=40;v.meta.rows=20;v.meta.text_pitch=40;v.meta.text_start=4095;
 v.codes[4095]='X';v.attributes[4095]=0x41;v.codes[39]='X';v.attributes[39]=0x81;
 kt_video_compose(&v,pixels,1,glyph,0);assert(pixels[0]==18&&pixels[15]==18&&pixels[16]==24&&pixels[20*640]==20);
 puts("Terminal display: planes, palette indices, semigraphics, private/unset gaiji and 40x20 ring modes passed");
}
