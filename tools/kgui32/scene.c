#include "scene.h"
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
void scene_init(Scene *s,int width,int height){
 *s=(Scene){.width=width,.height=height,.mx=320,.my=200,.active=0,.order={3,2,1,0},.slots=2};
 s->window[0]=(GuiWindow){{48,64,340,224},1,0};
 s->window[1]=(GuiWindow){{width-272,120,244,224},1,0};
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
int scene_event(Scene *s,const GuiEvent *e){
 if(e->type==GUI_TIMER){
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
  if(e->key==11||e->key==281){scene_focus(s,0);return ACT_NONE;}
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
   if(inside((Rect){w->r.x+w->r.w-18,w->r.y+w->r.h-18,18,18},s->mx,s->my)){
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
  if(s->my<w->r.y+30){s->drag=id+1;s->dx=s->mx-w->r.x;s->dy=s->my-w->r.y;return ACT_NONE;}
  if(!id){
   if(inside((Rect){w->r.x+18,w->r.y+116,140,28},s->mx,s->my))return ACT_CALCULATOR;
   if(inside((Rect){w->r.x+174,w->r.y+116,140,28},s->mx,s->my))return ACT_CLOCK;
   if(inside((Rect){w->r.x+18,w->r.y+152,140,28},s->mx,s->my))return ACT_SHELL;
   if(inside((Rect){w->r.x+174,w->r.y+152,140,28},s->mx,s->my))return ACT_FILES;
   if(inside((Rect){w->r.x+18,w->r.y+188,140,28},s->mx,s->my))return ACT_EDITOR;
   if(inside((Rect){w->r.x+174,w->r.y+188,140,28},s->mx,s->my))return ACT_IMAGE;
   if(inside((Rect){w->r.x+174,w->r.y+88,140,28},s->mx,s->my))return ACT_WORDS;
  }
  if(id==1&&inside((Rect){w->r.x+16,w->r.y+168+(s->slots-2)*24,94,28},s->mx,s->my))return ACT_START;
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
