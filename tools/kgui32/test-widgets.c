#include <assert.h>
#include <string.h>
#include "sdk/widgets.h"
int main(void){
 KwInput input={0};KgEvent e={.type=KG_KEY,.key='b'};
 assert(kw_input(&input,&e)==2);e.key=260;assert(kw_input(&input,&e)==1);e.key='a';kw_input(&input,&e);
 assert(!strcmp(input.text,"ab")&&input.cursor==1);e.key=264;kw_input(&input,&e);assert(!strcmp(input.text,"a"));
 e.key=8;kw_input(&input,&e);assert(!input.length&&!input.cursor&&!input.text[0]);
 for(int i=0;i<80;i++){e.key='x';kw_input(&input,&e);}assert(input.length==63&&input.text[63]==0);
 KwFocus f={.count=3};e.key=9;assert(kw_focus(&f,&e)&&f.current==1);kw_focus(&f,&e);kw_focus(&f,&e);assert(!f.current);e.control=KG_MOD_SHIFT;kw_focus(&f,&e);assert(f.current==2);e.control=0;
 KwList l={.count=100,.rows=5};e.key=263;kw_list(&l,(KwRect){0,0,80,80},&e);assert(l.selected==5&&l.top==1);
 e.key=261;kw_list(&l,(KwRect){0,0,80,80},&e);assert(l.selected==99&&l.top==95);
 l.count=0;kw_reveal(&l);assert(!l.selected&&!l.top);
 KwScroll s={.r={10,20,20,112},.total=100,.rows=5};
 e=(KgEvent){.type=KG_MOUSE,.x=15,.y=39,.key=1};assert(kw_scroll(&s,&e)&&s.dragging);
 e.type=KG_POINTER;e.x=-200;e.y=1000;assert(kw_scroll(&s,&e)&&s.top==95);
 e.key=0;assert(kw_scroll(&s,&e));assert(!s.dragging);
 e=(KgEvent){.type=KG_MOUSE,.x=15,.y=120,.key=1};kw_scroll(&s,&e);assert(s.top==95);
 s.total=2;s.top=0;e.y=40;kw_scroll(&s,&e);e.type=KG_POINTER;e.y=-100;kw_scroll(&s,&e);assert(!s.top);
 e=(KgEvent){.type=KG_FOCUS};kw_scroll(&s,&e);assert(!s.dragging);
 e=(KgEvent){.type=KG_MOUSE,.key=1,.control=900};assert(kw_dialog(&e));e.key=0;assert(!kw_dialog(&e));
 return 0;
}
