/* Actual user-mode negative requests; an optional argv names a real peer token. */
#include "stats.h"
volatile KgStats boundary_stats __attribute__((section(".data.gui_stats")))={.magic=0x3242474b,.version=1};
static KgRequest request;
static int call(void){return k32_call(13,(u32)&request,sizeof request);}
#define CHECK(test) do {boundary_stats.value++;if(!(test)){boundary_stats.error=boundary_stats.value;return 1;}}while(0)
int main(int argc,char **argv){
 request=(KgRequest){.version=1,.size=sizeof request,.operation=KG_OPEN,.width=328,.height=160,.title="SDK boundary checks"};
 int token=call();if(token<0){boundary_stats.error=-token;return 2;}boundary_stats.token=token;
 CHECK(k32_call(13,0x10000,sizeof request)==-14);
 CHECK(k32_call(13,0x800000,sizeof request)==-14);
 CHECK(k32_call(13,0xbff800,sizeof request)==-14);
 CHECK(k32_call(13,(u32)&request,sizeof request-1)==-22);
 request.operation=KG_FRAME;request.count=1;request.draw[0]=(KgDraw){.op=KG_RECT,.w=1,.h=1};
 request.version=2;CHECK(call()==-22);request.version=1;
 request.operation=99;CHECK(call()==-22);request.operation=KG_FRAME;
 request.count=41;CHECK(call()==-22);request.count=1;
 request.draw[0].x=-1;CHECK(call()==-22);request.draw[0].x=0;
 request.draw[0].w=0x7fffffff;CHECK(call()==-22);request.draw[0].w=1;
 request.draw[0].color=0x1000000;CHECK(call()==-22);request.draw[0].color=0;
 unsigned peer=token+1;if(argc>1){peer=0;for(const char *s=argv[1];*s>='0'&&*s<='9';s++)peer=peer*10+*s-'0';}
 request.slot^=1;request.generation=~0u;request.token=peer;CHECK(call()==-116);
 request.token=token;request.count=0;CHECK(call()==0); /* forged slot cannot select a peer */
 request.operation=KG_CLOSE;CHECK(call()==0);
 request.operation=KG_POLL;CHECK(call()==-116);
 return 0;
}
