/* Actual CPL3 boundary requests, peer-token forgery and a clipped bitmap. */
#include "stats.h"
volatile KgStats bitmap_stats __attribute__((section(".data.gui_stats")))={.magic=0x3142474b,.version=1};
static KgRequest r;static unsigned token,surface;
static int call(unsigned op){r.operation=op;return k32_call(13,(u32)&r,sizeof r);}
#define CHECK(v) do {bitmap_stats.value++;if(!(v)){bitmap_stats.error=bitmap_stats.value;return 1;}}while(0)
static unsigned number(const char *p){unsigned n=0;while(*p>='0'&&*p<='9')n=n*10+*p++-'0';return n;}
static int waited(void){for(unsigned n=0;n<2048;n++){r.width=surface;int e=call(KG_SURFACE_STATUS);if(e!=-11)return e;k32_wait(K32_WAIT_TIME,16);}return -110;}
int main(int argc,char **argv){
 r=(KgRequest){.version=1,.size=sizeof r,.width=328,.height=160,.title="Bitmap boundary"};
 int t=call(KG_OPEN);CHECK(t>0);token=t;r.token=token;bitmap_stats.token=token;
 CHECK(k32_call(13,0x10000,sizeof r)==-14);CHECK(k32_call(13,0x800000,sizeof r)==-14);CHECK(k32_call(13,0xbff800,sizeof r)==-14);
 r.width=0;r.height=2;CHECK(call(KG_SURFACE_CREATE)==-22);r.width=385;CHECK(call(KG_SURFACE_CREATE)==-22);r.width=3;r.height=257;CHECK(call(KG_SURFACE_CREATE)==-22);
 r.height=2;t=call(KG_SURFACE_CREATE);CHECK(t>0);surface=t;bitmap_stats.reserved[0]=surface;CHECK(!waited());
 r.width=argc>1?number(argv[1]):surface+1;r.height=0;r.count=2;CHECK(call(KG_SURFACE_UPLOAD)==-116);
 r.width=surface;r.count=0;CHECK(call(KG_SURFACE_UPLOAD)==-22);r.count=1;CHECK(call(KG_SURFACE_UPLOAD)==-22);r.count=3042;CHECK(call(KG_SURFACE_UPLOAD)==-22);
 r.count=2;r.height=~0u;CHECK(call(KG_SURFACE_UPLOAD)==-22);r.height=1;CHECK(call(KG_SURFACE_UPLOAD)==-22);r.height=0;
 r.slot=0;r.generation=~0u;r.token=argc>2?number(argv[2]):token+1;CHECK(call(KG_SURFACE_UPLOAD)==-116);r.token=token;
 ((u16 *)r.draw)[0]=0x001f;CHECK(!call(KG_SURFACE_UPLOAD));CHECK(!waited());
 r.count=1;r.draw[0]=(KgDraw){.op=KG_BITMAP,.id=surface,.w=3,.h=2};CHECK(call(KG_FRAME)==-11);
 r.width=surface;r.height=2;r.count=10;const u16 rest[]={0x07e0,0xf800,0xffe0,0xffff,0xf81f};for(unsigned i=0;i<5;i++)((u16 *)r.draw)[i]=rest[i];CHECK(!call(KG_SURFACE_UPLOAD));CHECK(!waited());
 r.count=1;r.draw[0]=(KgDraw){.op=KG_BITMAP,.id=surface+1,.w=3,.h=2};CHECK(call(KG_FRAME)==-116);
 r.draw[0].id=surface;r.draw[0].color=3;CHECK(call(KG_FRAME)==-22);r.draw[0].color=0;r.draw[0].w=0x7fffffff;CHECK(call(KG_FRAME)==-22);
 r.count=2;r.draw[0]=(KgDraw){.op=KG_RECT,.w=344,.h=168,.color=0x334455};r.draw[1]=(KgDraw){.op=KG_BITMAP,.x=-2,.y=-1,.w=3,.h=2,.id=surface};CHECK(!call(KG_FRAME));bitmap_stats.frames=1;
 for(;;){int e=call(KG_POLL);if(e<0)return 2;bitmap_stats.heartbeat++;if(e&&r.event.type==KG_KEY&&r.event.key=='q')break;k32_wait(K32_WAIT_TIME,50);}
 r.width=surface;CHECK(!call(KG_SURFACE_DESTROY));CHECK(!waited());CHECK(call(KG_SURFACE_DESTROY)==-116);
 r.count=1;r.draw[0]=(KgDraw){.op=KG_BITMAP,.w=1,.h=1,.id=surface};CHECK(call(KG_FRAME)==-116);CHECK(!call(KG_CLOSE));CHECK(call(KG_SURFACE_STATUS)==-116);
 return 0;
}
