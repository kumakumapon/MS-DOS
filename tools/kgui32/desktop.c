/* Original flat-pointer GUI: composition, cursor, hit testing and window state
 * execute only in CPL3 protected mode. No physical VRAM or I/O mapping. */
#include "scene.h"
#include "sdk/i18n.h"
unsigned ku_language;
#include "sdk/protocol.h"
#include "sdk/text.h"
#include "surface.h"
#include "../k386/text.h"
#include "../k38632/sdk/k32x.h"
#include "ui-desktop.h"
typedef struct {u32 magic,version,frames,events,mx,my,x0,y0,x1,y1,active,drag,presents,last_key,heartbeat,error;
 u32 terminal_magic,terminal0,terminal1,terminal_phase,terminal_valid;
 u32 terminal_geometry[2][6];
 u32 capacity,terminal2,terminal3,extra_geometry[2][6];
 u32 bitmap_magic,bitmap_meta,bitmap_pixels,bitmap_stride,bitmap_frames;
} GuiState;
volatile GuiState gui_state __attribute__((used,section(".data.gui_state")))={.magic=0x3147554b,.version=1,.terminal_magic=0x3254474b};
static GuiInfo info;static Scene scene;static u8 font[95*16];
static struct {u32 code;u8 bitmap[32];} glyph_cache[128];
static unsigned glyph_slot;
static u8 framebuffer[800*600*2],packed[800*600*2];
static Rect clip;
static GuiSessionView views[4];static int errors[4],editing[4],lengths[4];
static GuiTerminalView terminals[4],terminal_stage;
static unsigned terminal_valid[4],terminal_phase;static u32 terminal_tick;
static char paths[4][128];static unsigned following[4];
static KgRequest clients[4],client_io;
static KgImage images[4];static u16 image_pixels[4][KG_SURFACE_PIXELS*2];
static unsigned exit_pending;
static u32 close_generation[4],close_token[4],close_cancel[4];
static unsigned capture,capture_generation,capture_token;
static void refresh_graphical(unsigned);
static int client_event(unsigned,const KgEvent *);
static void follow_cursor(unsigned);
static void terminal_report(void){
 gui_state.terminal0=(u32)&terminals[0];gui_state.terminal1=(u32)&terminals[1];gui_state.terminal_valid=terminal_valid[0]|terminal_valid[1]<<1|terminal_valid[2]<<2|terminal_valid[3]<<3;gui_state.capacity=scene.slots;gui_state.terminal2=(u32)&terminals[2];gui_state.terminal3=(u32)&terminals[3];gui_state.terminal_phase=terminal_phase;
 for(unsigned id=0;id<(unsigned)scene.slots;id++){Rect r=scene.window[id+2].r;u32 *g=(u32 *)(id<2?gui_state.terminal_geometry[id]:gui_state.extra_geometry[id-2]);g[0]=r.x;g[1]=r.y;g[2]=r.w;g[3]=r.h;g[4]=scene.scroll_x[id];g[5]=scene.scroll_y[id];}
}
static int session_command(unsigned slot,unsigned operation,unsigned key){
 GuiSessionCommand c={.version=1,.size=sizeof c,.slot=slot,.generation=operation==GS_CREATE?0:views[slot].generation,.operation=operation,.key=key};
 for(unsigned i=0;i<128;i++){c.path[i]=paths[slot][i];if(!c.path[i])break;}
 return k32_call(GUI_SESSION,(u32)&c,sizeof c);
}
static void refresh(void){
 for(unsigned id=0;id<(unsigned)scene.slots;id++){
  GuiSessionView v={.version=1,.size=sizeof v,.slot=id};
  if(k32_call(GUI_SNAPSHOT,(u32)&v,sizeof v))continue;
  GuiWindow *w=&scene.window[id+2];
  if(v.kind||!v.owned){terminal_valid[id]=0;}
  else if(!terminal_valid[id]||terminals[id].generation!=v.generation||terminals[id].revision!=v.text_revision||terminals[id].state!=v.state){
   terminal_stage=(GuiTerminalView){.version=2,.size=sizeof terminal_stage,.slot=id,.generation=v.generation};
   if(!k32_call(GUI_SNAPSHOT,(u32)&terminal_stage,sizeof terminal_stage)){
    if(w->open&&!w->hidden)for(unsigned r=0;r<25;r++){int changed=!terminal_valid[id]||terminals[id].generation!=v.generation;
     for(unsigned c=0;c<80&&!changed;c++){unsigned at=r*80+c;if(terminal_stage.codes[at]!=terminals[id].codes[at]||terminal_stage.attributes[at]!=terminals[id].attributes[at])changed=1;}
     if(changed)scene_window_damage(&scene,id+2,(Rect){w->r.x+8,w->r.y+70+((int)r-scene.scroll_y[id])*16,(w->r.w-16)/8*8,16});
    }
    terminals[id]=terminal_stage;terminal_valid[id]=1;
   }
  }
  if(w->open&&!w->hidden){
   int changed=v.generation!=views[id].generation||v.state!=views[id].state||v.owned!=views[id].owned;
   for(int i=0;i<16;i++)if(v.program[i]!=views[id].program[i])changed=1;
   if(changed)scene_window_damage(&scene,id+2,w->r);
   int cols=(w->r.w-16)/8,rows=(w->r.h-104)/16;
   for(int row=0;row<rows;row++){
    int r=row+scene.scroll_y[id];if(r>=25)break;
    int dirty=(v.row==(u32)r||views[id].row==(u32)r)&&(v.row!=views[id].row||v.column!=views[id].column);
    for(int col=0;col<cols&&col+scene.scroll_x[id]<80;col++){
     int at=r*80+col+scene.scroll_x[id];if(v.screen[at]!=views[id].screen[at])dirty=1;
    }
    if(dirty)scene_window_damage(&scene,id+2,(Rect){w->r.x+8,w->r.y+70+row*16,cols*8,16});
   }
  }
  views[id]=v;refresh_graphical(id);if(following[id]){follow_cursor(id);following[id]--;}
 }
 terminal_report();
}
static int client_event(unsigned id,const KgEvent *e){
 client_io=(KgRequest){.version=1,.size=sizeof client_io,.slot=id,.generation=views[id].generation,.token=clients[id].token,.event=*e};
 return k32_call(GUI_CLIENT_EVENT,(u32)&client_io,sizeof client_io);
}
static void refresh_graphical(unsigned id){
 client_io=(KgRequest){.version=1,.size=sizeof client_io,.slot=id};
 if(k32_call(GUI_CLIENT_VIEW,(u32)&client_io,sizeof client_io))return;
 if(kg_surface_operation(client_io.operation)&&client_io.token&&views[id].owned&&client_io.generation==views[id].generation){
  int e=kg_surface_receive(&images[id],image_pixels[id],&client_io,views[id].generation,client_io.token);
  if(!e){GuiSurfaceAck ack={1,sizeof ack,id,client_io.generation,client_io.token,client_io.revision};e=k32_call(GUI_SURFACE_ACK,(u32)&ack,sizeof ack);}
  if(e)errors[id]=e;
  else if(client_io.operation==KG_SURFACE_DESTROY||(images[id].valid&&images[id].uploaded==images[id].width*images[id].height*2))scene_window_damage(&scene,id+2,scene.window[id+2].r);
  return;
 }
 if(!client_io.token||client_io.generation!=views[id].generation||!views[id].owned)images[id]=(KgImage){0};
 if(close_token[id]){
  if(close_generation[id]!=views[id].generation||close_generation[id]!=client_io.generation||close_cancel[id]!=client_io.event.key||(client_io.token&&client_io.token!=close_token[id]))close_token[id]=0;
  else if(!client_io.token){scene_close(&scene,id+2);close_token[id]=0;capture=0;}
 }
 unsigned yes=views[id].owned&&views[id].kind==1&&client_io.generation==views[id].generation&&client_io.revision;
 scene.graphical[id]=yes;
 if(yes&&client_io.token&&(clients[id].token!=client_io.token||clients[id].generation!=client_io.generation)){
  GuiWindow *w=&scene.window[id+2];scene_damage(&scene,(Rect){w->r.x,w->r.y,w->r.w+6,w->r.h+6});
  w->r.w=client_io.width+16;w->r.h=client_io.height+104;
  w->r.x=id?scene.width-w->r.w-16:16;w->r.y=id?scene.height-w->r.h-40:40;
  if(w->r.y<40)w->r.y=40;
  scene_focus(&scene,id+2);scene_damage(&scene,w->r);
 }
 if(exit_pending&&clients[id].event.key!=client_io.event.key)exit_pending=0;
 if(!client_io.operation&&clients[id].token==client_io.token&&clients[id].generation==client_io.generation&&client_io.token){clients[id].event=client_io.event;return;}
 if(clients[id].revision!=client_io.revision||clients[id].generation!=client_io.generation||clients[id].token!=client_io.token){
  if(yes&&scene.window[id+2].open&&!scene.window[id+2].hidden)scene_window_damage(&scene,id+2,scene.window[id+2].r);
  clients[id]=client_io;
 }
}
static void launch(int act){
 /* An app can finish between GUI timer events. Decide from the monitor's
  * current state rather than rejecting an available slot from the cache. */
 refresh();
 unsigned id=0;
 while(id<(unsigned)scene.slots&&(views[id].state==1||views[id].state==2||views[id].state==3||(views[id].state==4&&!views[id].owned)))id++;
 if(id==(unsigned)scene.slots){scene.action_result=-16;scene_damage(&scene,scene.window[0].r);return;}
 const char *path=act==ACT_CALCULATOR?"KGCALC.EXE":act==ACT_CLOCK?"KGCLOCK.EXE":act==ACT_FILES?"KGFILES.EXE":act==ACT_EDITOR?"KGEDIT.EXE":act==ACT_IMAGE?"KGIMAGE.EXE":"KSH.EXE";
 for(unsigned i=0;i<128;i++){paths[id][i]=path[i];if(!path[i])break;}
 errors[id]=session_command(id,GS_CREATE,0);scene.action_result=errors[id];
 if(!errors[id])scene_focus(&scene,id+2);
 else scene_damage(&scene,scene.window[0].r);
 refresh();
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
static unsigned key_modifiers;
static void action(int act){
 unsigned id=scene.slot;
 if(act>=ACT_CALCULATOR){launch(act);return;}
 if(act==ACT_START||act==ACT_STOP){scene.action_result=k32_call(GUI_PROOF,act==ACT_START?1:2,0);scene_damage(&scene,scene.window[1].r);refresh();return;}
 if(act<ACT_NEW)return;
 if(act==ACT_SCROLL){following[id]=0;return;}
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
  if(act==ACT_KEY&&scene.graphical[id]){
   if(clients[id].token){KgEvent event={.type=KG_KEY,.key=scene.key,.control=key_modifiers};errors[id]=client_event(id,&event);}
  }else errors[id]=session_command(id,op,scene.key);
  if(act==ACT_KEY&&!scene.graphical[id])following[id]=3;
  if(act==ACT_RESTART&&!errors[id])scene.scroll_x[id]=scene.scroll_y[id]=0;
 }
 refresh();if(act==ACT_KEY&&!editing[id]&&!scene.graphical[id])follow_cursor(id);
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
static const u8 *glyph(unsigned code){
 for(unsigned i=0;i<128;i++)if(glyph_cache[i].code==code)return glyph_cache[i].bitmap;
 unsigned i=glyph_slot++%128;GuiGlyph r={.version=1,.size=sizeof r,.code=code};
 if(k32_call(GUI_FONT,(u32)&r,sizeof r)||r.columns!=kt_glyph(code)){
  for(unsigned j=0;j<32;j++)r.bitmap[j]=font[('?' -32)*16+j%16];
 }
 glyph_cache[i].code=code;for(unsigned j=0;j<32;j++)glyph_cache[i].bitmap[j]=r.bitmap[j];return glyph_cache[i].bitmap;
}
static void text(int x,int y,const char *s,u32 c){
 for(;*s;){unsigned cells=kt_unit(s,s[1]?2:1),ch=(u8)*s;
  if(!cells){cells=1;ch='?';}
  unsigned code=cells==2?(ch<<8)|(u8)s[1]:ch;const u8 *bits_data;
  s+=cells;int origin=x;x+=cells*8;
  if(ch==' '||x<=clip.x||origin>=clip.x+clip.w||y+16<=clip.y||y>=clip.y+clip.h)continue;
  bits_data=code>=32&&code<=126?font+(code-32)*16:glyph(code);
  for(int row=0;row<16;row++){
   int yy=y+row;if(yy<clip.y||yy>=clip.y+clip.h)continue;
   u8 *line=framebuffer+yy*info.pitch;
   for(unsigned col=0;col<cells*8;col++)if((bits_data[col/8*16+row]&(128>>(col%8)))&&origin+(int)col>=clip.x&&origin+(int)col<clip.x+clip.w){
    if(info.bpp==1)line[origin+col]=(u8)c;else ((u16 *)line)[origin+col]=(u16)c;
   }
  }
 }
}
static void ui_label(int x,int y,const char *s,u32 c){text(x,y,KU(s),c);}
static void number(int x,int y,u32 n,u32 c){char rev[10],out[11];unsigned k=0,i=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);while(k)out[i++]=rev[--k];out[i]=0;text(x,y,out,c);}
static void button(int x,int y,int w,const char *s){
 s=KU(s);
 box(x,y,w,28,color(11));box(x+1,y+1,w-2,26,color(3));box(x+2,y+2,w-4,1,color(7));text(x+(w-8*(int)k32_len(s))/2,y+6,s,color(0));
}
static void terminal(unsigned id,int x,int y,int width,int height){
 GuiSessionView *v=&views[id];
 const char *names[]={"Run","Stop","Resume","End","Again"};
 for(int i=0;i<5;i++)button(x+8+i*64,y+36,60,names[i]);
 int cols=(width-16)/8,rows=(height-104)/16;
 box(x+8,y+70,cols*8,rows*16,terminal_valid[id]&&!v->kind?rgb(0,0,0):color(0));
 Rect before=clip;Rect area={x+8,y+70,cols*8,rows*16};
 int right=clip.x+clip.w,bottom=clip.y+clip.h;
 if(clip.x<area.x)clip.x=area.x;
 if(clip.y<area.y)clip.y=area.y;
 if(right>area.x+area.w)right=area.x+area.w;
 if(bottom>area.y+area.h)bottom=area.y+area.h;
 clip.w=right>clip.x?right-clip.x:0;clip.h=bottom>clip.y?bottom-clip.y:0;
 for(int row=0;row<rows;row++){
  int rr=row+scene.scroll_y[id];if(rr>=25)break;
  if(terminal_valid[id]&&terminals[id].generation==v->generation&&!v->kind){
   const u16 *codes=terminals[id].codes+rr*80;
   u16 row_glyphs[80];u8 row_halves[80];k98_row(codes,row_glyphs,row_halves);
   for(int col=0;col<cols&&col+scene.scroll_x[id]<80;col++){
    unsigned at=col+scene.scroll_x[id],half=row_halves[at],code=row_glyphs[at],attr=terminals[id].attributes[rr*80+at];
    if(code==' '&&!(attr&28))continue;
    const u8 *bitmap=code>=32&&code<=126?font+(code-32)*16:glyph(code);
    u32 ink=rgb((k98_rgb(attr)>>16)&255,(k98_rgb(attr)>>8)&255,k98_rgb(attr)&255);
    int xx=x+8+col*8,yy=y+70+row*16;
    for(unsigned scan=0;scan<16;scan++){
     unsigned bits=k98_ink(bitmap[half*16+scan],attr,scan,terminal_phase,0);
     int py=yy+scan;if(py<clip.y||py>=clip.y+clip.h)continue;
     u8 *line=framebuffer+py*info.pitch;
     for(unsigned bit=0;bit<8;bit++)if(bits&(128>>bit)){int px=xx+bit;if(px<clip.x||px>=clip.x+clip.w)continue;if(info.bpp==1)line[px]=(u8)ink;else ((u16 *)line)[px]=(u16)ink;}
    }
   }
   continue;
  }
  char line[81];int col=0;
  for(;col<cols&&col+scene.scroll_x[id]<80;col++)line[col]=v->screen[rr*80+col+scene.scroll_x[id]];
  line[col]=0;text(x+8,y+70+row*16,line,color(7));
 }
 if(v->owned&&(v->state==1||v->state==2)&&scene.active==(int)id+2){
  unsigned half=0,code=32;if(terminal_valid[id]&&v->row<25&&v->column<80)code=k98_glyph(terminals[id].codes+v->row*80,v->column,&half);
  box(x+8+((int)v->column-(int)half-scene.scroll_x[id])*8,y+84+((int)v->row-scene.scroll_y[id])*16,code>255?16:8,2,color(6));
 }
 clip=before;
 int yy=y+height-30;const char *arrows[]={"Left","Right","Up","Down"};
 for(int i=0;i<4;i++)button(x+8+i*48,yy,44,arrows[i]);
 ui_label(x+208,yy+6,errors[id]?"Error:":!v->owned&&v->state?"Other owner":v->state==1?"Running":v->state==2?"Waiting":v->state==3?"Stopped":v->state==4?"Faulted":"Ended",color(10));
 if(errors[id])number(x+264,yy+6,-errors[id],color(10));
 text(x+width-18,y+height-18,"/",color(10));
 if(editing[id]){
  box(x+8,y+70,width-16,72,color(3));ui_label(x+16,y+78,"Run (End current first):",color(0));
  int n=lengths[id],visible=(width-40)/8,start=n>visible?n-visible:0;
  text(x+16,y+102,paths[id]+start,color(0));box(x+16+(n-start)*8,y+116,8,2,color(4));
  ui_label(x+16,y+126,"Enter: start   Esc: cancel",color(10));
 }
}
static void graphical(unsigned id,int x,int y,int width,int height){
 const char *names[]={"Run","Stop","Resume","End","Again"};
 for(int i=0;i<5;i++)button(x+8+i*64,y+36,60,names[i]);
 Rect saved=clip;int left=x+8,top=y+70,right=x+width-8,bottom=y+height-34;
 if(clip.x>left)left=clip.x;
 if(clip.y>top)top=clip.y;
 if(clip.x+clip.w<right)right=clip.x+clip.w;
 if(clip.y+clip.h<bottom)bottom=clip.y+clip.h;
 clip=(Rect){left,top,right>left?right-left:0,bottom>top?bottom-top:0};
 box(x+8,y+70,width-16,height-104,rgb(234,240,245));
 KgRequest *r=&clients[id];
 for(unsigned i=0;r->token&&i<r->count;i++){
  KgDraw *d=&r->draw[i];int xx=x+8+d->x,yy=y+70+d->y;u32 c=rgb(d->color>>16,(d->color>>8)&255,d->color&255);
  if(d->op==KG_RECT)box(xx,yy,d->w,d->h,c);
  if(d->op==KG_TEXT)text(xx,yy,d->text,c);
  if(d->op==KG_BITMAP){
   const KgImage *im=&images[id];
   if(im->valid&&im->handle==d->id&&im->generation==r->generation&&im->token==r->token){
    unsigned sx=d->color&65535,sy=d->color>>16;
    if(sx<im->width&&sy<im->height&&(unsigned)d->w<=im->width-sx&&(unsigned)d->h<=im->height-sy){
     const u16 *p=image_pixels[id]+im->front*KG_SURFACE_PIXELS;
     int l=xx>clip.x?xx:clip.x,t=yy>clip.y?yy:clip.y,right=xx+d->w<clip.x+clip.w?xx+d->w:clip.x+clip.w,bottom=yy+d->h<clip.y+clip.h?yy+d->h:clip.y+clip.h;
     for(int row=t;row<bottom;row++)for(int col=l;col<right;col++){u16 c=p[(sy+row-yy)*im->width+sx+col-xx];u32 pixel=info.bpp==2?c:rgb(((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31);box(col,row,1,1,pixel);}
    }
   }
  }
  if(d->op==KG_BUTTON){
   box(xx,yy,d->w,d->h,color(11));box(xx+1,yy+1,d->w-2,d->h-2,c);box(xx+2,yy+2,d->w-4,1,color(7));
   text(xx+(d->w-8*(int)k32_len(d->text))/2,yy+(d->h-16)/2,d->text,color(0));
  }
 }
 if(!r->token)ui_label(x+24,y+86,views[id].state==4?"Application faulted":"Application ended",color(10));
 clip=saved;
 ui_label(x+16,y+height-22,views[id].state==3?"Stopped":views[id].state==4?"Faulted":views[id].state==0?"Ended":"PM32 GUI client",color(10));
 text(x+width-18,y+height-18,"/",color(10));
 if(editing[id]){
  box(x+8,y+70,width-16,72,color(3));ui_label(x+16,y+78,"Run (End current first):",color(0));
  int n=lengths[id],visible=(width-40)/8,start=n>visible?n-visible:0;
  text(x+16,y+102,paths[id]+start,color(0));ui_label(x+16,y+126,"Enter: start   Esc: cancel",color(10));
 }
}
static void client_mouse(const GuiEvent *e){
 if(e->type!=GUI_BUTTON&&e->type!=GUI_MOTION)return;
 if(capture){unsigned id=capture-1;GuiWindow *w=&scene.window[id+2];
  if(scene.active!=(int)id+2||!w->open||w->hidden||editing[id]||scene.drag||scene.resize||clients[id].token!=capture_token||views[id].generation!=capture_generation)capture=0;
 }
 if(scene.active<2||scene.drag||scene.resize)return;
 unsigned id=capture?capture-1:(unsigned)scene.active-2;
 if(!scene.graphical[id]||!clients[id].token||editing[id])return;
 GuiWindow *w=&scene.window[id+2];int x=(int)e->x-w->r.x-8,y=(int)e->y-w->r.y-70;
 if(!w->open||w->hidden||(!capture&&(x<0||y<0||x>=w->r.w-16||y>=w->r.h-104)))return;
 unsigned flags=clients[id].event.control;
 if(e->type==GUI_MOTION&&(!(flags&KG_INPUT_MOTION)||!capture))return;
 if(e->type==GUI_BUTTON&&(e->buttons&1)&&(flags&KG_INPUT_MOTION)){
  capture=id+1;capture_generation=views[id].generation;capture_token=clients[id].token;
 }
 KgEvent event={.type=e->type==GUI_MOTION?KG_POINTER:KG_MOUSE,.x=x,.y=y,.key=e->buttons};
 KgRequest *r=&clients[id];
 for(unsigned i=0;i<r->count;i++){KgDraw *d=&r->draw[i];if(d->op==KG_BUTTON&&x>=d->x&&y>=d->y&&x-d->x<d->w&&y-d->y<d->h)event.control=d->id;}
 errors[id]=client_event(id,&event);
 if(!(e->buttons&1))capture=0;
}
static void window(int id){
 GuiWindow *w=&scene.window[id];if(!w->open||w->hidden)return;
 int x=w->r.x,y=w->r.y,width=w->r.w,height=w->r.h;
 if(x+width+6<=clip.x||x>=clip.x+clip.w||y+height+6<=clip.y||y>=clip.y+clip.h)return;
 box(x+5,y+5,width,height,rgb(8,16,28));box(x,y,width,height,color(11));box(x+1,y+1,width-2,height-2,color(2));
 box(x+2,y+2,width-4,28,color(scene.active==id?4:9));box(x+3,y+3,width-6,1,color(scene.active==id?13:3));
 char label[6]="DOS 1";if(id>=2){if(scene.graphical[id-2]){label[0]='A';label[1]='p';label[2]='p';}label[4]='1'+id-2;}
 ui_label(x+12,y+9,id>=2?label:id?"System":"Welcome to KGUI32",color(scene.active==id?7:0));
 box(x+width-47,y+6,18,18,color(3));box(x+width-25,y+6,18,18,color(3));
 text(x+width-43,y+7,"_",color(0));text(x+width-21,y+7,"x",color(0));
 if(id>=2){
  ui_label(x+72,y+9,scene.graphical[id-2]?clients[id-2].title:views[id-2].program,color(7));
  if(scene.graphical[id-2])graphical(id-2,x,y,width,height);else terminal(id-2,x,y,width,height);
 }else if(!id){
  box(x+16,y+48,46,46,color(4));box(x+24,y+56,12,28,color(7));box(x+38,y+56,12,12,color(6));box(x+38,y+72,12,12,color(3));
  ui_label(x+78,y+48,"Your new desktop",color(0));ui_label(x+78,y+72,"32-bit protected mode",color(10));
  button(x+18,y+116,140,"Calculator");button(x+174,y+116,140,"Clock");button(x+18,y+152,140,"DOS Shell");button(x+174,y+152,140,"Files");button(x+18,y+188,140,"Editor");button(x+174,y+188,140,"Images");
  ui_label(x+18,y+92,scene.action_result<0?"Slots full / error":"F2 / F10",color(10));
 }else{
  ui_label(x+16,y+46,"Desktop: private context",color(0));
  ui_label(x+16,y+70,scene.slots==4?"Application slots: 4":"Application slots: 2",color(0));
  for(int i=0;i<scene.slots;i++){
   int yy=y+98+i*24;number(x+16,yy,i+1,color(0));ui_label(x+32,yy,scene.states[i]==1?"Running":scene.states[i]==2?"Waiting":scene.states[i]==3?"Stopped":scene.states[i]==4?"Faulted":"Free",color(10));number(x+108,yy,scene.counters[i],color(0));
  }
  ui_label(x+16,y+146+(scene.slots-2)*24,scene.action_result<0?"Slots busy / unavailable":"CPU workers: T=start S=stop",color(10));
  button(x+16,y+168+(scene.slots-2)*24,94,"Test CPU");button(x+124,y+168+(scene.slots-2)*24,94,"Stop");
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
 box(12,8,12,16,color(6));box(26,8,6,16,color(4));text(44,9,"KGUI32",color(7));text(124,9,"32-bit",color(9));ui_label(190,9,"F8:JP/EN",color(9));
 for(int i=0;i<scene.slots;i++){char label[]="DOS 1";label[4]+=i;if(scene.slots==2)button(info.width-312+i*92,3,84,i?"New DOS 2":"New DOS 1");else button(info.width-360+i*56,3,52,label);}
 button(info.width-128,3,120,"Exit to DOS");
 for(int i=0;i<scene.slots+2;i++)window(scene.order[i]);
 box(0,info.height-32,info.width,32,color(0));box(0,info.height-32,info.width,1,color(4));
 button(6,info.height-29,136,"Welcome");button(152,info.height-29,136,"System");
 for(int i=0;i<scene.slots;i++)if(scene.window[i+2].open){char label[]="DOS 1";label[4]+=i;int step=scene.slots==2?146:(info.width-300)/4;button(300+i*step,info.height-29,step-10,scene.graphical[i]?clients[i].title:label);}
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
 static const u32 client_header[4]={1,sizeof(KgRequest),0,0};
 if(k32_call(GUI_CLIENT_VIEW,0x10000,sizeof(KgRequest))!=-14)return 12;
 if(k32_call(GUI_CLIENT_VIEW,(u32)client_header,sizeof(KgRequest))!=-14)return 13;
 client_io=(KgRequest){.version=1,.size=sizeof client_io,.slot=scene.slots};
 if(k32_call(GUI_CLIENT_VIEW,(u32)&client_io,sizeof client_io)!=-22)return 14;
 client_io.slot=0;client_io.generation=~0u;
 if(k32_call(GUI_CLIENT_EVENT,(u32)&client_io,sizeof client_io)!=-1)return 15;
 static const u32 readonly_header[4]={1,sizeof(GuiSessionView),0,0};
 GuiSessionCommand c={.version=1,.size=sizeof c,.operation=GS_END};
 GuiSessionView v={.version=1,.size=sizeof v};
 if(k32_call(GUI_SESSION,0x10000,sizeof c)!=-14)return 1;
 if(k32_call(GUI_SNAPSHOT,0x10000,sizeof v)!=-14)return 2;
 if(k32_call(GUI_SNAPSHOT,(u32)readonly_header,sizeof v)!=-14)return 11;
 if(k32_call(GUI_SNAPSHOT,0x800000,sizeof v)!=-22)return 3; /* RO ELF header is not a valid versioned request. */
 if(k32_call(GUI_SESSION,(u32)&c,sizeof c-1)!=-22)return 4;
 c.slot=scene.slots;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-22)return 5;c.slot=0;
 c.operation=7;if(k32_call(GUI_SESSION,(u32)&c,sizeof c)!=-22)return 6;
 for(unsigned id=0;id<(unsigned)scene.slots;id++){
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
 GuiGlyph g={.version=1,.size=sizeof g,.code=0x93fa};
 if(k32_call(GUI_FONT,0x10000,sizeof g)!=-14)return 6;
 if(k32_call(GUI_FONT,(u32)&g,sizeof g)||g.columns!=2)return 7;
 g.columns=0;g.code=0x80;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 8;
 g.code=0x817f;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 9;
 g.code=0x10093fa;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 10;
 g.code=0x93fa;g.version=2;if(k32_call(GUI_FONT,(u32)&g,sizeof g)!=-22)return 11;
 static const GuiGlyph ro={.version=1,.size=sizeof(GuiGlyph),.code=0x93fa};
 if(k32_call(GUI_FONT,(u32)&ro,sizeof ro)!=-14)return 12;
 if(k32_call(GUI_SNAPSHOT,0x10000,sizeof terminal_stage)!=-14)return 13;
 static const u32 text_ro[4]={2,sizeof(GuiTerminalView),0,0};
 if(k32_call(GUI_SNAPSHOT,(u32)text_ro,sizeof terminal_stage)!=-14)return 14;
 terminal_stage=(GuiTerminalView){.version=3,.size=sizeof terminal_stage};
 if(k32_call(GUI_SNAPSHOT,(u32)&terminal_stage,sizeof terminal_stage)!=-22)return 15;
 if(k32_call(GUI_SNAPSHOT,(u32)&terminal_stage,sizeof terminal_stage-1)!=-22)return 16;
 GuiSurfaceAck ack={.version=1,.size=sizeof ack};
 if(k32_call(GUI_SURFACE_ACK,0x10000,sizeof ack)!=-14)return 17;
 ack.slot=scene.slots;if(k32_call(GUI_SURFACE_ACK,(u32)&ack,sizeof ack)!=-22)return 18;
 ack.slot=0;if(k32_call(GUI_SURFACE_ACK,(u32)&ack,sizeof ack)!=-1)return 19;
 return 0;
}
int main(int argc,char **argv){
 terminal_phase=1;
 if((k32_query()&23)!=23)return 1;
 int e=k32_call(GUI_INFO,(u32)&info,sizeof info);if(e)return 2;
 if(info.magic!=0x3149474b||info.version!=1||info.width>800||info.height>600||(info.bpp!=1&&info.bpp!=2)||info.pitch!=info.width*info.bpp)return 3;
 if(k32_call(GUI_FONT,(u32)font,sizeof font))return 4;
 gui_state.bitmap_magic=0x3153474b;gui_state.bitmap_meta=(u32)images;gui_state.bitmap_pixels=(u32)image_pixels;gui_state.bitmap_stride=sizeof image_pixels[0];gui_state.bitmap_frames=(u32)clients;
 scene_init(&scene,info.width,info.height);scene_slots(&scene,info.features&GUI_FOUR_SLOTS?4:2);gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;
 refresh();
 int once=argc>1&&equal(argv[1],"once"),proof=argc>1&&equal(argv[1],"proof");
 if(argc>1&&equal(argv[1],"sessionapi")){int bad=session_negative();gui_state.error=bad?200+bad:0;return bad?5:0;}
 if(proof)scene.action_result=k32_call(GUI_PROOF,1,0);
 if(argc>1&&equal(argv[1],"badapi")){int bad=negative();if(bad){gui_state.error=100+bad;return 5;}once=1;}
 if(paint()){gui_state.error=6;return 6;}
 if(argc>1&&equal(argv[1],"fault"))*(volatile u32 *)0x10000=1;
 if(argc>1&&equal(argv[1],"spin"))for(;;)gui_state.heartbeat++;
 for(;;){
  GuiEvent4 event4={0};int got=k32_call(GUI_EVENT,(u32)&event4,sizeof event4);GuiEvent event=event4.base;
  if(got<0){gui_state.error=7;return 7;}
  if(!got){k32_wait(K32_WAIT_TIME,16);continue;}
  gui_state.events++;gui_state.heartbeat++;key_modifiers=event.type==GUI_KEY?event.buttons:0;if(event.type==GUI_KEY)gui_state.last_key=event.key;
  if(event.type==GUI_KEY&&event.key==287){ku_language^=1;k32_call(GUI_LANGUAGE,ku_language,0);scene_damage(&scene,(Rect){0,0,info.width,info.height});if(paint())return 7;continue;}
  if(!terminal_tick)terminal_tick=event.ticks;
  if(((event.ticks-terminal_tick)&0xffffff)>=153600){terminal_tick=event.ticks;terminal_phase^=1;
   for(unsigned id=0;id<(unsigned)scene.slots;id++)if(terminal_valid[id]&&scene.window[id+2].open&&!scene.window[id+2].hidden){
    unsigned blink=0;for(unsigned i=0;i<2000;i++)if(terminals[id].attributes[i]&2){blink=1;break;}
    if(blink)scene_window_damage(&scene,id+2,scene.window[id+2].r);
   }
  }
  int previous_focus=scene.active;Rect previous_rect[4];for(unsigned id=0;id<(unsigned)scene.slots;id++)previous_rect[id]=scene.window[id+2].r;
  /* A child may OPEN/SUBSCRIBE between timer events. Route input against
   * its current generation/token, never the ended root's cached canvas. */
  if(event.type==GUI_KEY||event.type==GUI_BUTTON)refresh();
  int act;
  if(event.type==GUI_KEY&&scene.active>=2&&event.key!=281&&event.key!=11&&event.key!=289&&
     ((editing[scene.active-2]&&event.key!=9)||(!editing[scene.active-2]&&clients[scene.active-2].token&&event.key!=289&&((event.key==9&&(clients[scene.active-2].event.control&KG_INPUT_TAB))||((clients[scene.active-2].event.control&KG_INPUT_KEYS)&&event.key!=9))))){
   scene.slot=scene.active-2;scene.key=event.key;act=ACT_KEY;
  }else act=scene_event4(&scene,&event4);
  if(act==ACT_EXIT){
   exit_pending=1;
   for(unsigned id=0;id<(unsigned)scene.slots;id++)if(clients[id].token&&(clients[id].event.control&KG_INPUT_CLOSE)){
    KgEvent q={.type=KG_QUIT,.key=2};client_event(id,&q);
   }
   act=ACT_NONE;
  }
  if(act==ACT_END&&event.type==GUI_BUTTON&&scene.slot<scene.slots&&clients[scene.slot].token&&event.x>=(u32)(scene.window[scene.slot+2].r.x+scene.window[scene.slot+2].r.w-25)&&event.y<(u32)(scene.window[scene.slot+2].r.y+30)&&(clients[scene.slot].event.control&KG_INPUT_CLOSE)){
   unsigned id=scene.slot;scene.window[id+2].open=1;scene_focus(&scene,id+2);
   KgEvent q={.type=KG_QUIT,.key=1};if(!client_event(id,&q)){close_generation[id]=views[id].generation;close_token[id]=clients[id].token;close_cancel[id]=clients[id].event.key;}act=ACT_NONE;
  }
  action(act);if(event.type==GUI_TIMER)refresh();
  if(exit_pending){unsigned pending=0;for(unsigned id=0;id<(unsigned)scene.slots;id++)if(clients[id].token&&(clients[id].event.control&KG_INPUT_CLOSE))pending=1;if(!pending)return 0;}
  for(unsigned id=0;id<(unsigned)scene.slots;id++)if(scene.graphical[id]&&clients[id].token){
   Rect r=scene.window[id+2].r;
   if(r.w!=previous_rect[id].w||r.h!=previous_rect[id].h){capture=0;KgEvent change={.type=KG_RESIZE,.width=r.w-16,.height=r.h-104};client_event(id,&change);}
   if(previous_focus!=scene.active&&(previous_focus==(int)id+2||scene.active==(int)id+2)){KgEvent focus={.type=KG_FOCUS,.key=scene.active==(int)id+2};client_event(id,&focus);capture=0;}
  }
  if(act==ACT_NONE)client_mouse(&event);
  input_focus();
  gui_state.mx=scene.mx;gui_state.my=scene.my;gui_state.x0=scene.window[0].r.x;gui_state.y0=scene.window[0].r.y;gui_state.x1=scene.window[1].r.x;gui_state.y1=scene.window[1].r.y;gui_state.active=scene.active;gui_state.drag=scene.drag;
  terminal_report();
  e=paint();if(e){gui_state.error=8;return 8;}
  if((once&&gui_state.events>=20)||(proof&&gui_state.events>=16))return 0;
 }
}
