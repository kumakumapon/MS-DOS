#include "stats.h"
#include "../sdk/calendar.h"
#include "../sdk/menu.h"
#include "../sdk/widgets.h"
#include "ui-calendar.h"

static const KmItem menu[]={
 {"Previous month",KG_KEY,'p',0},
 {"Next month",KG_KEY,'n',0},
 {"Previous year",KG_KEY,'[',0},
 {"Next year",KG_KEY,']',0},
 {"Today",KG_KEY,'t',0},
 {"Close",KG_KEY,'q',0},
};
static const unsigned keys[]={'p','t','n','q',263};
static KwFocus focus;
static unsigned week,week_rows;

volatile KgStats calendar_stats __attribute__((section(".data.gui_stats")))={
 .magic=0x314c4143,.version=1
};

static void draw_calendar(unsigned year,unsigned month,unsigned today){
 unsigned width=kg_width(),height=kg_height();
 unsigned large=kg_large_text()&&width>=336,row_height=large?32:height<184?14:16;
 week_rows=large?(height-82)/32:6;if(!week_rows)week_rows=1;if(week_rows>6)week_rows=6;
 unsigned first_row=large?week:0;
 if(first_row+week_rows>6)first_row=6-week_rows;
 week=first_row;focus.count=week_rows<6?5:4;if(focus.current>=focus.count)focus.current=0;
 unsigned first=calendar_weekday(year,month),days=calendar_days(year,month);
 char title[]="0000-00";
 for(unsigned i=0,value=year;i<4;i++,value/=10)title[3-i]='0'+value%10;
 title[5]='0'+month/10;
 title[6]='0'+month%10;

 kg_begin();
 kg_rect(0,0,width,height,0xeaf0f5);
 kw_readable(16,kg_large_text()?0:8,title,128,0x182c46);
 unsigned pitch=large?48:40,cells=large?3:5;
 static const char *days_short[]={"Su","Mo","Tu","We","Th","Fr","Sa"};
 for(unsigned col=0;col<7;col++)kg_text(16+col*pitch,34,days_short[col],0x263e58);
 for(unsigned row=0;row<week_rows;row++){
  char line[33];
  unsigned length=6*cells+2;
  for(unsigned i=0;i<length;i++)line[i]=' ';
  line[length]=0;
  for(unsigned col=0;col<7;col++){
   unsigned cell=(first_row+row)*7+col,day=cell-first+1;
   if(cell<first||day>days)continue;
   if(day==today)kg_rect(14+col*pitch,50+row*row_height,large?36:22,row_height,0x98c9f1);
   if(day>=10)line[col*cells]='0'+day/10;
   line[col*cells+1]='0'+day%10;
  }
  if(large)kg_text_double(16,50+row*row_height,line,0x182c46);
  else kg_text(16,50+row*row_height,line,0x182c46);
 }
 const char *labels[]={"Previous","Today","Next","Close"};
 int button_width=(width-18)/4;
 for(unsigned i=0;i<4;i++)kw_draw_button((KwRect){8+i*button_width,(int)height-26,button_width-4,24},keys[i],KU(labels[i]),KW_SECONDARY,focus.current==i?KW_FOCUSED:0);
 if(week_rows<6)kw_draw_button((KwRect){(int)width-108,2,100,28},keys[4],KU("More weeks"),KW_SECONDARY,focus.current==4?KW_FOCUSED:0);
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
 focus.count=4;
 kg_menu(menu,sizeof menu/sizeof *menu);
 kg_menu_labels(ku_labels,sizeof ku_labels/sizeof *ku_labels);
 int token=kg_open("Calendar",344,194);
 if(token<0)return 1;
 kg_subscribe(KG_INPUT_TAB|KG_INPUT_MODIFIERS);
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
   if(event.type==KG_KEY){
    key=event.key;calendar_stats.keys++;
    if(kw_focus(&focus,&event))dirty=1;
    if(key==13)key=keys[focus.current];
   }
   if(event.type==KG_MOUSE&&(event.key&1)){key=event.control;calendar_stats.clicks++;}
   if(event.type==KG_RESIZE)dirty=1;
  }
  if(key=='q'||key=='Q'||key==27)return kg_close()?4:0;
  if(key==262||key==263){week=key==262||week+week_rows>=6?0:week+week_rows;dirty=1;}
  if(!year||key=='t'||key=='T'){
   if(kg_date(&year,&month,&day)<0)return 3;
   shown_year=year;shown_month=month;week=0;dirty=1;
  }
  int delta=key=='p'||key=='P'||key==258?-1:key=='n'||key=='N'||key==259?1:key=='['?-12:key==']'?12:0;
  if(delta&&calendar_move(&shown_year,&shown_month,delta)){week=0;dirty=1;}
  if(dirty){
   draw_calendar(shown_year,shown_month,shown_year==year&&shown_month==month?day:0);
   dirty=calendar_stats.error==22||calendar_stats.error==11;
   if(calendar_stats.error&&!dirty)return 5;
  }
  k32_wait(K32_WAIT_TIME,100);
 }
}
