#include "video.h"
unsigned kv_plane(unsigned p){return p==3?0xe0000u:0xa8000u+p*0x8000u;}
int kv_region(unsigned region,unsigned offset,unsigned bytes,unsigned *address){
 unsigned base,limit;
 if(region==1){base=KV_FONT_BASE;limit=KV_FONT_BYTES;}
 else if(region>=2&&region<=5){base=kv_plane(region-2);limit=32768;}
 else if(region==6||region==7){base=0xa0000+(region-6)*8192;limit=8192;}
 else return -22;
 if(!bytes||bytes>960||offset>limit||bytes>limit-offset)return -22;
 *address=base+offset;return 0;
}
static void palette(KvVideo *v){
 for(unsigned c=0;c<16;c++)for(unsigned component=0;component<3;component++){
  unsigned bit=component==0?2:component==1?4:1;
  unsigned index=c&7,packed=v->digital[index&3],color=index<4?packed>>4:packed&15;
  v->meta.palette[c][component]=v->meta.flags&KV_ANALOG?v->analog[c][component]:(color&bit?15:0);
 }
}
void kv_reset(KvVideo *v){
 *v=(KvVideo){.meta={.version=1,.size=sizeof(KvMeta),.flags=KV_TEXT|KV_CURSOR|KV_GAIJI,.columns=80,.rows=25,.width=640,.height=400,.pitch=80,.text_pitch=80}};
 for(unsigned i=0;i<2;i++){v->gdc[i].mask=65535;v->gdc[i].count=1;v->gdc[i].pitch=i?40:80;v->gdc[i].raster=16;}
 for(unsigned c=0;c<4;c++)v->digital[c]=(c<<4)|(c+4);
 for(unsigned c=0;c<16;c++)for(unsigned k=0;k<3;k++)v->analog[c][k]=c==8?4:c&(k==0?2:k==1?4:1)?(c<8?7:15):0;
 palette(v);
}
static int font_index(const KvVideo *v){
 unsigned row=v->cg_low&127,ten=v->cg_high&127;
 return row>=0x56&&row<=0x57&&ten>=0x21&&ten<=0x7e?(int)((row-0x56)*94+ten-0x21):-1;
}
static unsigned font_address(const KvVideo *v,unsigned index){return KV_FONT_BASE+index*32+(v->cg_line&32?0:16)+(v->cg_line&15);}
static unsigned word_address(const KvGdc *g,unsigned channel){return channel?kv_plane((g->cursor>>14)&3)+(g->cursor&16383)*2:0xa0000+(g->cursor&8191)*2;}
static int words_write(KvGdc *g,unsigned channel,unsigned data,const KvMemory *io){
 unsigned mask=g->mask,op=g->command&3,left=g->count;unsigned char bytes[1024];
 if((g->command&24)==16)mask&=255;
 if((g->command&24)==24){mask&=65280;data<<=8;}
 while(left){unsigned limit=channel?16384:8192,n=limit-(g->cursor&(limit-1));if(n>left)n=left;if(n>512)n=512;
  unsigned address=word_address(g,channel);
  if((op||mask!=65535)&&!io->read(io->context,address,bytes,n*2))return -5;
  for(unsigned i=0;i<n;i++){
   unsigned old=op||mask!=65535?bytes[i*2]|(unsigned)bytes[i*2+1]<<8:0;
   unsigned value=op==0?(old&~mask)|(data&mask):op==1?old^(data&mask):op==2?old&~(data&mask):old|(data&mask);
   bytes[i*2]=value;bytes[i*2+1]=value>>8;
  }
  if(!io->write(io->context,address,bytes,n*2))return -5;
  g->cursor=(g->cursor+n)&(channel?65535:8191);left-=n;
 }return KV_CHANGED;
}
static int complete(KvVideo *v,unsigned channel,const KvMemory *io){
 KvGdc *g=&v->gdc[channel];unsigned char *p=g->parameters;unsigned command=g->command;
 if(command==0x49){g->cursor=(p[0]|(unsigned)p[1]<<8);g->dot=p[2]>>4;if(p[2]&3)return -22;if(!channel){v->meta.text_cursor=g->cursor&4095;return KV_CURSOR_CHANGED;}return 0;}
 if(command==0x4a){g->mask=p[0]|(unsigned)p[1]<<8;return 0;}
 if(command==0x47){if(!p[0]||p[0]>80)return -22;g->pitch=p[0];if(channel)v->meta.pitch=p[0]*2;else v->meta.text_pitch=p[0];return KV_CHANGED|KV_MODE_CHANGED;}
 if(command==0x4b){unsigned raster=(p[0]&31)+1;if(raster!=16&&raster!=20)return -38;g->raster=raster;if(!channel){v->meta.rows=400/raster;v->meta.flags=(v->meta.flags&~KV_CURSOR)|(p[0]&128?KV_CURSOR:0);}return KV_CHANGED|KV_MODE_CHANGED;}
 if(command==0||command==14||command==15){unsigned w=(p[1]+2)*(channel?16:1),h=p[6]|(unsigned)(p[7]&3)<<8;if(!h||h>400||(channel?(w>640):(w!=40&&w!=80)))return -22;if(channel){v->meta.width=w;v->meta.height=h;}else v->meta.columns=w;return KV_CHANGED|KV_MODE_CHANGED;}
 if(command==0x4c){if(p[0]!=2)return -38;g->count=(p[1]|(unsigned)(p[2]&63)<<8)+1;return 0;}
 if((command&0xe4)==0x20){unsigned data=p[0]|(unsigned)p[1]<<8;return words_write(g,channel,data,io);}
 return -38;
}
static int gdc_command(KvVideo *v,unsigned channel,unsigned value){
 KvGdc *g=&v->gdc[channel];g->command=value;g->used=g->phase=g->reply_at=g->reply_count=0;
 if(value==12||value==13){unsigned flag=channel?KV_GRAPH:KV_TEXT;if(value==13)v->meta.flags|=flag;else v->meta.flags&=~flag;return KV_CHANGED|KV_MODE_CHANGED;}
 if(value==0||value==14||value==15)g->need=8;
 else if(value==0x47)g->need=1;
 else if(value==0x49||value==0x4b)g->need=3;
 else if(value==0x4a)g->need=2;
 else if(value==0x4c)g->need=11;
 else if((value&0xe4)==0x20){unsigned type=(value>>3)&3;if(type==1)return -38;g->need=type?1:2;}
 else if(value==0xe0){g->need=0;g->reply_count=5;g->reply[0]=g->cursor;g->reply[1]=g->cursor>>8;g->reply[2]=g->dot<<4;g->reply[3]=g->mask;g->reply[4]=g->mask>>8;}
 else if((value&0xe4)==0xa0||(value>=0x70&&value<=0x7f))g->need=0;
 else return -38;
 return 0;
}
int kv_out(KvVideo *v,unsigned port,unsigned value,const KvMemory *io){
 if(value>255)return -22;
 v->meta.io_count++;
 if(port==0xa1){v->cg_high=value;return 0;}if(port==0xa3){v->cg_low=value;return 0;}if(port==0xa5){v->cg_line=value;return 0;}
 if(port==0xa9){int i=font_index(v);if(i<0)return 0;unsigned char b=value;if(!io->write(io->context,font_address(v,i),&b,1))return -5;v->meta.valid[i/8]|=1u<<(i&7);v->meta.font_writes++;return KV_CHANGED;}
 if(port==0x68){if(value>1)return -38;unsigned bit=1u<<(value>>1);if(value&1)v->mode1|=bit;else v->mode1&=~bit;v->meta.flags=(v->meta.flags&~KV_SEMI)|(v->mode1&1?KV_SEMI:0);return KV_CHANGED|KV_MODE_CHANGED;}
 if(port==0x6a){if(value>1)return -38;v->meta.flags=(v->meta.flags&~KV_ANALOG)|(value?KV_ANALOG:0);palette(v);return KV_CHANGED;}
 if(port>=0xa8&&port<=0xae&&!(port&1)){
  if(v->meta.flags&KV_ANALOG){if(port==0xa8)v->palette=value&15;else v->analog[v->palette][port==0xac?0:port==0xaa?1:2]=value&15;}
  else {unsigned i=port==0xa8?3:port==0xaa?1:port==0xac?2:0;v->digital[i]=value&0x77;}
  palette(v);return KV_CHANGED;
 }
 if(port==0xa4||port==0xa6)return value?-38:0;
 if(port==0x62||port==0xa2)return gdc_command(v,port==0xa2,value);
 if(port==0x60||port==0xa0){
  unsigned channel=port==0xa0;KvGdc *g=&v->gdc[channel];
  if(g->command>=0x70&&g->command<=0x7f){unsigned at=(g->command&15)+g->used;if(at>=16)return -22;g->pram[at]=value;g->used++;if(at==3){unsigned start=g->pram[0]|(unsigned)g->pram[1]<<8;if(channel)v->meta.start=(start&16383)*2;else v->meta.text_start=start&4095;return KV_CHANGED;}return at>3&&value?-38:0;}
  if(!g->need||g->used>=sizeof g->parameters)return -22;
  g->parameters[g->used++]=value;
  if(g->used<g->need)return 0;
  g->used=0;return complete(v,channel,io);
 }
 return -38;
}
int kv_in(KvVideo *v,unsigned port,unsigned ticks,unsigned *value,const KvMemory *io){
 v->meta.io_count++;
 if(port==0xa9){int i=font_index(v);if(i<0)return -38;unsigned char b;if(!io->read(io->context,font_address(v,i),&b,1))return -5;*value=b;return 0;}
 if(port==0x60||port==0xa0){KvGdc *g=&v->gdc[port==0xa0];*value=0x84|(ticks%5120<512?32:0)|(g->reply_count||(g->command&0xe4)==0xa0?1:0);return 0;}
 if(port==0x62||port==0xa2){KvGdc *g=&v->gdc[port==0xa2];if(g->reply_count){*value=g->reply[g->reply_at++];g->reply_count--;return 0;}if((g->command&0xe4)!=0xa0)return -22;unsigned char b[2];if(!io->read(io->context,word_address(g,port==0xa2),b,2))return -5;*value=b[g->phase];if(++g->phase==2){g->phase=0;g->cursor=(g->cursor+1)&65535;}return 0;}
 return -38;
}
unsigned kv_semigraph(unsigned code,unsigned x,unsigned y){return x<8&&y<16?((code>>((x/4)*4+y/4))&1):0;}
