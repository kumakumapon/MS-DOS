#include <assert.h>
#include <string.h>
#include "gui.h"
#include "../window/k386_client.h"
u8 loader_buffer[4096];unsigned owned[4]={1},manager=1,session_focus;u32 owned_generation[4]={7};
static GuiVideoRead request;static unsigned writes,kind,revision=9;
extern int gui_video_read(u32,unsigned);
int gui_copy(u32 p,void *v,unsigned n,int write){if(p!=1&&p!=2)return -14;if(write){assert(p==2&&n==request.bytes);writes++;return 0;}assert(p==1&&n==sizeof request);memcpy(v,&request,n);return 0;}
int gui_output_range(u32 p,unsigned n){return p==2&&n<=KV_READ_BYTES;}
unsigned k386_capacity(void){return 4;}
int k386_usage(unsigned id,K386Usage *u){(void)id;u->generation=7;return 1;}
unsigned k386_word(unsigned id,unsigned at){(void)id;assert(at==6);return revision;}
unsigned k386_kind(unsigned id){(void)id;return kind;}
int k386_video(unsigned id,unsigned region,unsigned offset,unsigned n,KvRead *r){assert(id==0&&region==request.region&&offset==request.offset&&n==request.bytes);r->packet.generation=7;return 0;}
static int run(void){return gui_video_read(1,sizeof request);}
int main(void){
 request=(GuiVideoRead){.version=1,.size=sizeof request,.slot=0,.generation=7,.region=0,.data=2,.bytes=128,.revision=9};
 assert(!run()&&writes==1);assert(gui_video_read(99,sizeof request)==-14);assert(gui_video_read(1,sizeof request-1)==-22);
 request.reserved[0]=1;assert(run()==-22);request.reserved[0]=0;
 request.slot=4;assert(run()==-22);request.slot=0;
 request.generation=6;assert(run()==-116);request.generation=7;
 request.revision=8;assert(run()==-116);request.revision=9;
 owned[0]=0;assert(run()==-1);owned[0]=1;owned_generation[0]=8;assert(run()==-1);owned_generation[0]=7;
 kind=1;assert(run()==-38);kind=0;manager=0;assert(run()==-38);manager=1;
 request.data=99;assert(run()==-14);request.data=2;
 request.offset=128;assert(run()==-22);request.offset=0;
 request.bytes=KV_READ_BYTES+1;assert(run()==-22);request.bytes=128;
 for(unsigned r=1;r<=7;r++){request.region=r;assert(!run());}
 request.region=8;assert(run()==-22);request.region=1;request.offset=KV_FONT_BYTES-2;request.bytes=4;assert(run()==-22);
 request.offset=KV_FONT_BYTES-4;assert(!run());assert(writes==9);
}
