#include "scene.h"
#include "shell-ui.h"
#include "gallery.h"
#include "sdk/protocol.h"
#include "../desktop-style.h"
enum { NOTICE_WIDTH=512, NOTICE_TICKS=4*307200u };
static int inside(Rect r,int x,int y){return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;}
static Rect covered(GuiWindow *w){return (Rect){w->r.x,w->r.y,w->r.w+6,w->r.h+6};}
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
void scene_damage(Scene *s,Rect r){
 if(r.x<0){r.w+=r.x;r.x=0;}if(r.y<0){r.h+=r.y;r.y=0;}
 if(r.x+r.w>s->width)r.w=s->width-r.x;
 if(r.y+r.h>s->height)r.h=s->height-r.y;
 if(r.w<=0||r.h<=0)return;
 if(!s->damage.w){s->damage=r;return;}
 int right=s->damage.x+s->damage.w,bottom=s->damage.y+s->damage.h;
 if(r.x<s->damage.x)s->damage.x=r.x;
 if(r.y<s->damage.y)s->damage.y=r.y;
 if(r.x+r.w>right)right=r.x+r.w;
 if(r.y+r.h>bottom)bottom=r.y+r.h;
 s->damage.w=right-s->damage.x;s->damage.h=bottom-s->damage.y;
}
/* Remove higher opaque window rectangles before invalidating changing text.
 * Cached content still advances; revealing a window repaints from that cache. */
static void visible_damage(Scene *s,Rect r,int level){
 if(r.w<=0||r.h<=0)return;
 for(int n=level;n<s->slots+2;n++){
  GuiWindow *w=&s->window[s->order[n]];if(!w->open||w->hidden)continue;
  Rect c=w->r;int l=r.x>c.x?r.x:c.x,t=r.y>c.y?r.y:c.y;
  int right=r.x+r.w<c.x+c.w?r.x+r.w:c.x+c.w,bottom=r.y+r.h<c.y+c.h?r.y+r.h:c.y+c.h;
  if(l>=right||t>=bottom)continue;
  visible_damage(s,(Rect){r.x,r.y,r.w,t-r.y},n+1);
  visible_damage(s,(Rect){r.x,bottom,r.w,r.y+r.h-bottom},n+1);
  visible_damage(s,(Rect){r.x,t,l-r.x,bottom-t},n+1);
  visible_damage(s,(Rect){right,t,r.x+r.w-right,bottom-t},n+1);return;
 }
 scene_damage(s,r);
}
void scene_window_damage(Scene *s,int id,Rect r){
 for(int n=0;n<s->slots+2;n++)if(s->order[n]==id){visible_damage(s,r,n+1);return;}
}
Rect scene_launcher_button(unsigned index){
 return (Rect){18+(int)(index%2)*156,80+(int)(index/2)*36,140,28};
}
Rect scene_notice_rect(const Scene *s){
 return (Rect){(s->width-NOTICE_WIDTH)/2,s->height-32-DESKTOP_SPACE_MEDIUM-DESKTOP_CONTROL_HEIGHT,
  NOTICE_WIDTH,DESKTOP_CONTROL_HEIGHT};
}
void scene_notice_set(Scene *s,unsigned kind){
 if(!s->notice&&!kind)return;
 s->notice=kind;s->notice_started=s->ticks;
 scene_damage(s,scene_notice_rect(s));
}
void scene_notice_tick(Scene *s,u32 ticks){
 s->ticks=ticks;
 /* The PC-98 307.2 kHz input timer wraps at 24 bits. */
 u32 elapsed=(ticks-s->notice_started)&0xffffff;
 if(s->notice&&elapsed>=NOTICE_TICKS&&elapsed<0x800000)scene_notice_set(s,GN_NONE);
}
static void launcher_page(Scene *s,int page){
 s->help=page;
 scene_damage(s,s->window[0].r);
}
void scene_init(Scene *s,int width,int height){
 *s=(Scene){.width=width,.height=height,.mx=320,.my=200,.active=0,.order={3,2,1,0},.slots=2};
 s->window[0]=(GuiWindow){{48,64,332,280},1,0};
 s->window[1]=(GuiWindow){{width-252,64,244,224},1,0};
 for(int i=0;i<2;i++)s->window[i+2]=(GuiWindow){{16+i*8,40+i*8,width>688?656:width-32,height>576?496:height-80},0,0};
 scene_damage(s,(Rect){0,0,width,height});
}
void scene_slots(Scene *s,unsigned n){
 if(n!=2&&n!=4)return;
 s->slots=n;
 for(unsigned i=0;i<n+2;i++)s->order[i]=n+1-i;
 for(unsigned i=2;i<n;i++)s->window[i+2]=(GuiWindow){{32+(int)i*8,40+(int)i*8,s->width>688?656:s->width-32,s->height>576?496:s->height-80},0,0};
 s->window[1].r.h=224+(n-2)*24;
}
void scene_focus(Scene *s,int id){
 if(s->active==id&&s->window[id].open&&!s->window[id].hidden)return;
 for(int i=0;i<s->slots+2;i++)if(s->window[i].open&&!s->window[i].hidden)scene_damage(s,covered(&s->window[i]));
 for(int i=0;i<s->slots+1;i++)if(s->order[i]==id){for(int j=i;j<s->slots+1;j++)s->order[j]=s->order[j+1];break;}
 s->order[s->slots+1]=id;s->active=id;s->window[id].open=1;s->window[id].hidden=0;scene_damage(s,covered(&s->window[id]));
 scene_damage(s,(Rect){0,s->height-32,s->width,32});
}
static void next_visible(Scene *s){
 scene_damage(s,(Rect){0,s->height-32,s->width,32});
 for(int n=s->slots+1;n>=0;n--){int id=s->order[n];if(s->window[id].open&&!s->window[id].hidden){s->active=id;scene_damage(s,covered(&s->window[id]));return;}}
 s->active=0;
}
void scene_close(Scene *s,int id){GuiWindow *w=&s->window[id];w->open=0;scene_damage(s,covered(w));if(s->active==id)next_visible(s);}
static void move(Scene *s,int id,int x,int y){
 GuiWindow *w=&s->window[id];scene_damage(s,covered(w));
 w->r.x=clamp(x,8,s->width-w->r.w-8);w->r.y=clamp(y,40,s->height-32-w->r.h-8);
 scene_damage(s,covered(w));
}
/* Tiling uses the minimum client width, overlapping only on 640px desktops. */
static void place_window(Scene *s,int id,int mode){
 GuiWindow *w=&s->window[id];
 Rect *restored=&s->restored[id-2];
 scene_damage(s,covered(w));
 if(!mode){
  if(restored->w)w->r=*restored;
  *restored=(Rect){0};
 }else{
  if(!restored->w)*restored=w->r;
  int width=s->width-16,height=s->height-80;
  if(height>520)height=520; /* 416 client pixels plus 104 chrome. */
  if(mode!=1)width=clamp((width-8)/2,360,width);
  w->r=(Rect){mode==3?s->width-8-width:8,40,width,height};
 }
 s->placement[id-2]=mode;
 scene_damage(s,covered(w));
}
void scene_restore_window(Scene *s,int id,Rect r,unsigned mode){
 GuiWindow *w=&s->window[id];scene_damage(s,covered(w));
 if(id>=2){
  r.w=clamp(r.w,176,s->width-16);r.h=clamp(r.h,184,s->height-80);
  s->restored[id-2]=(Rect){0};s->placement[id-2]=0;
 }
 r.x=clamp(r.x,8,s->width-r.w-8);r.y=clamp(r.y,40,s->height-40-r.h);w->r=r;
 if(id>=2&&mode)place_window(s,id,mode);
 scene_damage(s,covered(w));
}
int scene_event(Scene *s,const GuiEvent *e){
 if(e->type==GUI_TIMER){
  shell_timer(s,e->key);
  GuiWindow *w=&s->window[1];
  for(int i=0;i<2;i++){
   if(w->open&&!w->hidden){
    if(s->states[i]!=e->states[i])scene_window_damage(s,1,(Rect){w->r.x+32,w->r.y+98+i*24,76,16});
    if(s->counters[i]!=e->counters[i])scene_window_damage(s,1,(Rect){w->r.x+108,w->r.y+98+i*24,120,16});
   }
   s->counters[i]=e->counters[i];s->states[i]=e->states[i];
  }
  return ACT_NONE;
 }
 if(e->type==GUI_KEY){
  if(e->key==289)return ACT_EXIT;
  int shell=shell_key(s,e);if(shell>=0)return shell;
  if(e->key==281){if(s->help)launcher_page(s,LAUNCHER_APPS);scene_focus(s,0);return ACT_NONE;}
  if(s->active==0&&e->key==280){launcher_page(s,s->help?LAUNCHER_APPS:LAUNCHER_GUIDE);return ACT_NONE;}
  if(s->active==0&&e->key==27&&s->help){launcher_page(s,s->help==LAUNCHER_GALLERY?LAUNCHER_GUIDE:LAUNCHER_APPS);return ACT_NONE;}
  if(s->active==0&&s->help==LAUNCHER_GUIDE&&(e->key=='g'||e->key=='G')){
   launcher_page(s,LAUNCHER_GALLERY);return ACT_NONE;
  }
  if(e->key==9){
   int id=(s->active+1)%(s->slots+2);
   while(id>=2&&id!=s->active&&!s->window[id].open)id=(id+1)%(s->slots+2);
   scene_focus(s,id);return ACT_NONE;
  }
  if(s->active>=2&&s->window[s->active].open&&!s->window[s->active].hidden){
   s->slot=s->active-2;s->key=e->key;
   if(e->key==12)return ACT_RUN;
   if(e->key==16)return ACT_PAUSE;
   if(e->key==18)return ACT_RESUME;
   if(e->key==5)return ACT_END;
   return ACT_KEY;
  }
  if(e->key==288)return ACT_SETTINGS;
  if(e->key==283)return ACT_MEMO;
  if(e->key=='q'||e->key=='Q')return ACT_EXIT;
  if(e->key=='t'||e->key=='T')return ACT_START;
  if(e->key=='s'||e->key=='S')return ACT_STOP;
  GuiWindow *w=&s->window[s->active];
  if(e->key>=256&&e->key<=259&&w->open&&!w->hidden)
   move(s,s->active,w->r.x+(e->key==258?-8:e->key==259?8:0),w->r.y+(e->key==256?-8:e->key==257?8:0));
  return ACT_NONE;
 }
 int down=(e->buttons&1)&&!(s->buttons&1);
 scene_damage(s,(Rect){s->mx,s->my,17,19});s->mx=e->x;s->my=e->y;
 scene_damage(s,(Rect){s->mx,s->my,17,19});s->buttons=e->buttons;
 if(s->resize){
  GuiWindow *w=&s->window[s->resize-1];scene_damage(s,covered(w));
  w->r.w=clamp(s->mx-w->r.x+s->dx,360,s->width-w->r.x-8);
  w->r.h=clamp(s->my-w->r.y+s->dy,s->resize>=3&&s->graphical[s->resize-3]?272:208,s->height-40-w->r.y);
  scene_damage(s,covered(w));if(!(s->buttons&1))s->resize=0;return ACT_NONE;
 }
 if(s->drag){move(s,s->drag-1,s->mx-s->dx,s->my-s->dy);if(!(s->buttons&1))s->drag=0;return ACT_NONE;}
 int shell=shell_mouse(s,e,down);if(shell>=0)return shell;
 if(s->notice_click){
  if(!(s->buttons&1))s->notice_click=0;
  return ACT_NOTICE_INPUT;
 }
 if(down&&s->notice&&inside(scene_notice_rect(s),s->mx,s->my)){
  scene_notice_set(s,GN_NONE);s->notice_click=1;return ACT_NOTICE_INPUT;
 }
 if(!down)return ACT_NONE;
 if(inside((Rect){s->width-128,4,120,24},s->mx,s->my))return ACT_EXIT;
 for(int i=0;i<s->slots;i++)if(inside((Rect){s->width-(s->slots==2?312:360)+i*(s->slots==2?92:56),3,s->slots==2?84:52,28},s->mx,s->my)){
  s->slot=i;scene_focus(s,i+2);return ACT_NEW;
 }
 if(s->my>=s->height-28){
  if(s->mx<146)scene_focus(s,0);else if(s->mx<292)scene_focus(s,1);
  else for(int i=0;i<s->slots;i++)if(s->mx>=300+i*(s->slots==2?146:(s->width-300)/4)&&s->mx<300+(i+1)*(s->slots==2?146:(s->width-300)/4)-10&&s->window[i+2].open)scene_focus(s,i+2);
  return ACT_NONE;
 }
 for(int n=s->slots+1;n>=0;n--){
  int id=s->order[n];GuiWindow *w=&s->window[id];
  if(!w->open||w->hidden||!inside(w->r,s->mx,s->my))continue;
  scene_focus(s,id);
  if(inside((Rect){w->r.x+w->r.w-25,w->r.y+6,18,18},s->mx,s->my)){w->open=0;scene_damage(s,covered(w));next_visible(s);if(id>=2){s->slot=id-2;return ACT_END;}return ACT_NONE;}
  if(inside((Rect){w->r.x+w->r.w-47,w->r.y+6,18,18},s->mx,s->my)){w->hidden=1;scene_damage(s,covered(w));next_visible(s);return ACT_NONE;}
  if(id>=2){
   s->slot=id-2;
   if(inside((Rect){w->r.x+w->r.w-84,w->r.y+2,32,26},s->mx,s->my)){
    place_window(s,id,s->placement[id-2]==1?0:1);return ACT_NONE;
   }
   if(inside((Rect){w->r.x+w->r.w-120,w->r.y+2,32,26},s->mx,s->my)){
    int mode=s->placement[id-2];
    place_window(s,id,mode==3?0:mode==2?3:2);return ACT_NONE;
   }
   if((s->command[id-2]||s->shell[id-2])&&s->my>=w->r.y+w->r.h-30){
    int offset=s->mx-w->r.x;
    if(offset>=206&&offset<266){s->key=s->command[id-2]?13:3;return ACT_KEY;}
    if(offset>=270&&offset<330){s->key=s->command[id-2]?27:4;return ACT_KEY;}
   }
   if(s->menu[id-2]&&inside((Rect){w->r.x+8,w->r.y+3,56,26},s->mx,s->my)){s->key=KG_MENU_KEY;return ACT_KEY;}
   if(!s->restored[id-2].w&&inside((Rect){w->r.x+w->r.w-18,w->r.y+w->r.h-18,18,18},s->mx,s->my)){
    s->resize=id+1;s->dx=w->r.x+w->r.w-s->mx;s->dy=w->r.y+w->r.h-s->my;return ACT_NONE;
   }
   if(inside((Rect){w->r.x+8,w->r.y+36,w->r.w-16,28},s->mx,s->my)){
    int b=(s->mx-w->r.x-8)/64;
    return b==0?ACT_RUN:b==1?ACT_PAUSE:b==2?ACT_RESUME:b==3?ACT_END:b==4?ACT_RESTART:ACT_NONE;
   }
   if(s->my>=w->r.y+w->r.h-30){
    int b=(s->mx-w->r.x-8)/48;
    if(b==0)s->scroll_x[id-2]=clamp(s->scroll_x[id-2]-8,0,79);
    if(b==1)s->scroll_x[id-2]=clamp(s->scroll_x[id-2]+8,0,79);
    if(b==2)s->scroll_y[id-2]=clamp(s->scroll_y[id-2]-4,0,24);
    if(b==3)s->scroll_y[id-2]=clamp(s->scroll_y[id-2]+4,0,24);
    scene_damage(s,w->r);return ACT_SCROLL;
   }
  }
  if(s->my<w->r.y+30){if(id>=2&&s->restored[id-2].w)return ACT_NONE;s->drag=id+1;s->dx=s->mx-w->r.x;s->dy=s->my-w->r.y;return ACT_NONE;}
  if(!id){
   int x=s->mx-w->r.x,y=s->my-w->r.y;
   if(s->help==LAUNCHER_GALLERY){
    if(inside((Rect){KGALLERY_BACK_X,KGALLERY_BACK_Y,KGALLERY_BACK_W,KGALLERY_BACK_H},x,y))launcher_page(s,LAUNCHER_GUIDE);
    return ACT_NONE;
   }
   if(s->help==LAUNCHER_GUIDE){
    if(inside(scene_launcher_button(0),x,y))launcher_page(s,LAUNCHER_APPS);
    if(inside(scene_launcher_button(1),x,y))launcher_page(s,LAUNCHER_GALLERY);
    return ACT_NONE;
   }
   if(inside((Rect){216,38,98,36},x,y))return ACT_MEMO;
   static const unsigned actions[LAUNCHER_BUTTONS]={0,ACT_WORDS,ACT_CALCULATOR,ACT_CLOCK,
    ACT_SHELL,ACT_FILES,ACT_EDITOR,ACT_IMAGE,ACT_SHELL32,ACT_CALENDAR};
   for(unsigned i=0;i<LAUNCHER_BUTTONS;i++){
    if(!inside(scene_launcher_button(i),x,y))continue;
    if(!i)launcher_page(s,LAUNCHER_GUIDE);
    return actions[i];
   }
  }
  if(id==1)for(int i=0;i<s->slots;i++){
   if(s->window[i+2].open&&inside((Rect){w->r.x+16,w->r.y+94+i*24,202,24},s->mx,s->my)){
    scene_focus(s,i+2);return ACT_NONE;
   }
  }
  if(id==1&&inside((Rect){w->r.x+16,w->r.y+168+(s->slots-2)*24,94,28},s->mx,s->my))return ACT_START;
  if(id==1&&inside((Rect){w->r.x+16,w->r.y+198+(s->slots-2)*24,202,24},s->mx,s->my))return ACT_SETTINGS;
  if(id==1&&inside((Rect){w->r.x+124,w->r.y+168+(s->slots-2)*24,94,28},s->mx,s->my))return ACT_STOP;
  break;
 }
 return ACT_NONE;
}

int scene_event4(Scene *s,const GuiEvent4 *e){
 if(e->base.type==GUI_TIMER){
  GuiWindow *w=&s->window[1];
  for(int i=2;i<s->slots;i++){
   if(w->open&&!w->hidden){
    if(s->states[i]!=e->extra_states[i-2])scene_window_damage(s,1,(Rect){w->r.x+32,w->r.y+98+i*24,76,16});
    if(s->counters[i]!=e->extra_counters[i-2])scene_window_damage(s,1,(Rect){w->r.x+108,w->r.y+98+i*24,120,16});
   }
   s->states[i]=e->extra_states[i-2];s->counters[i]=e->extra_counters[i-2];
  }
 }
 return scene_event(s,&e->base);
}
