#include <assert.h>
#include "scene.h"
#include "shell-ui.h"
#include "layout.h"
#include "sdk/protocol.h"
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
 assert(s.damage.x+s.damage.w>=s.window[0].r.x+s.window[0].r.w+6&&s.damage.y+s.damage.h>=s.window[0].r.y+s.window[0].r.h+6);
 /* Clamp both edges and retain space for taskbar/shadows. */
 mouse(&s,GUI_BUTTON,83,84,1);mouse(&s,GUI_MOTION,10000,10000,1);
 assert(s.window[0].r.x+s.window[0].r.w==width-8&&s.window[0].r.y+s.window[0].r.h==height-40);
 mouse(&s,GUI_MOTION,0,0,1);mouse(&s,GUI_BUTTON,0,0,0);
 assert(s.window[0].r.x==8&&s.window[0].r.y==40&&!s.drag);
 key(&s,9);assert(s.active==1);key(&s,258);
 assert(s.window[1].r.x==width-260);
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
static void launcher(int width,int height){
 Scene s;scene_init(&s,width,height);
 Rect w=s.window[0].r,system=s.window[1].r;
 assert(w.x+w.w+6<=system.x&&system.x+system.w+6<=width);
 assert(w.y+w.h+6<=height-32&&system.y+system.h+6<=height-32);
 scene_slots(&s,4);assert(s.window[1].r.y+s.window[1].r.h+6<=height-32);
 for(unsigned i=0;i<LAUNCHER_BUTTONS;i++){
  Rect r=scene_launcher_button(i);
  assert(r.x>=0&&r.x+r.w<=w.w&&r.y+r.h<=LAUNCHER_HINT_Y);
  for(unsigned j=0;j<i;j++){
   Rect q=scene_launcher_button(j);
   assert(r.x>=q.x+q.w||q.x>=r.x+r.w||r.y>=q.y+q.h||q.y>=r.y+r.h);
  }
 }
 key(&s,280);assert(s.help);key(&s,27);assert(!s.help);
 key(&s,280);
 Rect components=scene_launcher_button(1);
 mouse(&s,GUI_BUTTON,w.x+components.x+10,w.y+components.y+10,1);
 mouse(&s,GUI_BUTTON,w.x+components.x+10,w.y+components.y+10,0);
 assert(s.help==LAUNCHER_GALLERY);
 key(&s,27);assert(s.help==LAUNCHER_GUIDE);
 key(&s,281);assert(s.help==LAUNCHER_APPS);
}
static void notices(void){
 Scene s;scene_init(&s,640,400);s.damage=(Rect){0};
 const u32 start=0xfa0000,end=(start+4*307200u)&0xffffff;
 Rect r=scene_notice_rect(&s);
 assert(r.x>=0&&r.y>=32&&r.x+r.w<=640&&r.y+r.h<=400-32);
 scene_notice_tick(&s,start);scene_notice_set(&s,GN_WORKERS_STARTED);
 assert(s.notice==GN_WORKERS_STARTED&&s.damage.x==r.x&&s.damage.y==r.y&&s.damage.w==r.w&&s.damage.h==r.h);
 scene_notice_tick(&s,start-1);assert(s.notice==GN_WORKERS_STARTED);
 s.damage=(Rect){0};scene_notice_tick(&s,end-1);assert(!s.damage.w);
 scene_notice_tick(&s,end);assert(s.notice==GN_NONE&&s.damage.w==r.w&&s.damage.h==r.h);

 scene_focus(&s,2);scene_notice_set(&s,GN_APP_FAULTED);
 GuiEvent click={.type=GUI_BUTTON,.x=r.x+16,.y=r.y+16,.buttons=1};
 assert(scene_event(&s,&click)==ACT_NOTICE_INPUT&&s.notice==GN_NONE&&s.active==2);
 click.buttons=0;assert(scene_event(&s,&click)==ACT_NOTICE_INPUT&&!s.notice_click);
}
static void sessions(int width,int height){
 Scene s;scene_init(&s,width,height);
 GuiEvent e={.type=GUI_BUTTON,.x=width-280,.y=15,.buttons=1};
 assert(scene_event(&s,&e)==ACT_NEW&&s.slot==0&&s.active==2);
 assert(s.window[2].open&&s.order[3]==2);
 s.damage=(Rect){0};GuiEvent timer={.type=GUI_TIMER,.counters={1,0}};scene_event(&s,&timer);
 if(width==640)assert(!s.damage.w);
 else assert(s.damage.x==672&&s.damage.w==s.window[1].r.x+228-672&&s.damage.h==16);
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
static void clients(int width,int height){
 Scene s;scene_init(&s,width,height);scene_focus(&s,2);s.graphical[0]=1;
 /* App letters are delivered to the app; launcher shortcuts are consumed
  * even when both app windows are open and the launcher is minimized. */
 GuiEvent e={.type=GUI_KEY,.key='t'};
 assert(scene_event(&s,&e)==ACT_KEY&&s.slot==0&&s.key=='t');
 s.menu[0]=1;scene_focus(&s,2);
 e=(GuiEvent){.type=GUI_BUTTON,.x=s.window[2].r.x+20,.y=s.window[2].r.y+16,.buttons=1};
 assert(scene_event(&s,&e)==ACT_KEY&&s.key==KG_MENU_KEY&&!s.drag&&s.slot==0);
 e.buttons=0;scene_event(&s,&e);e.type=GUI_KEY;
 s.window[0].hidden=1;e.key=281;
 assert(scene_event(&s,&e)==ACT_NONE&&s.active==0&&!s.window[0].hidden);
 scene_focus(&s,3);e.key=11;
 assert(scene_event(&s,&e)==ACT_NOTICE_INPUT&&s.shell_page);
 key(&s,281);assert(s.active==0&&s.order[3]==0);
 Rect r=s.window[0].r;
 e=(GuiEvent){.type=GUI_BUTTON,.x=r.x+40,.y=r.y+128,.buttons=1};
 assert(scene_event(&s,&e)==ACT_CALCULATOR);
 mouse(&s,GUI_BUTTON,e.x,e.y,0);e.x=r.x+190;
 assert(scene_event(&s,&e)==ACT_CLOCK);
 mouse(&s,GUI_BUTTON,e.x,e.y,0);e.x=r.x+40;e.y=r.y+164;
 assert(scene_event(&s,&e)==ACT_SHELL);
 mouse(&s,GUI_BUTTON,e.x,e.y,0);e.y=r.y+236;assert(scene_event(&s,&e)==ACT_SHELL32);
 mouse(&s,GUI_BUTTON,e.x,e.y,0);
 scene_focus(&s,2);Rect original=s.window[2].r;
 mouse(&s,GUI_BUTTON,original.x+original.w-70,original.y+12,1);
 mouse(&s,GUI_BUTTON,original.x+original.w-70,original.y+12,0);
 Rect maximized=s.window[2].r;
 assert(maximized.w==width-16&&maximized.h<=520&&s.restored[0].w==original.w);
 mouse(&s,GUI_BUTTON,maximized.x+maximized.w-70,maximized.y+12,1);
 mouse(&s,GUI_BUTTON,maximized.x+maximized.w-70,maximized.y+12,0);
 r=s.window[2].r;assert(r.x==original.x&&r.y==original.y&&r.w==original.w&&r.h==original.h);
 for(unsigned transition=0;transition<3;transition++){
  Rect frame=s.window[2].r;
  mouse(&s,GUI_BUTTON,frame.x+frame.w-106,frame.y+12,1);
  mouse(&s,GUI_BUTTON,frame.x+frame.w-106,frame.y+12,0);
  if(!transition)assert(s.placement[0]==2&&s.window[2].r.x==8);
  else if(transition==1)assert(s.placement[0]==3&&s.window[2].r.x+s.window[2].r.w==width-8);
  else assert(s.placement[0]==0&&s.window[2].r.x==original.x);
 }
 s.shell[0]=1;
 e=(GuiEvent){.type=GUI_BUTTON,.x=r.x+230,.y=r.y+r.h-16,.buttons=1};
 assert(scene_event(&s,&e)==ACT_KEY&&s.key==3);
 e.buttons=0;scene_event(&s,&e);
 s.command[0]=1;e.x=r.x+292;e.buttons=1;
 assert(scene_event(&s,&e)==ACT_KEY&&s.key==27);
 e.buttons=0;scene_event(&s,&e);s.command[0]=s.shell[0]=0;
 /* Client controls remain usable at the minimum size. Text windows keep
  * their previous smaller minimum, including a final motion on release. */
 scene_focus(&s,2);r=s.window[2].r;
 mouse(&s,GUI_BUTTON,r.x+r.w-8,r.y+r.h-8,1);
 assert(s.resize==3);mouse(&s,GUI_BUTTON,r.x+100,r.y+100,0);
 assert(!s.resize&&s.window[2].r.w==360&&s.window[2].r.h==272);
 scene_focus(&s,3);r=s.window[3].r;
 mouse(&s,GUI_BUTTON,r.x+r.w-8,r.y+r.h-8,1);
 mouse(&s,GUI_BUTTON,r.x+100,r.y+100,0);
 assert(!s.resize&&s.window[3].r.h==208);
}
static void four(int width,int height){
 Scene s;scene_init(&s,width,height);scene_slots(&s,4);assert(s.slots==4&&s.window[1].r.h==272);
 for(int i=0;i<4;i++){
  GuiEvent e={.type=GUI_BUTTON,.x=width-360+i*56+24,.y=15,.buttons=1};assert(scene_event(&s,&e)==ACT_NEW&&s.slot==i&&s.active==i+2&&s.order[5]==i+2);
  e.buttons=0;scene_event(&s,&e);
 }
 for(int i=0;i<4;i++){int step=(width-300)/4;mouse(&s,GUI_BUTTON,300+i*step+20,height-16,1);mouse(&s,GUI_BUTTON,300+i*step+20,height-16,0);assert(s.active==i+2);}
 GuiEvent4 timer={.base={.type=GUI_TIMER,.states={1,2},.counters={10,20}},.extra_states={3,4},.extra_counters={30,40}};
 assert(scene_event4(&s,&timer)==ACT_NONE&&s.states[3]==4&&s.counters[2]==30);
 scene_focus(&s,5);key(&s,16);assert(s.slot==3);scene_close(&s,5);assert(s.active!=5&&!s.window[5].open);
}
static void shell_and_layout(void){
 Scene s;scene_init(&s,800,600);
 mouse(&s,GUI_BUTTON,40,16,1);mouse(&s,GUI_BUTTON,40,16,0);
 assert(s.shell_page==SHELL_KUMA);
 key(&s,11);assert(s.shell_page==SHELL_SEARCH);
 for(const char *q="kgclock";*q;q++)key(&s,*q);
 assert(shell_count(&s)==1&&shell_item(&s,0)==ACT_CLOCK);
 GuiEvent enter={.type=GUI_KEY,.key=13};
 assert(scene_event(&s,&enter)==ACT_CLOCK&&s.shell_page==SHELL_CLOSED);
 scene_focus(&s,2);scene_focus(&s,3);
 GuiEvent alt_tab={.type=GUI_KEY,.key=9,.buttons=KG_MOD_ALT};
 assert(scene_event(&s,&alt_tab)==ACT_NOTICE_INPUT&&s.shell_page==SHELL_SWITCH);
 GuiEvent timer={.type=GUI_TIMER};scene_event(&s,&timer);
 assert(!s.shell_page&&s.active==2);
 s.window[2].r=(Rect){385,120,400,280};
 WindowLayout layout={0};layout_validate(&layout);
 assert(layout_remember(&layout,&s,2,"KGCALC.EXE"));
 assert(!layout_remember(&layout,&s,2,"KGCALC.EXE"));
 Scene pegc;scene_init(&pegc,640,400);
 assert(layout_restore(&layout,&pegc,3,"KGCALC.EXE"));
 assert(pegc.window[3].r.x==232&&pegc.window[3].r.y==80);
 assert(!layout_restore(&layout,&pegc,2,"KGCLOCK.EXE"));
}
int main(void){
 shell_and_layout();four(640,400);four(800,600);launcher(640,400);launcher(800,600);notices();check(640,400);check(800,600);sessions(640,400);sessions(800,600);clients(640,400);clients(800,600);return 0;}
