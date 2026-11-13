/* Independent root/Work browser with a deferred, owned Editor launch. */
#include "../sdk/kgui.h"
#include "../sdk/widgets.h"
#include "../sdk/text.h"
#define ENTRIES 192
#define TEXT_BYTES (128*1024)
#define LINES TEXT_BYTES /* even a file consisting entirely of newlines */
/* First 64 bytes are sampled from this slot's private RW mapping. */
typedef struct {u32 magic,version,frames,events,focus,selected,entries,matches,top,mode,bytes,hash,heartbeat,error,name_hash,records;} FilesStats;
volatile FilesStats files_stats __attribute__((section(".data.gui_stats")))={.magic=0x3246474b,.version=2};
static K32DirEntry entries[ENTRIES],entry;
static unsigned matches[ENTRIES],count,cursor,loading,holding,mode,bytes,line_count,offsets[LINES],preview_top;
static unsigned char *contents;
static unsigned work,edit_pending;static K32WorkInfo destination;
static KwInput filter;
static KwList list;static KwFocus focus;static KwScroll scroll;
static const char *message;static char filename[13];
static unsigned hash(const void *p,unsigned n){const unsigned char *b=p;unsigned h=2166136261u;while(n--)h=(h^*b++)*16777619;return h;}
static unsigned upper(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int contains(const char *s,const char *part){
 if(!*part)return 1;
 for(;*s;s++){unsigned i=0;while(part[i]&&s[i]&&upper(s[i])==upper(part[i]))i++;if(!part[i])return 1;}return 0;
}
static void rebuild(void){
 list.count=0;for(unsigned i=0;i<count;i++)if(contains(entries[i].name,filter.text))matches[list.count++]=i;
 kw_reveal(&list);
}
static void close_cursor(void){if(cursor)k32_dir_close(cursor);cursor=loading=0;}
static void refresh(void){close_cursor();files_stats.error=0;count=0;list.selected=list.top=0;loading=1;rebuild();}
static int enumerate(void){
 if(!loading||(holding&&count))return 0;
 for(unsigned i=0;i<4;i++){
  int e=cursor?k32_dir_next(cursor,&entry):work?k32_work_first(&entry):k32_dir_first(&entry);
  if(e<0){files_stats.error=-e;message=work?"Cannot read Work":"Cannot read A: root";close_cursor();break;}
  cursor=entry.cursor;
  if(!e){close_cursor();break;}
  if(count==ENTRIES){message="Root entry limit reached";close_cursor();break;}
  entries[count++]=entry;
 }
 rebuild();return 1;
}
static void layout(void){
 unsigned h=kg_height(),w=kg_width();
 list.rows=(h-104)/16;if(list.rows<1)list.rows=1;
 /* Reserve commands for all eight controls at maximum canvas height. */
 if(list.rows>18)list.rows=18;
 if(!mode)kw_reveal(&list);
 unsigned rows=list.rows;
 unsigned total=mode?line_count:list.count,top=mode?preview_top:list.top;
 unsigned max=total>rows?total-rows:0;if(top>max)top=max;
 if(mode)preview_top=top;else list.top=top;
 scroll=(KwScroll){.r={(int)w-28,52,20,(int)rows*16},.dragging=scroll.dragging,.offset=scroll.offset,.total=total,.rows=rows,.top=top};
}
static void number(char *s,unsigned n){char rev[10];unsigned i=0,k=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);while(k)s[i++]=rev[--k];s[i]=0;}
static void preview_lines(void){
 line_count=0;unsigned col=0;int new_line=1;
 for(unsigned at=0;at<bytes;){
  unsigned c=contents[at];if(c=='\r'){at++;continue;}
  unsigned size=kt_next((const char *)contents,bytes,at)-at;
  if(c!='\n'&&col+size>40){new_line=1;col=0;}
  if(new_line){if(line_count==LINES)break;offsets[line_count++]=at;new_line=0;}
  if(c=='\n'){new_line=1;col=0;}else {col+=size;if(col==40){new_line=1;col=0;}}
  at+=size;
 }
}
static void open_preview(void){
 if(!list.count||loading)return;
 K32DirEntry *d=&entries[matches[list.selected]];
 if(d->attributes&16){message="Subdirectories are not supported";return;}
 unsigned n=k32_len(d->name);
 if(n<4||d->name[n-4]!='.'||upper(d->name[n-3])!='T'||upper(d->name[n-2])!='X'||upper(d->name[n-1])!='T'){message="Select a TXT file to preview";return;}
 if(d->bytes>TEXT_BYTES){message="Preview limit: 128 KiB";return;}
 K32Work opened=k32_work_name(K32_WORK_OPEN,d->name);
 int h=work?k32_work(&opened):k32_open(d->name);if(h<0){message="Cannot open this file";return;}
 contents=k32_alloc(d->bytes?d->bytes:1);bytes=0;
 int error=contents?0:-12;
 while(!error&&bytes<d->bytes){unsigned n=d->bytes-bytes;if(n>4096)n=4096;int got=k32_read(h,contents+bytes,n);if(got<=0){error=got?got:-5;break;}bytes+=got;}
 int closed=k32_close(h);if(!error&&closed<0)error=closed;
 if(error){if(contents)k32_free(contents);contents=0;message="Preview read failed";files_stats.error=-error;return;}
 if(!kt_valid((const char *)contents,bytes,KT_LINES|KT_CRLF)){k32_free(contents);contents=0;message="Select valid Shift-JIS text";return;}
 for(unsigned i=0;i<13;i++)filename[i]=d->name[i];
 files_stats.hash=hash(contents,bytes);files_stats.bytes=bytes;
 preview_lines();preview_top=0;mode=1;focus.current=1;scroll.dragging=0;
}
static void back(void){if(contents)k32_free(contents);contents=0;mode=0;scroll.dragging=0;focus.current=1;}
static void switch_root(void){
 back();close_cursor();work=!work;files_stats.error=0;
 if(work){int e=k32_work_info(&destination);if(e<0||destination.error){message="Workspace unavailable";files_stats.error=e<0?-e:-destination.error;count=list.count=0;return;}}
 refresh();
}
static void edit_selected(void){
 if(edit_pending){message="Editor request pending";return;}
 if(loading||!list.count){message="Select a TXT or BMP file";return;}
 K32DirEntry *d=&entries[matches[list.selected]];unsigned n=k32_len(d->name);
 int bitmap=!work&&n>=5&&d->name[n-4]=='.'&&upper(d->name[n-3])=='B'&&upper(d->name[n-2])=='M'&&upper(d->name[n-1])=='P';
 if((d->attributes&16)||n<5||d->name[n-4]!='.'||(!bitmap&&(upper(d->name[n-3])!='T'||upper(d->name[n-2])!='X'||upper(d->name[n-1])!='T'))){message="Select a TXT or BMP file";return;}
 if(!bitmap&&d->bytes>65536){message="Editor limit: 64 KiB";return;}
 int e=kg_edit_document(d->name,work);if(e<0){files_stats.error=-e;message="Editor request rejected";}else{edit_pending=1;files_stats.error=0;}
}
static void diagnostics(void){
 files_stats.focus=focus.current;files_stats.selected=list.selected;files_stats.entries=count;files_stats.matches=list.count;
 files_stats.top=mode?preview_top:list.top;files_stats.mode=(work?32u:0u)|(edit_pending?64u:0u)|mode|(loading?2u:0u)|(message?4u:0u)|(scroll.dragging?8u:0u)|(holding?16u:0u);
 files_stats.name_hash=mode?hash(filename,k32_len(filename)):list.count?hash(entries[matches[list.selected]].name,k32_len(entries[matches[list.selected]].name)):0;
 unsigned metadata=2166136261u;
 for(unsigned i=0;i<count;i++){K32DirEntry *d=&entries[i];metadata=(metadata^hash(d->name,k32_len(d->name)))*16777619;metadata=(metadata^d->bytes)*16777619;metadata=(metadata^d->attributes)*16777619;metadata=(metadata^d->date)*16777619;metadata=(metadata^d->time)*16777619;}
 files_stats.records=metadata;
}
static int draw(void){
 layout();unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 kg_text(8,8,mode?filename:work?destination.root:"A: root / read only",0x20344a);
 if(focus.current>=6)kg_rect(w-(focus.current==6?169:89),1,focus.current==6?78:82,22,0x327ac0);
 kg_button(104,w-168,2,76,20,work?"Root":"Work");kg_button(105,w-88,2,80,20,"Open app");
 if(mode)kg_text(8,30,"TXT preview / 40 columns",0x466581);
 else {kg_text(8,32,"Filter",0x466581);kw_draw_input((KwRect){64,24,(int)w-76,26},&filter,focus.current==0);}
 kg_rect(8,52,w-40,list.rows*16,0xffffff);
 if(mode){
  for(unsigned row=0;row<scroll.rows&&row+preview_top<line_count;row++){
   char line[41];unsigned at=offsets[row+preview_top],i=0;
   while(at<bytes&&i<40&&contents[at]!='\n'){unsigned c=contents[at];if(c=='\r'){at++;continue;}unsigned n=kt_next((const char *)contents,bytes,at)-at;if(n>40-i)break;for(unsigned j=0;j<n;j++)line[i++]=c=='\t'?' ':contents[at+j];at+=n;}line[i]=0;
   kw_text(12,52+row*16,line,(w-48)/8,0x20344a);
  }
 }else {
  if(list.count&&list.selected>=list.top&&list.selected<list.top+list.rows)kg_rect(8,52+(list.selected-list.top)*16,w-40,16,focus.current==1?0x327ac0:0xdce7ef);
  for(unsigned row=0;row<list.rows&&row+list.top<list.count;row++){
   K32DirEntry *d=&entries[matches[row+list.top]];char line[44];unsigned n=0;
   for(;d->name[n]&&n<12;n++)line[n]=d->name[n];
   for(;n<12;n++)line[n]=' ';
   line[12]=' ';line[13]=' ';if(d->attributes&16){line[14]='<';line[15]='D';line[16]='I';line[17]='R';line[18]='>';line[19]=0;}else number(line+14,d->bytes);
   kw_text(12,52+row*16,line,(w-48)/8,row+list.top==list.selected&&focus.current==1?0xffffff:0x20344a);
  }
 }
 kw_draw_scroll(&scroll);
 int y=h-46;const char *labels[]={mode?"Back":"Open",mode?"List":"Refresh","About","Close"};
 for(unsigned i=0;i<4;i++){
  int x=8+i*((w-16)/4),bw=(w-24)/4;
  if(focus.current==i+2)kg_rect(x-1,y-1,bw+2,30,0x327ac0);
  kg_button(100+i,x,y,bw,28,labels[i]);
 }
 char n[11];number(n,mode?bytes:list.count);kg_text(8,h-16,n,0x466581);kw_text(64,h-16,loading?"Loading...":"Ctrl W:root O:Editor",(w-72)/8,0x466581);
 /* A modal uses its own frame, keeping even a maximized canvas inside the
  * 40-command ABI budget rather than silently losing its OK button. */
 if(message){kg_begin();kg_rect(0,0,w,h,0xb4c2d0);kw_draw_dialog(w,h,"Files",message);}
 diagnostics();int e=kg_present();if(!e)files_stats.frames++;return e;
}
int main(int argc,char **argv){
 int fault=argc>1&&k32_len(argv[1])==5&&contains(argv[1],"fault");
 holding=argc>1&&k32_len(argv[1])==4&&contains(argv[1],"hold");
 int t=kg_open("Files",576,320);if(t<0){files_stats.error=-t;return 1;}
 if((k32_query()&K32_FEATURE_DIRECTORY)==0||kg_capabilities()<0||kg_subscribe(KG_INPUT_FLAGS)){kg_close();return 2;}
 list.rows=1;focus.count=8;refresh();int dirty=1,quit=0;
 while(!quit){
  KgEvent e;int got=kg_poll(&e);files_stats.heartbeat++;
  if(got<0){files_stats.error=-got;break;}
  if(enumerate())dirty=1;
  if(edit_pending){int result=kg_edit_status();if(result!=-11){edit_pending=0;dirty=1;files_stats.error=result<0?-result:0;if(result<0)message=result==-16?"Both slots in use: close a task":"Cannot start Editor";}}
  if(fault&&!loading&&!mode){
   for(unsigned i=0;i<list.count;i++)if(k32_len(entries[matches[i]].name)==10&&contains(entries[matches[i]].name,"BROWSE.TXT")){list.selected=i;open_preview();break;}
   if(mode)*(volatile unsigned *)0x10000=1;
  }
  if(got){files_stats.events++;layout();
   if(e.type==KG_RESIZE){scroll.dragging=0;dirty=1;}
   if(e.type==KG_FOCUS&&!e.key&&scroll.dragging){scroll.dragging=0;dirty=1;}
   if(holding&&e.type==KG_KEY&&e.key==32){holding=0;dirty=1;}
   else if(message){if(kw_dialog(&e)){message=0;dirty=1;}}
   else if(kw_focus(&focus,&e))dirty=1;
   else {
    if(e.type==KG_MOUSE&&(e.key&1)){
     if(!mode&&kw_inside((KwRect){64,24,(int)kg_width()-76,26},e.x,e.y))focus.current=0;
     if(kw_inside((KwRect){8,52,(int)kg_width()-40,(int)list.rows*16},e.x,e.y))focus.current=1;
     if(e.control>=100&&e.control<=103)focus.current=e.control-98;
     if(e.control==104||e.control==105)focus.current=e.control-98;
     dirty=1;
    }
    if(kw_scroll(&scroll,&e)){if(mode)preview_top=scroll.top;else {list.top=scroll.top;if(list.selected<list.top||list.selected>=list.top+list.rows)list.selected=list.top;}dirty=1;}
    if(!mode&&focus.current==0){int changed=kw_input(&filter,&e);if(changed){if(changed==2){list.selected=list.top=0;rebuild();}dirty=1;}}
    if(focus.current==1){
     if(!mode){if(kw_list(&list,(KwRect){8,52,(int)kg_width()-40,(int)list.rows*16},&e))dirty=1;}
     else if(e.type==KG_KEY){unsigned k=e.key,max=line_count>scroll.rows?line_count-scroll.rows:0;
      if(k==256&&preview_top)preview_top--;
      if(k==257&&preview_top<max)preview_top++;
      if(k==260)preview_top=0;
      if(k==261)preview_top=max;
      if(k==262)preview_top=preview_top>scroll.rows?preview_top-scroll.rows:0;
      if(k==263)preview_top=preview_top+scroll.rows<max?preview_top+scroll.rows:max;
      dirty=1;
     }
    }
    unsigned command=0;
    if(e.type==KG_MOUSE&&(e.key&1)&&e.control>=100&&e.control<=105)command=e.control-99;
    if(e.type==KG_KEY&&e.key==13)command=focus.current>=2?focus.current-1:focus.current==1&&!mode?1:0;
    if(e.type==KG_KEY&&e.key==27&&mode)command=1;
    if(e.type==KG_KEY&&e.key==23)command=5;
    if(e.type==KG_KEY&&e.key==15)command=6;
    if(command==5){switch_root();dirty=1;}
    if(command==6){edit_selected();dirty=1;}
    if(command==1){if(mode)back();else open_preview();dirty=1;}
    if(command==2){if(mode)back();else refresh();dirty=1;}
    if(command==3){message="Root: read only / Work: Editor";dirty=1;}
    if(command==4)quit=1;
   }
  }
  if(dirty&&!quit){int error=draw();if(error==-22)dirty=1;else if(error<0){files_stats.error=-error;break;}else dirty=0;}
  k32_wait(K32_WAIT_TIME,16);
 }
 close_cursor();if(contents)k32_free(contents);return kg_close()?3:0;
}
