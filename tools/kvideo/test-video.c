#include <assert.h>
#include <string.h>
#include "video.h"
static unsigned char ram[2][KV_BACKING_BYTES];
static unsigned reads,writes;
static int read_mem(void *context,unsigned at,void *p,unsigned n){unsigned id=*(unsigned *)context;if(at>KV_BACKING_BYTES||n>KV_BACKING_BYTES-at)return 0;reads++;memcpy(p,ram[id]+at,n);return 1;}
static int write_mem(void *context,unsigned at,const void *p,unsigned n){unsigned id=*(unsigned *)context;if(at>KV_BACKING_BYTES||n>KV_BACKING_BYTES-at)return 0;writes++;memcpy(ram[id]+at,p,n);return 1;}
static void cursor(KvVideo *v,unsigned at,const KvMemory *io){assert(kv_out(v,0xa2,0x49,io)>=0);assert(!kv_out(v,0xa0,at&255,io));assert(!kv_out(v,0xa0,at>>8,io));assert(kv_out(v,0xa0,0,io)>=0);}
int main(void){
 unsigned ids[]={0,1};KvMemory io[2]={{ids,read_mem,write_mem},{ids+1,read_mem,write_mem}};KvVideo v[2];
 for(unsigned id=0;id<2;id++)kv_reset(v+id);
 assert(v[0].meta.columns==80&&v[0].meta.rows==25&&v[0].meta.width==640&&v[0].meta.height==400);
 for(unsigned id=0;id<2;id++){
  assert(!kv_out(v+id,0xa1,0x21,io+id));assert(!kv_out(v+id,0xa3,0x56,io+id));
  for(unsigned half=0;half<2;half++)for(unsigned y=0;y<16;y++){
   unsigned value=id*93+half*37+y;
   assert(!kv_out(v+id,0xa5,y+(half?0:32),io+id));assert(kv_out(v+id,0xa9,value,io+id)==KV_CHANGED);
   unsigned got;assert(!kv_in(v+id,0xa9,0,&got,io+id)&&got==value);
   assert(ram[id][KV_FONT_BASE+half*16+y]==value);
  }
  assert(v[id].meta.valid[0]==1&&v[id].meta.font_writes==32);
 }
 assert(memcmp(ram[0]+KV_FONT_BASE,ram[1]+KV_FONT_BASE,32));
 assert(kv_out(v,0x68,1,io)&KV_CHANGED);assert(v[0].meta.flags&KV_SEMI);assert(kv_out(v,0x68,0,io)>=0&&!(v[0].meta.flags&KV_SEMI));
 assert(kv_semigraph(1,0,0)&&kv_semigraph(1,3,3)&&!kv_semigraph(1,4,0)&&!kv_semigraph(1,0,4));
 assert(kv_semigraph(128,7,15)&&!kv_semigraph(128,0,15));
 assert(!kv_semigraph(255,8,0)&&!kv_semigraph(255,0,16));
 assert(kv_out(v,0x6a,1,io)>=0);assert(kv_out(v,0xa8,7,io)>=0);assert(kv_out(v,0xaa,3,io)>=0);assert(kv_out(v,0xac,1,io)>=0);assert(kv_out(v,0xae,14,io)>=0);assert(v[0].meta.palette[7][0]==1&&v[0].meta.palette[7][1]==3&&v[0].meta.palette[7][2]==14);
 assert(kv_out(v,0x6a,0,io)>=0);assert(kv_out(v,0xa8,0x32,io)>=0);assert(v[0].meta.palette[3][0]==15&&v[0].meta.palette[3][2]==15&&v[0].meta.palette[7][0]==15&&v[0].meta.palette[7][2]==0);
 assert(kv_out(v,0xa2,13,io)&KV_MODE_CHANGED);assert(v[0].meta.flags&KV_GRAPH);
 cursor(v,0,io);assert(!kv_out(v,0xa2,0x20,io));assert(!kv_out(v,0xa0,0xa5,io));assert(kv_out(v,0xa0,0x5a,io)==KV_CHANGED);assert(ram[0][0xa8000]==0xa5&&ram[0][0xa8001]==0x5a&&ram[1][0xa8000]==0);
 cursor(v,0,io);assert(!kv_out(v,0xa2,0xa0,io));unsigned got;assert(!kv_in(v,0xa2,0,&got,io)&&got==0xa5);assert(!kv_in(v,0xa2,0,&got,io)&&got==0x5a);
 assert(!kv_in(v,0xa0,0,&got,io)&&(got&32));assert(!kv_in(v,0xa0,1024,&got,io)&&!(got&32));
 cursor(v,0x4000,io);assert(!kv_out(v,0xa2,0x20,io));assert(!kv_out(v,0xa0,0xff,io));assert(kv_out(v,0xa0,0x80,io)>=0&&ram[0][0xb0000]==0xff&&ram[0][0xb0001]==0x80);
 /* A whole-plane fill uses bounded 1 KiB memory operations. */
 cursor(v,0x8000,io);assert(!kv_out(v,0xa2,0x4c,io));unsigned char fig[11]={2,255,63};for(unsigned i=0;i<11;i++)assert(kv_out(v,0xa0,fig[i],io)>=0);
 assert(!kv_out(v,0xa2,0x20,io));assert(!kv_out(v,0xa0,0x96,io));writes=reads=0;assert(kv_out(v,0xa0,0x69,io)>=0);assert(writes==32&&reads==0);for(unsigned i=0;i<32768;i++)assert(ram[0][0xb8000+i]==(i&1?0x69:0x96));
 unsigned before=writes;assert(kv_out(v,2,0xff,io)==-38&&writes==before);assert(kv_out(v,0x6a,5,io)==-38&&writes==before);assert(kv_out(v,0xa9,256,io)==-22&&writes==before);
 for(unsigned region=1;region<=7;region++){unsigned address,limit=region==1?KV_FONT_BYTES:region<6?32768:8192;assert(!kv_region(region,limit-1,1,&address));assert(address<KV_BACKING_BYTES);assert(kv_region(region,limit,1,&address)==-22);assert(kv_region(region,0,961,&address)==-22);}

 /* Independent complete pages, CPU access and displayed page are separate. */
 assert(kv_out(v,0xa6,1,io)&KV_MODE_CHANGED);assert(kv_page_plane(v,0,0)==KV_PAGE_BASE&&kv_page_plane(v,0,1)==0xa8000);
 cursor(v,0,io);assert(!kv_out(v,0xa2,0x20,io));assert(!kv_out(v,0xa0,0x12,io));assert(kv_out(v,0xa0,0x34,io)>=0);assert(ram[0][KV_PAGE_BASE]==0x12&&ram[0][0xa8000]==0xa5&&ram[1][KV_PAGE_BASE]==0);
 assert(kv_out(v,0xa4,1,io)&KV_MODE_CHANGED);assert(kv_page_plane(v,3,1)==0xc0000);assert(!kv_in(v,0xa4,0,&got,io)&&got==1);
 unsigned ax=0x0a03;assert(kv_bios(v,&ax,0,0,io)&KV_MODE_CHANGED);assert(v->meta.columns==40&&v->meta.rows==20);ax=0x0b00;assert(!kv_bios(v,&ax,0,0,io)&&(ax&255)==3);
 ax=0x1600;assert(kv_bios(v,&ax,0,0xe12a,io)==KV_CHANGED);for(unsigned i=0;i<4096;i++){assert(ram[0][0xa0000+i*2]==42&&ram[0][0xa2000+i*2]==0xe1);}ax=0x0d00;assert(kv_bios(v,&ax,0,0,io)&KV_CHANGED);ax=0x0c00;assert(kv_bios(v,&ax,0,0,io)&KV_CHANGED);assert(ram[0][0xa0000]==42);
 ax=0x1300;assert(kv_bios(v,&ax,0,242,io)&KV_CURSOR_CHANGED);assert(v->meta.text_cursor==121);assert(kv_bios(v,&ax,0,8192,io)==-22);ax=0x4200;assert(kv_bios(v,&ax,0xd000,0,io)&KV_MODE_CHANGED);assert(v->meta.height==400&&(v->meta.flags&KV_DISPLAY_PAGE));assert(kv_bios(v,&ax,0,0,io)==-38);
 for(unsigned op=0xe4;op<=0xef;op++){if(op>=0xe8&&op<=0xeb)continue;KvPacket p={.width=op|(0xa0u<<8),.port=0xa2,.value=0x12345678};assert(!kv_decode(&p));assert(p.width==(op<0xe8?2:1)&&p.value==0x78&&p.operation==((op&2)?1u:0u)+((op&1)?6u:0u));}
 KvPacket bad={.width=0xef66,.port=0xa2,.value=0x12345678},before_packet=bad;assert(kv_decode(&bad)==-38&&!memcmp(&bad,&before_packet,sizeof bad));bad=(KvPacket){.width=0xef,.port=2};assert(kv_decode(&bad)==-38);
 kv_reset(v);assert(!(v->meta.flags&(KV_ACCESS_PAGE|KV_DISPLAY_PAGE)));
 /* Complete plane storage is unique; text, nested DOS state and private
    * glyphs stay outside both pages. CPU PTEs agree with broker addresses. */
 unsigned char seen[KV_BACKING_BYTES];memset(seen,0,sizeof seen);
 for(unsigned page=0;page<2;page++)for(unsigned plane=0;plane<4;plane++)for(unsigned off=0;off<32768;off++){
  v->meta.flags=page?KV_ACCESS_PAGE:0;unsigned at=kv_page_plane(v,plane,0)+off;
  assert(at<KV_BACKING_BYTES&&!seen[at]);seen[at]=1;
  assert(at<0xa4000||at>=0xa5000);assert(at<KV_FONT_BASE||at>=KV_FONT_BASE+KV_FONT_BYTES);
  assert(at==kv_vram_page(kv_plane(plane)/4096+off/4096,page)*4096+off%4096);
 }
 return 0;
}
