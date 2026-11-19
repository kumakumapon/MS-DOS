#include <assert.h>
#include "sdk/menu.h"
#include "sdk/stopwatch.h"
#include "sdk/calendar.h"
static int send(KmMenu *m,unsigned type,unsigned key,unsigned id){KgEvent e={.type=type,.key=key,.control=id,.seconds=123};return km_event(m,&e,168);}
int main(void){
 KmItem items[KM_MAX];for(unsigned i=0;i<KM_MAX;i++)items[i]=(KmItem){"Command",KG_KEY,100+i,2};
 KmMenu m={.items=items,.count=KM_MAX,.enabled=1};
 assert(send(&m,KG_KEY,KG_MENU_KEY,0)==1&&m.open&&m.rows==2);
 assert(send(&m,KG_MOUSE,2,KM_ITEM)==1&&m.open); /* Right click never selects. */
 assert(send(&m,KG_MOUSE,0,KM_ITEM)==1&&m.open); /* Release never selects. */
 assert(send(&m,KG_MOUSE,1,KM_ITEM+3)==1&&m.open); /* Hidden page cannot execute. */
 assert(send(&m,KG_MOUSE,1,KM_NEXT)==1&&m.selected==2);
 KgEvent e={.type=KG_MOUSE,.key=1,.control=KM_ITEM+3,.seconds=456};
 assert(km_event(&m,&e,168)==2&&!m.open&&e.type==KG_KEY&&e.key==103&&e.control==2&&e.seconds==456&&e.x==-1);
 send(&m,KG_KEY,280,0);send(&m,KG_KEY,256,0);assert(m.selected==23);
 e=(KgEvent){.type=KG_KEY,.key=13};assert(km_event(&m,&e,168)==2&&e.key==123&&!m.open);
 send(&m,KG_KEY,280,0);send(&m,KG_KEY,27,0);assert(!m.open);
 send(&m,KG_KEY,280,0);send(&m,KG_FOCUS,0,0);assert(!m.open);
 send(&m,KG_KEY,280,0);assert(!send(&m,KG_QUIT,1,0)&&!m.open);
 send(&m,KG_KEY,280,0);m.enabled=0;send(&m,KG_MOUSE,1,KM_ITEM);assert(m.open);
 send(&m,KG_MOUSE,1,KM_BACK);assert(!m.open);
 m.enabled=1;send(&m,KG_KEY,280,0);send(&m,KG_MOUSE,1,KM_NEXT);send(&m,KG_RESIZE,0,0);assert(m.selected==2);
 assert(send(&m,KG_MOUSE,1,KM_PREV)==1&&m.selected==0);
 m.disabled=1;
 send(&m,KG_MOUSE,1,KM_ITEM);assert(m.open);
 send(&m,KG_KEY,13,0);assert(m.open);
 send(&m,KG_MOUSE,1,KM_ITEM+1);assert(!m.open);
 unsigned year=1999,month=12;
 assert(calendar_move(&year,&month,1)&&year==2000&&month==1);
 assert(calendar_days(2000,2)==29&&calendar_days(2001,2)==28);
 assert(calendar_weekday(2000,1)==6&&calendar_weekday(2024,2)==4);
 year=1980;assert(!calendar_move(&year,&month,-1)&&year==1980&&month==1);
 m.count=KM_MAX+1;assert(!send(&m,KG_KEY,280,0));
 KmStopwatch s={0};km_stopwatch_tick(&s,86398);s.running=1;km_stopwatch_tick(&s,86399);km_stopwatch_tick(&s,1);assert(s.elapsed==3);
 s.running=0;km_stopwatch_tick(&s,20);assert(s.elapsed==3);s.running=1;km_stopwatch_tick(&s,22);assert(s.elapsed==5);
 s.elapsed=359998;km_stopwatch_tick(&s,25);assert(s.elapsed==359999);km_stopwatch_tick(&s,90000);assert(s.last==25);
 return 0;
}
