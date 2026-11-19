#include "stats.h"
#include "../sdk/calendar.h"
#include "../sdk/menu.h"
#include "ui-calendar.h"

static const KmItem menu[]={
 {"Previous month",KG_KEY,'p',0},
 {"Next month",KG_KEY,'n',0},
 {"Previous year",KG_KEY,'[',0},
 {"Next year",KG_KEY,']',0},
 {"Today",KG_KEY,'t',0},
 {"Close",KG_KEY,'q',0},
};

volatile KgStats calendar_stats __attribute__((section(".data.gui_stats")))={
 .magic=0x314c4143,.version=1
};

static void draw_calendar(unsigned year,unsigned month,unsigned today){
 unsigned width=kg_width(),height=kg_height();
 unsigned row_height=height<184?14:16;
 unsigned first=calendar_weekday(year,month),days=calendar_days(year,month);
 char title[]="0000-00";
 for(unsigned i=0,value=year;i<4;i++,value/=10)title[3-i]='0'+value%10;
 title[5]='0'+month/10;
 title[6]='0'+month%10;

 kg_begin();
 kg_rect(0,0,width,height,0xeaf0f5);
 kg_text(16,8,title,0x182c46);
 kg_text(16,32,"Su   Mo   Tu   We   Th   Fr   Sa",0x263e58);
 for(unsigned row=0;row<6;row++){
  char line[33];
  for(unsigned i=0;i<32;i++)line[i]=' ';
  line[32]=0;
  for(unsigned col=0;col<7;col++){
   unsigned cell=row*7+col,day=cell-first+1;
   if(cell<first||day>days)continue;
   if(day==today)kg_rect(14+col*40,50+row*row_height,22,16,0x98c9f1);
   if(day>=10)line[col*5]='0'+day/10;
   line[col*5+1]='0'+day%10;
  }
  kg_text(16,50+row*row_height,line,0x182c46);
 }
 kg_button('p',8,height-26,80,24,"Previous");
 kg_button('t',96,height-26,80,24,"Today");
 kg_button('n',184,height-26,80,24,"Next");
 kg_button('q',272,height-26,64,24,"Close");
 kg_menu_state(0x3fu
  &~(year==1980&&month==1?1u:0u)
  &~(year==2099&&month==12?2u:0u)
  &~(year==1980?4u:0u)&~(year==2099?8u:0u),0);
 calendar_stats.value=year*100+month;
 calendar_stats.seconds=today;
 calendar_stats.width=width;
 calendar_stats.height=height;
 int result=kg_present();
 calendar_stats.error=result<0?-result:0;
 if(!result)calendar_stats.frames++;
}

int main(void){
 kg_menu(menu,sizeof menu/sizeof *menu);
 kg_menu_labels(ku_labels,sizeof ku_labels/sizeof *ku_labels);
 int token=kg_open("Calendar",344,194);
 if(token<0)return 1;
 calendar_stats.token=(unsigned)token;
 unsigned year=0,month=0,day=0,shown_year=0,shown_month=0;
 int dirty=1;
 for(;;){
  KgEvent event;
  int result=kg_poll(&event);
  if(result<0)return 2;
  calendar_stats.heartbeat++;
  unsigned key=0;
  if(result){
   calendar_stats.events++;
   if(event.type==KG_KEY){key=event.key;calendar_stats.keys++;}
   if(event.type==KG_MOUSE&&(event.key&1)){key=event.control;calendar_stats.clicks++;}
   if(event.type==KG_RESIZE)dirty=1;
  }
  if(key=='q'||key=='Q'||key==27)return kg_close()?4:0;
  if(!year||key=='t'||key=='T'){
   if(kg_date(&year,&month,&day)<0)return 3;
   shown_year=year;shown_month=month;dirty=1;
  }
  int delta=key=='p'||key=='P'||key==258?-1:key=='n'||key=='N'||key==259?1:key=='['?-12:key==']'?12:0;
  if(delta&&calendar_move(&shown_year,&shown_month,delta))dirty=1;
  if(dirty){
   draw_calendar(shown_year,shown_month,shown_year==year&&shown_month==month?day:0);
   dirty=calendar_stats.error==22||calendar_stats.error==11;
   if(calendar_stats.error&&!dirty)return 5;
  }
  k32_wait(K32_WAIT_TIME,100);
 }
}
