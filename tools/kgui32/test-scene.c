#include <assert.h>
#include "scene.h"
static void mouse(Scene *s,int type,int x,int y,int buttons){
 GuiEvent e={.type=type,.x=x,.y=y,.buttons=buttons};scene_event(s,&e);
}
static void key(Scene *s,int k){GuiEvent e={.type=GUI_KEY,.key=k};scene_event(s,&e);}
static void check(int width,int height){
 Scene s;scene_init(&s,width,height);s.damage=(Rect){0};
 /* A release can carry the last motion: never leave a stuck drag or an old
  * rectangle, including odd indexed-colour coordinates. */
 mouse(&s,GUI_BUTTON,70,75,1);assert(s.drag==1);
 mouse(&s,GUI_BUTTON,83,84,0);
 assert(s.drag==0&&s.window[0].r.x==61&&s.window[0].r.y==73);
 assert(s.damage.x<=48&&s.damage.y<=64);
 assert(s.damage.x+s.damage.w>=407&&s.damage.y+s.damage.h>=303);
 /* Clamp both edges and retain space for taskbar/shadows. */
 mouse(&s,GUI_BUTTON,83,84,1);mouse(&s,GUI_MOTION,10000,10000,1);
 assert(s.window[0].r.x==width-348&&s.window[0].r.y==height-264);
 mouse(&s,GUI_MOTION,0,0,1);mouse(&s,GUI_BUTTON,0,0,0);
 assert(s.window[0].r.x==8&&s.window[0].r.y==40&&!s.drag);
 key(&s,9);assert(s.active==1);key(&s,258);
 assert(s.window[1].r.x==width-280);
 /* Minimize, restore and close/reopen through the actual taskbar hit test. */
 int x=s.window[1].r.x,y=s.window[1].r.y;
 mouse(&s,GUI_BUTTON,x+203,y+12,1);mouse(&s,GUI_BUTTON,x+203,y+12,0);
 assert(s.window[1].hidden);
 mouse(&s,GUI_BUTTON,180,height-16,1);mouse(&s,GUI_BUTTON,180,height-16,0);
 assert(!s.window[1].hidden&&s.active==1);
 mouse(&s,GUI_BUTTON,x+225,y+12,1);mouse(&s,GUI_BUTTON,x+225,y+12,0);
 assert(!s.window[1].open);key(&s,9);key(&s,9);assert(s.window[1].open);
 /* GUI input never falls through to workers; only explicit proof actions. */
 GuiEvent e={.type=GUI_KEY,.key='a'};assert(scene_event(&s,&e)==ACT_NONE);
 e.key='t';assert(scene_event(&s,&e)==ACT_START);
 e.key='s';assert(scene_event(&s,&e)==ACT_STOP);
 e.key='q';assert(scene_event(&s,&e)==ACT_EXIT);
 s.damage=(Rect){0};e=(GuiEvent){.type=GUI_TIMER,.counters={123,456},.states={1,1}};scene_event(&s,&e);
 assert(s.damage.w==196&&s.damage.h==40);
 s.damage=(Rect){0};scene_event(&s,&e);assert(s.damage.w==0);
 e.counters[1]++;scene_event(&s,&e);assert(s.damage.w==120&&s.damage.h==16);
 s.damage=(Rect){0};scene_damage(&s,(Rect){-4,-9,20,30});
 assert(s.damage.x==0&&s.damage.y==0&&s.damage.w==16&&s.damage.h==21);
 scene_damage(&s,(Rect){width-2,height-3,50,50});
 assert(s.damage.x+s.damage.w==width&&s.damage.y+s.damage.h==height);
}
static void sessions(int width,int height){
 Scene s;scene_init(&s,width,height);
 GuiEvent e={.type=GUI_BUTTON,.x=width-280,.y=15,.buttons=1};
 assert(scene_event(&s,&e)==ACT_NEW&&s.slot==0&&s.active==2);
 assert(s.window[2].open&&s.order[3]==2);
 s.damage=(Rect){0};GuiEvent timer={.type=GUI_TIMER,.counters={1,0}};scene_event(&s,&timer);
 if(width==640)assert(!s.damage.w);
 else assert(s.damage.x==672&&s.damage.w==84&&s.damage.h==16);
 e.buttons=0;scene_event(&s,&e);
 e=(GuiEvent){.type=GUI_KEY,.key='q'};assert(scene_event(&s,&e)==ACT_KEY&&s.key=='q'&&s.slot==0);
 e.key='t';assert(scene_event(&s,&e)==ACT_KEY);e.key=12;assert(scene_event(&s,&e)==ACT_RUN);
 e.key=16;assert(scene_event(&s,&e)==ACT_PAUSE);e.key=18;assert(scene_event(&s,&e)==ACT_RESUME);
 e.key=5;assert(scene_event(&s,&e)==ACT_END);
 scene_focus(&s,3);assert(s.order[3]==3&&s.order[2]==2);
 Rect r=s.window[3].r;
 mouse(&s,GUI_BUTTON,r.x+r.w-8,r.y+r.h-8,1);assert(s.resize==4);
 mouse(&s,GUI_BUTTON,r.x+400-8,r.y+240-8,0);
 assert(!s.resize&&s.window[3].r.w==400&&s.window[3].r.h==240);
 r=s.window[3].r;
 mouse(&s,GUI_BUTTON,r.x+r.w-40,r.y+12,1);mouse(&s,GUI_BUTTON,r.x+r.w-40,r.y+12,0);
 assert(s.window[3].hidden&&s.active==2);
 mouse(&s,GUI_BUTTON,480,height-16,1);mouse(&s,GUI_BUTTON,480,height-16,0);
 assert(!s.window[3].hidden&&s.active==3);
 e=(GuiEvent){.type=GUI_BUTTON,.x=r.x+r.w-16,.y=r.y+12,.buttons=1};
 assert(scene_event(&s,&e)==ACT_END&&s.slot==1&&!s.window[3].open&&s.active==2);
 e=(GuiEvent){.type=GUI_TIMER,.states={2,3},.counters={12,24}};scene_event(&s,&e);
 assert(s.states[0]==2&&s.states[1]==3&&s.active==2);
}
int main(void){check(640,400);check(800,600);sessions(640,400);sessions(800,600);return 0;}
