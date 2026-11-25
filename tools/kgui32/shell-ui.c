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
enum {APP_COUNT=sizeof apps/sizeof *apps, DOCUMENT_ITEM=100};
_Static_assert(APP_COUNT<=12,"launcher recent capacity");
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
int shell_pinned(const Scene *s,int action){
 for(unsigned i=0;i<APP_COUNT;i++)if(apps[i].action==action)return !!(s->app_pins&(1u<<i));
 return 0;
}
void shell_record_launch(Scene *s,int action){
 for(unsigned i=0;i<APP_COUNT;i++)if(apps[i].action==action){
  unsigned at=0;
  while(at<s->recent_apps&&s->app_recent[at]!=i)at++;
  if(at==s->recent_apps)s->recent_apps++;
  for(;at;at--)s->app_recent[at]=s->app_recent[at-1];
  s->app_recent[0]=i;
  return;
 }
}
const volatile KgRecentDocument *shell_document(const Scene *s,int item){
 return s->documents&&item>=DOCUMENT_ITEM&&item<DOCUMENT_ITEM+(int)KG_RECENT_COUNT?s->documents+item-DOCUMENT_ITEM:0;
}
static int duplicate(const volatile KgRecentDocument *a,const volatile KgRecentDocument *b){
 if(a->bitmap!=b->bitmap||a->volume!=b->volume)return 0;
 for(unsigned i=0;i<KG_RECENT_PATH;i++){
  if(a->path[i]!=b->path[i])return 0;
  if(!a->path[i])return 1;
 }
 return 0;
}
static int next_document(const Scene *s,u32 before){
 int best=-1;
 if(!s->documents)return best;
 for(unsigned i=0;i<KG_RECENT_COUNT;i++){
  const volatile KgRecentDocument *d=&s->documents[i];
  if(!d->serial||d->serial>=before||(best>=0&&d->serial<=s->documents[best].serial))continue;
  unsigned j=0;
  for(;j<KG_RECENT_COUNT;j++)if(s->documents[j].serial>d->serial&&duplicate(&s->documents[j],d))break;
  if(j==KG_RECENT_COUNT)best=i;
 }
 return best;
}
int shell_item(const Scene *s,unsigned index){
 if(s->shell_page==SHELL_KUMA)return index<SHELL_ROWS?(int)index:-1;
 if(s->shell_page==SHELL_SEARCH){
  for(int pinned=1;pinned>=0;pinned--)for(unsigned i=0;i<APP_COUNT;i++){
   if(pinned!=(int)!!(s->app_pins&(1u<<i)))continue;
   if(!contains(apps[i].name,s->shell_query)&&!contains(apps[i].command,s->shell_query))continue;
   if(!index)return apps[i].action;
   index--;
  }
 }else if(s->shell_page==SHELL_SWITCH){
  for(int i=s->slots+1;i>=0;i--){int id=s->order[i];if(s->window[id].open){if(!index)return id;index--;}}
 }else if(s->shell_page==SHELL_RECENT){
  u32 before=~0u;int id;
  while((id=next_document(s,before))>=0){
   if(!index)return DOCUMENT_ITEM+id;
   index--;before=s->documents[id].serial;
  }
  if(index<s->recent_apps)return apps[s->app_recent[index]].action;
 }else if(s->shell_page==SHELL_REBOOT)return index<2?(int)index:-1;
 return -1;
}
unsigned shell_count(const Scene *s){
 unsigned n=0;while(shell_item(s,n)>=0)n++;return n;
}
const char *shell_label(const Scene *s,unsigned index){
 int i=shell_item(s,index);if(i<0)return "";
 if(s->shell_page==SHELL_KUMA)return menu_items[i];
 if(s->shell_page==SHELL_REBOOT)return i?"Cancel":"Restart computer";
 const volatile KgRecentDocument *doc=shell_document(s,i);
 if(doc){const volatile char *name=doc->path;for(const volatile char *c=name;*c;c++)if(*c=='\\')name=c+1;return (const char *)name;}
 const ShellApp *app=shell_app(i);
 return app?app->name:"";
}
static void open_page(Scene *s,unsigned page){
 s->shell_page=page;s->shell_alt=0;s->shell_selected=s->shell_top=s->shell_length=0;s->shell_query[0]=0;
 if(page==SHELL_REBOOT)s->shell_selected=1;
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
 if(page==SHELL_REBOOT)return item?ACT_NOTICE_INPUT:ACT_REBOOT;
 if(page==SHELL_RECENT&&item>=DOCUMENT_ITEM){s->document_index=item;return ACT_OPEN_RECENT;}
 if(page==SHELL_SEARCH||page==SHELL_RECENT)return item;
 switch(item){
 case 0:open_page(s,SHELL_ABOUT);return ACT_NOTICE_INPUT;
 case 1:return ACT_SETTINGS;
 case 2:return ACT_SHELL;
 case 3:open_page(s,SHELL_SEARCH);return ACT_NOTICE_INPUT;
 case 4:open_page(s,SHELL_SWITCH);return ACT_NOTICE_INPUT;
 default:return ACT_EXIT;
 }
}
static int toggle_pin(Scene *s){
 int action=shell_item(s,s->shell_selected);
 for(unsigned i=0;i<APP_COUNT;i++)if(apps[i].action==action){
  s->app_pins^=1u<<i;s->shell_selected=s->shell_top=0;scene_damage(s,shell_panel());return ACT_PIN_CHANGED;
 }
 return ACT_NOTICE_INPUT;
}
void shell_timer(Scene *s,unsigned modifiers){
 if(s->shell_alt&&!(modifiers&KG_MOD_ALT))activate(s);
}
int shell_key(Scene *s,const GuiEvent *e){
 if(e->key==281&&s->shell_page){open_page(s,SHELL_CLOSED);return -1;}
 if(e->key==11){open_page(s,e->buttons&KG_MOD_SHIFT?SHELL_KUMA:SHELL_SEARCH);return ACT_NOTICE_INPUT;}
 if(e->key==9&&(e->buttons&KG_MOD_ALT)){
  if(s->shell_page!=SHELL_SWITCH)open_page(s,SHELL_SWITCH);
  s->shell_alt=1;select_row(s,e->buttons&KG_MOD_SHIFT?-1:1);return ACT_NOTICE_INPUT;
 }
 if(!s->shell_page||e->key==289||e->key==287)return -1;
 if(e->key==27||s->shell_page==SHELL_ABOUT){open_page(s,SHELL_CLOSED);return ACT_NOTICE_INPUT;}
 if(s->shell_page==SHELL_KUMA&&(e->key=='r'||e->key=='R')){open_page(s,SHELL_RECENT);return ACT_NOTICE_INPUT;}
 if(s->shell_page==SHELL_KUMA&&(e->key=='b'||e->key=='B')){open_page(s,SHELL_REBOOT);return ACT_NOTICE_INPUT;}
 if(s->shell_page==SHELL_REBOOT&&(e->key=='r'||e->key=='R')){s->shell_selected=0;return activate(s);}
 if(s->shell_page==SHELL_SEARCH&&e->key==16)return toggle_pin(s);
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
 if(s->shell_page==SHELL_KUMA){
  if(inside((Rect){16,61,92,28},e->x,e->y)){open_page(s,SHELL_RECENT);return ACT_NOTICE_INPUT;}
  if(inside((Rect){112,61,120,28},e->x,e->y)){open_page(s,SHELL_REBOOT);return ACT_NOTICE_INPUT;}
 }
 for(unsigned i=0;i<SHELL_ROWS;i++)if(inside(shell_row(i),e->x,e->y)){
  s->shell_selected=s->shell_top+i;
  if(s->shell_page==SHELL_SEARCH&&e->x>=244)return toggle_pin(s);
  return activate(s);
 }
 if((s->shell_page==SHELL_SEARCH||s->shell_page==SHELL_RECENT)&&inside((Rect){224,61,88,28},e->x,e->y)){
  unsigned count=shell_count(s);
  if(count>SHELL_ROWS){s->shell_top=(s->shell_top+SHELL_ROWS)%count;s->shell_selected=s->shell_top;scene_damage(s,shell_panel());}
  return ACT_NOTICE_INPUT;
 }
 if(s->shell_page==SHELL_ABOUT||!inside(shell_panel(),e->x,e->y))open_page(s,SHELL_CLOSED);
 return ACT_NOTICE_INPUT;
}
