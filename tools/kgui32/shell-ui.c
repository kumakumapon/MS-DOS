#include "shell-ui.h"
#include "sdk/protocol.h"

static const ShellApp apps[] = {
 {ACT_CALCULATOR,"Calculator","KGCALC.EXE"},
 {ACT_CLOCK,"Clock","KGCLOCK.EXE"},
 {ACT_SHELL,"DOS Shell","KSH.EXE"},
 {ACT_FILES,"Files","KGFILES.EXE"},
 {ACT_EDITOR,"Editor","KGEDIT.EXE"},
 {ACT_IMAGE,"Images","KGIMAGE.EXE"},
 {ACT_WORDS,"User dictionary","KGWORDS.EXE"},
 {ACT_SHELL32,"32-bit shell","KSH32.EXE"},
 {ACT_CALENDAR,"Calendar","KGCAL.EXE"},
 {ACT_SETTINGS,"Settings","KGSET.EXE"},
 {ACT_MEMO,"Memo","KGEDIT.EXE memo"},
};
static const char *const menu_items[] = {
 "About KGUI32", "Settings", "New DOS", "Find apps", "Switch windows", "Exit to DOS",
};

const ShellApp *shell_app(int action){
 for(unsigned i=0;i<sizeof apps/sizeof *apps;i++)if(apps[i].action==action)return &apps[i];
 return 0;
}
Rect shell_button(void){return (Rect){8,3,80,26};}
Rect shell_panel(void){return (Rect){8,34,312,284};}
Rect shell_row(unsigned row){return (Rect){16,94+(int)row*34,296,32};}
static int inside(Rect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
static char upper(char c){return c>='a'&&c<='z'?c-32:c;}
static int contains(const char *text,const char *query){
 if(!*query)return 1;
 for(;*text;text++){
  unsigned n=0;while(query[n]&&text[n]&&upper(text[n])==upper(query[n]))n++;
  if(!query[n])return 1;
 }
 return 0;
}
int shell_item(const Scene *s,unsigned index){
 if(s->shell_page==SHELL_KUMA)return index<SHELL_ROWS?(int)index:-1;
 if(s->shell_page==SHELL_SEARCH){
  for(unsigned i=0;i<sizeof apps/sizeof *apps;i++){
   if(!contains(apps[i].name,s->shell_query)&&!contains(apps[i].command,s->shell_query))continue;
   if(!index)return apps[i].action;
   index--;
  }
 }else if(s->shell_page==SHELL_SWITCH){
  for(int i=s->slots+1;i>=0;i--){
   int id=s->order[i];if(s->window[id].open){if(!index)return id;index--;}
  }
 }
 return -1;
}
unsigned shell_count(const Scene *s){
 unsigned n=0;while(shell_item(s,n)>=0)n++;return n;
}
const char *shell_label(const Scene *s,unsigned index){
 int i=shell_item(s,index);if(i<0)return "";
 if(s->shell_page==SHELL_KUMA)return menu_items[i];
 const ShellApp *app=shell_app(i);
 return app?app->name:"";
}
static void open_page(Scene *s,unsigned page){
 s->shell_page=page;s->shell_alt=0;s->shell_selected=s->shell_top=s->shell_length=0;s->shell_query[0]=0;
 scene_damage(s,shell_panel());
}
static void select_row(Scene *s,int step){
 unsigned n=shell_count(s);if(!n)return;
 s->shell_selected=(s->shell_selected+n+step)%n;
 if(s->shell_selected<s->shell_top)s->shell_top=s->shell_selected;
 if(s->shell_selected>=s->shell_top+SHELL_ROWS)s->shell_top=s->shell_selected-SHELL_ROWS+1;
 scene_damage(s,shell_panel());
}
static int activate(Scene *s){
 int item=shell_item(s,s->shell_selected),page=s->shell_page;
 if(item<0)return ACT_NOTICE_INPUT;
 open_page(s,SHELL_CLOSED);
 if(page==SHELL_SWITCH){scene_focus(s,item);return ACT_NOTICE_INPUT;}
 if(page==SHELL_SEARCH)return item;
 switch(item){
 case 0:open_page(s,SHELL_ABOUT);return ACT_NOTICE_INPUT;
 case 1:return ACT_SETTINGS;
 case 2:return ACT_SHELL;
 case 3:open_page(s,SHELL_SEARCH);return ACT_NOTICE_INPUT;
 case 4:open_page(s,SHELL_SWITCH);return ACT_NOTICE_INPUT;
 default:return ACT_EXIT;
 }
}
void shell_timer(Scene *s,unsigned modifiers){
 if(s->shell_alt&&!(modifiers&KG_MOD_ALT))activate(s);
}
int shell_key(Scene *s,const GuiEvent *e){
 if(e->key==281&&s->shell_page){open_page(s,SHELL_CLOSED);return -1;}
 if(e->key==11){open_page(s,SHELL_SEARCH);return ACT_NOTICE_INPUT;}
 if(e->key==9&&(e->buttons&KG_MOD_ALT)){
  if(s->shell_page!=SHELL_SWITCH)open_page(s,SHELL_SWITCH);
  s->shell_alt=1;select_row(s,e->buttons&KG_MOD_SHIFT?-1:1);return ACT_NOTICE_INPUT;
 }
 if(!s->shell_page||e->key==289||e->key==287)return -1;
 if(e->key==27||s->shell_page==SHELL_ABOUT){open_page(s,SHELL_CLOSED);return ACT_NOTICE_INPUT;}
 if(e->key==13)return activate(s);
 if(e->key==256||e->key==257||e->key==9){select_row(s,e->key==256?-1:1);return ACT_NOTICE_INPUT;}
 if(s->shell_page==SHELL_SEARCH){
  if(e->key==8&&s->shell_length)s->shell_length--;
  else if(e->key>=32&&e->key<=126&&s->shell_length+1<sizeof s->shell_query)s->shell_query[s->shell_length++]=e->key;
  s->shell_query[s->shell_length]=0;s->shell_selected=s->shell_top=0;scene_damage(s,shell_panel());
 }
 return ACT_NOTICE_INPUT;
}
int shell_mouse(Scene *s,const GuiEvent *e,int down){
 if(s->shell_swallow){
  if(!(e->buttons&1))s->shell_swallow=0;
  return ACT_NOTICE_INPUT;
 }
 if(!down)return s->shell_page?ACT_NOTICE_INPUT:-1;
 if(inside(shell_button(),e->x,e->y)){
  open_page(s,s->shell_page?SHELL_CLOSED:SHELL_KUMA);s->shell_swallow=1;return ACT_NOTICE_INPUT;
 }
 if(!s->shell_page)return -1;
 s->shell_swallow=1;
 for(unsigned i=0;i<SHELL_ROWS;i++)if(inside(shell_row(i),e->x,e->y)){
  s->shell_selected=s->shell_top+i;return activate(s);
 }
 if(s->shell_page==SHELL_SEARCH&&inside((Rect){224,61,88,28},e->x,e->y)){
  unsigned count=shell_count(s);
  if(count>SHELL_ROWS){s->shell_top=(s->shell_top+SHELL_ROWS)%count;s->shell_selected=s->shell_top;scene_damage(s,shell_panel());}
  return ACT_NOTICE_INPUT;
 }
 if(s->shell_page==SHELL_ABOUT||!inside(shell_panel(),e->x,e->y))open_page(s,SHELL_CLOSED);
 return ACT_NOTICE_INPUT;
}
