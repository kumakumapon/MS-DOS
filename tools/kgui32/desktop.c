/* Original flat-pointer GUI: composition, cursor, hit testing and window state
 * execute only in CPL3 protected mode. No physical VRAM or I/O mapping. */
#include "scene.h"
#include "../k38632/sdk/k32x.h"
typedef struct {u32 magic,version,frames,events,mx,my,x0,y0,x1,y1,active,drag,presents,last_key,heartbeat,error;} GuiState;
volatile GuiState gui_state __attribute__((used,section(".data.gui_state")))={.magic=0x3147554b,.version=1};
static GuiInfo info;static Scene scene;static u8 font[95*16];
static u8 framebuffer[800*600*2],packed[800*600*2];
static Rect clip;
static u32 color(unsigned n){u32 c=info.colors[n];return info.bpp==1?n:((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31);}
static u32 rgb(unsigned r,unsigned g,unsigned b){return info.bpp==1?16+(r+25)/51*36+(g+25)/51*6+(b+25)/51:(r>>3)<<11|(g>>2)<<5|(b>>3);}
static void box(int x,int y,int w,int h,u32 c){
 int right=x+w,bottom=y+h;
 if(x<clip.x)x=clip.x;
 if(y<clip.y)y=clip.y;
 if(right>clip.x+clip.w)right=clip.x+clip.w;
 if(bottom>clip.y+clip.h)bottom=clip.y+clip.h;
 if(right<=x||bottom<=y)return;
 for(int row=y;row<bottom;row++){
  if(info.bpp==1){u8 *p=framebuffer+row*info.pitch+x;for(int col=x;col<right;col++)*p++=(u8)c;}
  else {u16 *p=(u16 *)(framebuffer+row*info.pitch)+x;for(int col=x;col<right;col++)*p++=(u16)c;}
 }
}
static void text(int x,int y,const char *s,u32 c){
 for(;*s;s++,x+=8){unsigned ch=(u8)*s;if(ch<32||ch>126)ch='?';
  for(int row=0;row<16;row++){u8 bits=font[(ch-32)*16+row];for(int col=0;col<8;col++)if(bits&(128>>col))box(x+col,y+row,1,1,c);}
 }
}
static void number(int x,int y,u32 n,u32 c){char rev[10],out[11];unsigned k=0,i=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);while(k)out[i++]=rev[--k];out[i]=0;text(x,y,out,c);}
static void button(int x,int y,int w,const char *s){
 box(x,y,w,28,color(11));box(x+1,y+1,w-2,26,color(3));box(x+2,y+2,w-4,1,color(7));text(x+(w-8*(int)k32_len(s))/2,y+6,s,color(0));
}
static void window(int id){
 GuiWindow *w=&scene.window[id];if(!w->open||w->hidden)return;
 int x=w->r.x,y=w->r.y,width=w->r.w,height=w->r.h;
 box(x+5,y+5,width,height,rgb(8,16,28));box(x,y,width,height,color(11));box(x+1,y+1,width-2,height-2,color(2));
 box(x+2,y+2,width-4,28,color(scene.active==id?4:9));box(x+3,y+3,width-6,1,color(scene.active==id?13:3));
 text(x+12,y+9,id?"System":"Welcome to KGUI32",color(scene.active==id?7:0));
 box(x+width-47,y+6,18,18,color(3));box(x+width-25,y+6,18,18,color(3));
 text(x+width-43,y+7,"_",color(0));text(x+width-21,y+7,"x",color(0));
 if(!id){
  box(x+16,y+48,46,46,color(4));box(x+24,y+56,12,28,color(7));box(x+38,y+56,12,12,color(6));box(x+38,y+72,12,12,color(3));
  text(x+78,y+48,"Your new desktop",color(0));text(x+78,y+72,"32-bit protected mode",color(10));
  text(x+18,y+116,"Drag either title bar to move.",color(0));
  text(x+18,y+140,"Tab switches windows.",color(0));
  text(x+18,y+164,"Use the taskbar to reopen.",color(0));
  text(x+18,y+196,"F10 or Esc returns to DOS.",color(10));
 }else{
  text(x+16,y+46,"Desktop: private context",color(0));
  text(x+16,y+70,"Application slots: 2",color(0));
  for(int i=0;i<2;i++){
   int yy=y+98+i*24;number(x+16,yy,i+1,color(0));text(x+32,yy,scene.states[i]==1?"Running":scene.states[i]==2?"Waiting":scene.states[i]==3?"Stopped":"Free",color(10));number(x+108,yy,scene.counters[i],color(0));
  }
  text(x+16,y+146,scene.action_result<0?"Slots busy / unavailable":"CPU workers: T=start S=stop",color(10));
  button(x+16,y+168,94,"Test CPU");button(x+124,y+168,94,"Stop");
 }
}
static void cursor(void){
 static const u16 edge[16]={0x8000,0xc000,0xe000,0xf000,0xf800,0xfc00,0xfe00,0xff00,0xff80,0xffc0,0xfc00,0xdc00,0x8e00,0x0600,0x0700,0x0300};
 static const u16 body[16]={0,0,0x4000,0x6000,0x7000,0x7800,0x7c00,0x7e00,0x7f00,0x7c00,0x5800,0x0800,0x0400,0x0200,0x0200,0};
 for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(edge[y]&(0x8000>>x))box(scene.mx+x,scene.my+y,1,1,color(body[y]&(0x8000>>x)?7:0));
}
static int paint(void){
 if(!scene.damage.w)return 0;
 clip=scene.damage;
 for(int y=clip.y;y<clip.y+clip.h;y++)box(clip.x,y,clip.w,1,rgb(18+y*14/info.height,30+y*34/info.height,58+y*40/info.height));
 box(0,0,info.width,32,color(0));box(0,31,info.width,1,color(4));
 box(12,8,12,16,color(6));box(26,8,6,16,color(4));text(44,9,"KGUI32",color(7));text(124,9,"Independent desktop",color(9));
 button(info.width-128,3,120,"Exit to DOS");
 window(1-scene.active);window(scene.active);
 box(0,info.height-32,info.width,32,color(0));box(0,info.height-32,info.width,1,color(4));
 button(6,info.height-29,136,"Welcome");button(152,info.height-29,136,"System");text(312,info.height-20,"PM32 / F10: DOS",color(7));
 cursor();
 unsigned row_bytes=clip.w*info.bpp;
 for(int row=0;row<clip.h;row++)for(unsigned i=0;i<row_bytes;i++)packed[row*row_bytes+i]=framebuffer[(clip.y+row)*info.pitch+clip.x*info.bpp+i];
 GuiPresent p={(u32)packed,clip.x,clip.y,clip.w,clip.h,row_bytes};
 int e=k32_call(GUI_PRESENT,(u32)&p,sizeof p);if(e)return e;
 scene.damage=(Rect){0};gui_state.presents++;gui_state.frames++;return 0;
}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int negative(void){
 GuiPresent p={(u32)packed,0,0,~0u,1,640};
 if(k32_call(GUI_INFO,0x10000,sizeof info)!=-14)return 1;
 if(k32_call(GUI_FONT,0x800000,sizeof font)!=-14)return 2;
 if(k32_call(GUI_PRESENT,(u32)&p,sizeof p)!=-22)return 3;
 p=(GuiPresent){0x10000,0,0,16,16,16*info.bpp};
 if(k32_call(GUI_PRESENT,(u32)&p,sizeof p)!=-14)return 4;
 if(k32_call(GUI_EVENT,0x10000,sizeof(GuiEvent))!=-14)return 5;
 return 0;
}
int main(int argc,char **argv){
 if((k32_query()&23)!=23)return 1;
 int e=k32_call(GUI_INFO,(u32)&info,sizeof info);if(e)return 2;
 if(info.magic!=0x3149474b||info.version!=1||info.width>800||info.height>600||(info.bpp!=1&&info.bpp!=2)||info.pitch!=info.width*info.bpp)return 3;
 if(k32_call(GUI_FONT,(u32)font,sizeof font))return 4;
 scene_init(&scene,info.width,info.height);gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;
 int once=argc>1&&equal(argv[1],"once"),proof=argc>1&&equal(argv[1],"proof");
 if(proof)scene.action_result=k32_call(GUI_PROOF,1,0);
 if(argc>1&&equal(argv[1],"badapi")){int bad=negative();if(bad){gui_state.error=100+bad;return 5;}once=1;}
 if(paint()){gui_state.error=6;return 6;}
 if(argc>1&&equal(argv[1],"fault"))*(volatile u32 *)0x10000=1;
 if(argc>1&&equal(argv[1],"spin"))for(;;)gui_state.heartbeat++;
 for(;;){
  GuiEvent event;int got=k32_call(GUI_EVENT,(u32)&event,sizeof event);
  if(got<0){gui_state.error=7;return 7;}
  if(!got){k32_wait(K32_WAIT_TIME,16);continue;}
  gui_state.events++;gui_state.heartbeat++;if(event.type==GUI_KEY)gui_state.last_key=event.key;
  int action=scene_event(&scene,&event);
  if(action==ACT_EXIT)return 0;
  if(action==ACT_START||action==ACT_STOP){scene.action_result=k32_call(GUI_PROOF,action==ACT_START?1:2,0);scene_damage(&scene,scene.window[1].r);}
  gui_state.mx=scene.mx;gui_state.my=scene.my;gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;gui_state.active=scene.active;gui_state.drag=scene.drag;
  e=paint();if(e){gui_state.error=8;return 8;}
  if((once&&gui_state.events>=20)||(proof&&gui_state.events>=64))return 0;
 }
}
