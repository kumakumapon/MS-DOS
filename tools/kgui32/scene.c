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
void scene_init(Scene *s,int width,int height){
 *s=(Scene){.width=width,.height=height,.mx=320,.my=200,.active=0};
 s->window[0]=(GuiWindow){{48,64,340,224},1,0};
 s->window[1]=(GuiWindow){{width-272,120,244,224},1,0};
 scene_damage(s,(Rect){0,0,width,height});
}
static void focus(Scene *s,int id){
 for(int i=0;i<2;i++)if(s->window[i].open&&!s->window[i].hidden)scene_damage(s,covered(&s->window[i]));
 s->active=id;s->window[id].open=1;s->window[id].hidden=0;scene_damage(s,covered(&s->window[id]));
}
static void move(Scene *s,int id,int x,int y){
 GuiWindow *w=&s->window[id];scene_damage(s,covered(w));
 w->r.x=clamp(x,8,s->width-w->r.w-8);w->r.y=clamp(y,40,s->height-32-w->r.h-8);
 scene_damage(s,covered(w));
}
int scene_event(Scene *s,const GuiEvent *e){
 if(e->type==GUI_TIMER){
  int changed=0;
  for(int i=0;i<2;i++){changed|=s->counters[i]!=e->counters[i]||s->states[i]!=e->states[i];s->counters[i]=e->counters[i];s->states[i]=e->states[i];}
  if(changed&&s->window[1].open&&!s->window[1].hidden)scene_damage(s,covered(&s->window[1]));
  return ACT_NONE;
 }
 if(e->type==GUI_KEY){
  if(e->key==9){focus(s,1-s->active);return ACT_NONE;}
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
 if(s->drag){move(s,s->drag-1,s->mx-s->dx,s->my-s->dy);if(!(s->buttons&1))s->drag=0;return ACT_NONE;}
 if(!down)return ACT_NONE;
 if(inside((Rect){s->width-128,4,120,24},s->mx,s->my))return ACT_EXIT;
 if(s->my>=s->height-28){if(s->mx<146)focus(s,0);else if(s->mx<292)focus(s,1);return ACT_NONE;}
 for(int n=0;n<2;n++){
  int id=n?1-s->active:s->active;GuiWindow *w=&s->window[id];
  if(!w->open||w->hidden||!inside(w->r,s->mx,s->my))continue;
  focus(s,id);
  if(inside((Rect){w->r.x+w->r.w-25,w->r.y+6,18,18},s->mx,s->my)){w->open=0;scene_damage(s,covered(w));return ACT_NONE;}
  if(inside((Rect){w->r.x+w->r.w-47,w->r.y+6,18,18},s->mx,s->my)){w->hidden=1;scene_damage(s,covered(w));return ACT_NONE;}
  if(s->my<w->r.y+30){s->drag=id+1;s->dx=s->mx-w->r.x;s->dy=s->my-w->r.y;return ACT_NONE;}
  if(id==1&&inside((Rect){w->r.x+16,w->r.y+168,94,28},s->mx,s->my))return ACT_START;
  if(id==1&&inside((Rect){w->r.x+124,w->r.y+168,94,28},s->mx,s->my))return ACT_STOP;
  break;
 }
 return ACT_NONE;
}
