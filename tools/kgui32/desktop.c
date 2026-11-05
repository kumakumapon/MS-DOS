/* Original flat-pointer GUI: composition, cursor, hit testing and window state
 * execute only in CPL3 protected mode. No physical VRAM or I/O mapping. */
#include "scene.h"
#include "../k38632/sdk/k32x.h"
typedef struct {u32 magic,version,frames,events,mx,my,x0,y0,x1,y1,active,drag,presents,last_key,heartbeat,error;} GuiState;
volatile GuiState gui_state __attribute__((used,section(".data.gui_state")))={.magic=0x3147554b,.version=1};
static GuiInfo info;static Scene scene;static u8 font[95*16];
static u8 framebuffer[800*600*2],packed[800*600*2];
static Rect clip;
static GuiSessionView views[2];static int errors[2],editing[2],lengths[2];
static char paths[2][128];static unsigned following[2];
static void follow_cursor(unsigned);
static int session_command(unsigned slot,unsigned operation,unsigned key){
 GuiSessionCommand c={.version=1,.size=sizeof c,.slot=slot,.generation=operation==GS_CREATE?0:views[slot].generation,.operation=operation,.key=key};
 for(unsigned i=0;i<128;i++){c.path[i]=paths[slot][i];if(!c.path[i])break;}
 return k32_call(GUI_SESSION,(u32)&c,sizeof c);
}
static void refresh(void){
 for(unsigned id=0;id<2;id++){
  GuiSessionView v={.version=1,.size=sizeof v,.slot=id};
  if(k32_call(GUI_SNAPSHOT,(u32)&v,sizeof v))continue;
  GuiWindow *w=&scene.window[id+2];
  if(w->open&&!w->hidden){
   int changed=v.generation!=views[id].generation||v.state!=views[id].state||v.owned!=views[id].owned;
   for(int i=0;i<16;i++)if(v.program[i]!=views[id].program[i])changed=1;
   if(changed)scene_damage(&scene,w->r);
   int cols=(w->r.w-16)/8,rows=(w->r.h-104)/16;
   for(int row=0;row<rows;row++){
    int r=row+scene.scroll_y[id];if(r>=25)break;
    int dirty=(v.row==(u32)r||views[id].row==(u32)r)&&(v.row!=views[id].row||v.column!=views[id].column);
    for(int col=0;col<cols&&col+scene.scroll_x[id]<80;col++){
     int at=r*80+col+scene.scroll_x[id];if(v.screen[at]!=views[id].screen[at])dirty=1;
    }
    if(dirty)scene_damage(&scene,(Rect){w->r.x+8,w->r.y+70+row*16,cols*8,16});
   }
  }
  views[id]=v;if(following[id]){follow_cursor(id);following[id]--;}
 }
}
static void follow_cursor(unsigned id){
 GuiWindow *w=&scene.window[id+2];int cols=(w->r.w-16)/8,rows=(w->r.h-104)/16;
 int x=scene.scroll_x[id],y=scene.scroll_y[id];
 if((int)views[id].column<x)x=views[id].column;
 if((int)views[id].column>=x+cols)x=(int)views[id].column-cols+1;
 if((int)views[id].row<y)y=views[id].row;
 if((int)views[id].row>=y+rows)y=(int)views[id].row-rows+1;
 if(x!=scene.scroll_x[id]||y!=scene.scroll_y[id]){scene.scroll_x[id]=x;scene.scroll_y[id]=y;scene_damage(&scene,w->r);}
}
static void begin_run(unsigned id){
 editing[id]=1;lengths[id]=0;paths[id][0]=0;
 GuiWindow *w=&scene.window[id+2];scene_damage(&scene,(Rect){w->r.x+8,w->r.y+70,w->r.w-16,72});
}
static void action(int act){
 unsigned id=scene.slot;
 if(act==ACT_START||act==ACT_STOP){scene.action_result=k32_call(GUI_PROOF,act==ACT_START?1:2,0);scene_damage(&scene,scene.window[1].r);refresh();return;}
 if(act<ACT_NEW)return;
 if(act==ACT_RUN){begin_run(id);return;}
 int was_editing=editing[id],old_error=errors[id];
 if(act==ACT_KEY&&editing[id]){
  unsigned k=scene.key;int n=lengths[id];
  if(k==27)editing[id]=0;
  else if(k==8&&n)paths[id][--n]=0;
  else if(k>=32&&k<=126&&n<127){paths[id][n++]=k;paths[id][n]=0;}
  else if(k==13&&n){errors[id]=session_command(id,GS_CREATE,0);if(!errors[id])editing[id]=0;}
  lengths[id]=n;
 }else if(act==ACT_NEW){
  if(!views[id].owned&&!views[id].state){
   const char *p="KSH.EXE";for(int i=0;i<8;i++)paths[id][i]=p[i];
   errors[id]=session_command(id,GS_CREATE,0);
  }
 }else {
  unsigned op=act==ACT_KEY?GS_KEY:act==ACT_END?GS_END:act==ACT_PAUSE?GS_PAUSE:act==ACT_RESUME?GS_RESUME:GS_RESTART;
  errors[id]=session_command(id,op,scene.key);if(act==ACT_KEY)following[id]=3;
  if(act==ACT_RESTART&&!errors[id])scene.scroll_x[id]=scene.scroll_y[id]=0;
 }
 refresh();if(act==ACT_KEY&&!editing[id])follow_cursor(id);
 GuiWindow *w=&scene.window[id+2];
 if(act==ACT_KEY&&!was_editing&&!editing[id]){
  if(old_error!=errors[id])scene_damage(&scene,(Rect){w->r.x,w->r.y+w->r.h-32,w->r.w,32});
 }else if(act==ACT_KEY&&editing[id])scene_damage(&scene,(Rect){w->r.x+8,w->r.y+70,w->r.w-16,72});
 else scene_damage(&scene,w->r);
}
static void input_focus(void){
 unsigned id=scene.active>=2?(unsigned)scene.active-2:0;
 unsigned on=0;
 if(scene.active>=2&&scene.window[scene.active].open&&!scene.window[scene.active].hidden)on=editing[id]?2:views[id].owned?1:0;
 session_command(id,GS_FOCUS,on);
}

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
  unsigned n=right-x;
  if(info.bpp==1){u8 *p=framebuffer+row*info.pitch+x;
   if(n==1)*p=(u8)c;else __asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(c):"memory","cc");
  }else {u16 *p=(u16 *)(framebuffer+row*info.pitch)+x;
   if(n==1)*p=(u16)c;else __asm__ volatile("cld; rep stosw":"+D"(p),"+c"(n):"a"(c):"memory","cc");
  }
 }
}
static void text(int x,int y,const char *s,u32 c){
 for(;*s;s++,x+=8){unsigned ch=(u8)*s;if(ch<32||ch>126)ch='?';
  if(x+8<=clip.x||x>=clip.x+clip.w||y+16<=clip.y||y>=clip.y+clip.h)continue;
  for(int row=0;row<16;row++){u8 bits=font[(ch-32)*16+row];for(int col=0;col<8;col++)if(bits&(128>>col))box(x+col,y+row,1,1,c);}
 }
}
static void number(int x,int y,u32 n,u32 c){char rev[10],out[11];unsigned k=0,i=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);while(k)out[i++]=rev[--k];out[i]=0;text(x,y,out,c);}
static void button(int x,int y,int w,const char *s){
 box(x,y,w,28,color(11));box(x+1,y+1,w-2,26,color(3));box(x+2,y+2,w-4,1,color(7));text(x+(w-8*(int)k32_len(s))/2,y+6,s,color(0));
}
static void terminal(unsigned id,int x,int y,int width,int height){
 GuiSessionView *v=&views[id];
 const char *names[]={"Run","Stop","Resume","End","Again"};
 for(int i=0;i<5;i++)button(x+8+i*64,y+36,60,names[i]);
 int cols=(width-16)/8,rows=(height-104)/16;
 box(x+8,y+70,cols*8,rows*16,color(0));
 Rect before=clip;Rect area={x+8,y+70,cols*8,rows*16};
 int right=clip.x+clip.w,bottom=clip.y+clip.h;
 if(clip.x<area.x)clip.x=area.x;
 if(clip.y<area.y)clip.y=area.y;
 if(right>area.x+area.w)right=area.x+area.w;
 if(bottom>area.y+area.h)bottom=area.y+area.h;
 clip.w=right>clip.x?right-clip.x:0;clip.h=bottom>clip.y?bottom-clip.y:0;
 for(int row=0;row<rows;row++){
  int rr=row+scene.scroll_y[id];if(rr>=25)break;
  char line[81];int col=0;
  for(;col<cols&&col+scene.scroll_x[id]<80;col++)line[col]=v->screen[rr*80+col+scene.scroll_x[id]];
  line[col]=0;text(x+8,y+70+row*16,line,color(7));
 }
 if(v->owned&&(v->state==1||v->state==2)&&scene.active==(int)id+2)
  box(x+8+((int)v->column-scene.scroll_x[id])*8,y+84+((int)v->row-scene.scroll_y[id])*16,8,2,color(6));
 clip=before;
 int yy=y+height-30;const char *arrows[]={"Left","Right","Up","Down"};
 for(int i=0;i<4;i++)button(x+8+i*48,yy,44,arrows[i]);
 text(x+208,yy+6,errors[id]?"Error:":!v->owned&&v->state?"Other owner":v->state==1?"Running":v->state==2?"Waiting":v->state==3?"Stopped":v->state==4?"Faulted":"Ended",color(10));
 if(errors[id])number(x+264,yy+6,-errors[id],color(10));
 text(x+width-18,y+height-18,"/",color(10));
 if(editing[id]){
  box(x+8,y+70,width-16,72,color(3));text(x+16,y+78,"Run (End current first):",color(0));
  int n=lengths[id],visible=(width-40)/8,start=n>visible?n-visible:0;
  text(x+16,y+102,paths[id]+start,color(0));box(x+16+(n-start)*8,y+116,8,2,color(4));
  text(x+16,y+126,"Enter: start   Esc: cancel",color(10));
 }
}
static void window(int id){
 GuiWindow *w=&scene.window[id];if(!w->open||w->hidden)return;
 int x=w->r.x,y=w->r.y,width=w->r.w,height=w->r.h;
 if(x+width+6<=clip.x||x>=clip.x+clip.w||y+height+6<=clip.y||y>=clip.y+clip.h)return;
 box(x+5,y+5,width,height,rgb(8,16,28));box(x,y,width,height,color(11));box(x+1,y+1,width-2,height-2,color(2));
 box(x+2,y+2,width-4,28,color(scene.active==id?4:9));box(x+3,y+3,width-6,1,color(scene.active==id?13:3));
 text(x+12,y+9,id>=2?(id==2?"DOS 1":"DOS 2"):id?"System":"Welcome to KGUI32",color(scene.active==id?7:0));
 box(x+width-47,y+6,18,18,color(3));box(x+width-25,y+6,18,18,color(3));
 text(x+width-43,y+7,"_",color(0));text(x+width-21,y+7,"x",color(0));
 if(id>=2){
  text(x+72,y+9,views[id-2].program,color(7));terminal(id-2,x,y,width,height);
 }else if(!id){
  box(x+16,y+48,46,46,color(4));box(x+24,y+56,12,28,color(7));box(x+38,y+56,12,12,color(6));box(x+38,y+72,12,12,color(3));
  text(x+78,y+48,"Your new desktop",color(0));text(x+78,y+72,"32-bit protected mode",color(10));
  text(x+18,y+116,"Drag either title bar to move.",color(0));
  text(x+18,y+140,"Tab switches windows.",color(0));
  text(x+18,y+164,"Use the taskbar to reopen.",color(0));
  text(x+18,y+196,"F10: DOS.  New DOS: top bar.",color(10));
 }else{
  text(x+16,y+46,"Desktop: private context",color(0));
  text(x+16,y+70,"Application slots: 2",color(0));
  for(int i=0;i<2;i++){
   int yy=y+98+i*24;number(x+16,yy,i+1,color(0));text(x+32,yy,scene.states[i]==1?"Running":scene.states[i]==2?"Waiting":scene.states[i]==3?"Stopped":scene.states[i]==4?"Faulted":"Free",color(10));number(x+108,yy,scene.counters[i],color(0));
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
 box(12,8,12,16,color(6));box(26,8,6,16,color(4));text(44,9,"KGUI32",color(7));text(124,9,"32-bit",color(9));
 button(info.width-312,3,84,"New DOS 1");button(info.width-220,3,84,"New DOS 2");
 button(info.width-128,3,120,"Exit to DOS");
 for(int i=0;i<4;i++)window(scene.order[i]);
 box(0,info.height-32,info.width,32,color(0));box(0,info.height-32,info.width,1,color(4));
 button(6,info.height-29,136,"Welcome");button(152,info.height-29,136,"System");
 for(int i=0;i<2;i++)if(scene.window[i+2].open)button(300+i*146,info.height-29,136,i?"DOS 2":"DOS 1");
 cursor();
 unsigned row_bytes=clip.w*info.bpp;
 for(int row=0;row<clip.h;row++){
  const u8 *source=framebuffer+(clip.y+row)*info.pitch+clip.x*info.bpp;u8 *to=packed+row*row_bytes;unsigned n=row_bytes;
  __asm__ volatile("cld; rep movsb":"+S"(source),"+D"(to),"+c"(n)::"memory","cc");
 }
 GuiPresent p={(u32)packed,clip.x,clip.y,clip.w,clip.h,row_bytes};
 int e=k32_call(GUI_PRESENT,(u32)&p,sizeof p);if(e)return e;
 scene.damage=(Rect){0};gui_state.presents++;gui_state.frames++;return 0;
}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int session_negative(void){
 static const u32 readonly_header[4]={1,sizeof(GuiSessionView),0,0};
 GuiSessionCommand c={.version=1,.size=sizeof c,.operation=GS_END};
 GuiSessionView v={.version=1,.size=sizeof v};
 if(k32_call(GUI_SESSION,0x10000,sizeof c)!=-14)return 1;
 if(k32_call(GUI_SNAPSHOT,0x10000,sizeof v)!=-14)return 2;
 if(k32_call(GUI_SNAPSHOT,(u32)readonly_header,sizeof v)!=-14)return 11;
 if(k32_call(GUI_SNAPSHOT,0x800000,sizeof v)!=-22)return 3; /* RO ELF header is not a valid versioned request. */
 if(k32_call(GUI_SESSION,(u32)&c,sizeof c-1)!=-22)return 4;
 c.slot=2;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-22)return 5;c.slot=0;
 c.operation=7;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-22)return 6;
 for(unsigned id=0;id<2;id++){
  c.slot=id;c.operation=GS_END;c.generation=views[id].generation+1;
  if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-116)return 7;
  c.generation=views[id].generation;c.key=1;
  /* A live DOS parent owns both workers in the KGPROOF scenario. */
  if(views[id].state&&!views[id].owned){
   for(unsigned op=GS_END;op<=GS_RESTART;op++){c.operation=op;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-1)return 8;}
   c.operation=GS_FOCUS;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-1)return 9;
   c.operation=GS_CREATE;c.generation=0;c.path[0]='X';c.path[1]=0;
   if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-16)return 10;
  }
 }
 return 0;
}
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
 refresh();
 int once=argc>1&&equal(argv[1],"once"),proof=argc>1&&equal(argv[1],"proof");
 if(argc>1&&equal(argv[1],"sessionapi")){int bad=session_negative();gui_state.error=bad?200+bad:0;return bad?5:0;}
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
  int act;
  if(event.type==GUI_KEY&&scene.active>=2&&editing[scene.active-2]&&event.key!=9){
   scene.slot=scene.active-2;scene.key=event.key;act=ACT_KEY;
  }else act=scene_event(&scene,&event);
  if(act==ACT_EXIT)return 0;
  action(act);if(event.type==GUI_TIMER)refresh();input_focus();
  gui_state.mx=scene.mx;gui_state.my=scene.my;gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;gui_state.active=scene.active;gui_state.drag=scene.drag;
  e=paint();if(e){gui_state.error=8;return 8;}
  if((once&&gui_state.events>=20)||(proof&&gui_state.events>=16))return 0;
 }
}
