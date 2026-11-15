#include "stats.h"
#include "ui-clock.h"
#include "../sdk/stopwatch.h"
static KmStopwatch stopwatch;static unsigned watch;
#include "../sdk/menu.h"
static const KmItem app_menu[]={
 {"12 / 24 hour",KG_KEY,'t',0},
 {"Stopwatch",KG_KEY,'w',0},
 {"Start / stop",KG_KEY,'s',0},
 {"Reset stopwatch",KG_KEY,'r',0},
 {"Close",KG_KEY,'q',0},
};
volatile KgStats clock_stats __attribute__((section(".data.gui_stats")))={.magic=0x324b474b,.version=1};
static void two(char *p,unsigned v){p[0]='0'+v/10;p[1]='0'+v%10;}
static int draw(unsigned seconds){
 unsigned w=kg_width(),h=kg_height(),hour=seconds/3600;char text[12]="00:00:00";
 if(watch){seconds=stopwatch.elapsed;hour=seconds/3600;}
 else if(clock_stats.mode){hour%=12;if(!hour)hour=12;}
 two(text,hour);two(text+3,seconds/60%60);two(text+6,seconds%60);
 kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kg_rect(12,12,w-24,48,0x182c46);
 kg_text((w-64)/2,28,text,0xffffff);
 kg_label(20,72,watch?(stopwatch.running?"Stopwatch / running":"Stopwatch / stopped"):"PC-98 / DOS wall clock",0x263e58);
 kg_button('w',w-100,68,80,28,watch?"Clock":"Watch");
 kg_button(watch?'s':'t',20,104,132,28,watch?(stopwatch.running?"Stop":"Start"):clock_stats.mode?"24-hour":"12-hour");if(watch)kg_button('r',164,104,64,28,"Reset");kg_button('q',w-100,104,80,28,"Close");
 int e=kg_present();clock_stats.error=e<0?-e:0;if(!e)clock_stats.frames++;
 clock_stats.width=w;clock_stats.height=h;clock_stats.seconds=seconds;clock_stats.value=stopwatch.elapsed;clock_stats.reserved[0]=watch;clock_stats.reserved[1]=stopwatch.running;return e;
}
int main(int argc,char **argv){
 kg_menu(app_menu,sizeof app_menu/sizeof *app_menu);
 kg_menu_labels(ku_labels,sizeof ku_labels/sizeof *ku_labels);
 int token=kg_open("Clock",328,160);if(token<0){clock_stats.error=-token;return 1;}clock_stats.token=token;unsigned previous=~0u;
 for(;;){
  KgEvent event;int got=kg_poll(&event);if(got<0){clock_stats.error=-got;return 2;}clock_stats.heartbeat++;
  unsigned elapsed=stopwatch.elapsed;km_stopwatch_tick(&stopwatch,event.seconds);
  int dirty=elapsed!=stopwatch.elapsed;unsigned k=0;
  if(got){clock_stats.events++;
   if(event.type==KG_KEY){k=event.key;clock_stats.keys++;}
   if(event.type==KG_MOUSE&&(event.key&1)){k=event.control;clock_stats.clicks++;}
   if(event.type==KG_RESIZE)dirty=1;
  }
  if(k=='q'||k=='Q')break;
  if(k=='t'||k=='T'){clock_stats.mode^=1;dirty=1;}
  if(k=='w'||k=='W'){watch^=1;dirty=1;}
  if(k=='s'||k=='S'){stopwatch.running^=1;watch=1;dirty=1;}
  if(k=='r'||k=='R'){stopwatch.elapsed=0;dirty=1;}
  if(previous!=event.seconds||dirty){previous=event.seconds;int e=draw(previous);if(e==-22)previous=~0u;else if(e<0)return 3;}
  if(argc>1&&same(argv[1],"fault"))*(volatile unsigned *)0x10000=1;
  k32_wait(K32_WAIT_TIME,100);
 }
 return kg_close()?4:0;
}
