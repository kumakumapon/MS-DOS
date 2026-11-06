/* Original SDK3 client. Documents are temporary; no write service is used. */
#include "../sdk/editor.h"
#include "../sdk/picker.h"
static KeDocument doc;static KpPicker picker;
static unsigned lines[KE_BYTES+1],line_count,top,cols,rows,drag,mark,mode,pending;
static char filename[13],clipboard[1024],load_buffer[KE_BYTES];
static const char *message;static KwScroll editor_scroll;
volatile struct {u32 magic,version,frames,events,bytes,hash,cursor,anchor,dirty,mode,top,heartbeat,error,history,redo,reserved;} editor_stats __attribute__((section(".data.gui_stats")))={.magic=0x3345474b,.version=3};
static void layout(void){cols=(kg_width()-48)/8;if(cols>47)cols=47;rows=(kg_height()-80)/16;if(rows>14)rows=14;}
static unsigned row_at(unsigned at){unsigned r=0;while(r+1<line_count&&lines[r+1]<=at)r++;return r;}
static void index_lines(int follow){
 layout();KeVersion *v=ke_view(&doc);unsigned col=0;line_count=1;lines[0]=0;
 for(unsigned i=0;i<v->n;i++){if(v->text[i]=='\n'||++col==cols){lines[line_count++]=i+1;col=0;}}
 unsigned r=row_at(v->cursor);if(follow){if(r<top)top=r;if(r>=top+rows)top=r-rows+1;}
 unsigned max=line_count>rows?line_count-rows:0;if(top>max)top=max;
}
static void report(void){KeVersion *v=ke_view(&doc);unsigned hash=2166136261u;for(unsigned i=0;i<v->n;i++)hash=(hash^(unsigned char)v->text[i])*16777619u;
 editor_stats.bytes=v->n;editor_stats.hash=hash;editor_stats.cursor=v->cursor;editor_stats.anchor=v->anchor;editor_stats.dirty=ke_dirty(&doc);editor_stats.mode=mode;editor_stats.top=top;editor_stats.history=doc.current;editor_stats.redo=doc.count-doc.current-1;
}
static void load(const char *name){
 int h=k32_open(name);if(h<0){message="Cannot open file";return;}unsigned n=0;int error=0;
 while(n<KE_BYTES){int got=k32_read(h,load_buffer+n,KE_BYTES-n>4096?4096:KE_BYTES-n);if(got<0){error=got;break;}if(!got)break;n+=got;}
 if(!error&&n==KE_BYTES){char extra;int got=k32_read(h,&extra,1);if(got)error=-27;}
 int closed=k32_close(h);if(!error&&closed<0)error=closed;
 if(error||ke_load(&doc,load_buffer,n)){message="Open failed: ASCII text, max 64 KiB";editor_stats.error=error?-error:22;return;}
 for(unsigned i=0;i<13;i++)filename[i]=name[i];
 top=0;index_lines(1);
}
static int perform(unsigned what){
 if(what==1)return 1;
 if(what==2){ke_load(&doc,"",0);filename[0]=0;top=0;}
 if(what==3){kp_open(&picker);mode=1;}
 return 0;
}
static int request(unsigned what){drag=mark=0;if(ke_dirty(&doc)){pending=what;mode=2;return 0;}return perform(what);}
static void copy_selection(int cut){KeVersion *v=ke_view(&doc);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;if(b-a>1024){message="Clipboard limit: 1024 bytes";return;}if(b==a)return;if(kg_clip_set(v->text+a,b-a)<0){message="Clipboard unavailable";return;}if(cut)ke_replace(&doc,"",0);}
static unsigned hit(const KgEvent *e){unsigned r=top+(e->y<40?0:(unsigned)(e->y-40)/16);if(r>=line_count)return ke_view(&doc)->n;unsigned col=e->x<8?0:(unsigned)(e->x-8)/8;if(col>cols)col=cols;unsigned at=lines[r]+col,end=r+1<line_count?lines[r+1]:ke_view(&doc)->n;KeVersion *v=ke_view(&doc);if(end>lines[r]&&v->text[end-1]=='\n')end--;return at>end?end:at;}
static void edit_key(unsigned k){
 KeVersion *v=ke_view(&doc);unsigned at=v->cursor,r=row_at(at),col=at-lines[r];
 if(k==1){ke_select(&doc,0,v->n);return;}
 if(k==2){mark=!mark;if(mark)v->anchor=v->cursor;return;}
 if(k==3){copy_selection(0);return;}if(k==24){copy_selection(1);return;}
 if(k==22){int n=kg_clip_get(clipboard,sizeof clipboard);if(n>0&&ke_replace(&doc,clipboard,n)<0)message="Paste rejected: text or size limit";return;}
 if(k==26||k==25){ke_undo(&doc,k==25);return;}
 if(k>=256&&k<=263){
  if(k==258&&at)at--;
  if(k==259&&at<v->n)at++;
  if(k==260)at=lines[r];
  if(k==261){at=r+1<line_count?lines[r+1]:v->n;if(at&&v->text[at-1]=='\n')at--;}
  if(k==256||k==257||k==262||k==263){unsigned step=k>=262?rows:1;r=(k==256||k==262)?(r>step?r-step:0):(r+step<line_count?r+step:line_count-1);at=lines[r]+col;unsigned end=r+1<line_count?lines[r+1]:v->n;if(at>end)at=end;}
  ke_select(&doc,mark?v->anchor:at,at);return;
 }
 if(k==8||k==264){if(v->anchor==v->cursor){if(k==8&&v->cursor)v->anchor--;else if(k==264&&v->cursor<v->n)v->anchor++;}ke_replace(&doc,"",0);return;}
 char c=k==13?'\n':(char)k;if(k==13||k==9||(k>=32&&k<=126))if(ke_replace(&doc,&c,1)<0)message="Document limit: 64 KiB";
}
static int draw(void){
 report();if(mode==1)return kp_draw(&picker);
 unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xffffff);
 if(mode==2){kg_text(8,8,"Unsaved temporary text",0x20344a);kg_text(8,32,"Discard changes? No disk saving yet.",0x20344a);kg_button(801,8,64,100,28,"Discard");kg_button(802,120,64,100,28,"Cancel");return kg_present();}
 if(message){kw_draw_dialog(w,h,"Editor",message);return kg_present();}
 kg_button(601,8,4,64,28,"New");kg_button(602,80,4,64,28,"Open");kw_text(152,10,filename[0]?filename:"Untitled",(w-160)/8,0x20344a);
 KeVersion *v=ke_view(&doc);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;
 for(unsigned i=0;i<rows&&i+top<line_count;i++){
  unsigned start=lines[i+top],end=i+top+1<line_count?lines[i+top+1]:v->n;
  if(a<end&&b>start){unsigned l=a>start?a-start:0,z=b<end?b-start:end-start;if(z>cols)z=cols;if(z>l)kg_rect(8+l*8,40+i*16,(z-l)*8,16,0xb4d6f0);}
  char text[48];unsigned n=0;while(start+n<end&&n<cols){char c=v->text[start+n];if(c=='\n')break;text[n++]=c=='\t'?' ':c;}text[n]=0;kg_text(8,40+i*16,text,0x20344a);
 }
 unsigned r=row_at(v->cursor);if(r>=top&&r<top+rows){unsigned c=v->cursor-lines[r];if(c<cols)kg_rect(8+c*8,54+(r-top)*16,8,2,0x327ac0);}
 KwScroll scroll={.r={(int)w-28,40,20,(int)rows*16},.total=line_count,.rows=rows,.top=top};kw_draw_scroll(&scroll);
 kw_text(8,h-32,ke_dirty(&doc)?"Modified / temporary text":"Temporary text / read-only disk",(w-16)/8,0x466581);
 kw_text(8,h-16,"Ctrl A/C/X/V Z/Y B:mark F2:launcher",(w-16)/8,0x466581);return kg_present();
}
int main(int argc,char **argv){
 ke_load(&doc,"",0);if(kg_open("Editor / temporary text",576,320)<0)return 1;
 if((kg_capabilities()&KG_CAP_ALL)!=KG_CAP_ALL||kg_subscribe(KG_CAP_ALL))return 2;
 index_lines(1);if(argc>1&&argv[1][0]=='f'){load("EDITTEST.TXT");draw();*(volatile u32 *)0x10000=1;}
 int dirty=1,quit=0;
 while(!quit){KgEvent e;int got=kg_poll(&e);editor_stats.heartbeat++;if(got<0)break;
  if(mode==1&&kp_tick(&picker))dirty=1;
  if(got){editor_stats.events++;dirty=1;
   if(e.type==KG_QUIT){if(mode==1)kp_close(&picker);mode=0;message=0;quit=request(1);}
   else if(mode==2){if(kw_button(801,&e)||(e.type==KG_KEY&&e.key=='d')){mode=0;quit=perform(pending);}else if(kw_button(802,&e)||(e.type==KG_KEY&&e.key==27)){mode=0;kg_cancel_close();}}
   else if(mode==1){char name[13];int selected=kp_event(&picker,&e,name);if(selected){mode=0;if(selected>0)load(name);}}
   else if(message){if(kw_dialog(&e))message=0;}
   else if(kw_button(601,&e)||(e.type==KG_KEY&&e.key==14))quit=request(2);
   else if(kw_button(602,&e)||(e.type==KG_KEY&&e.key==15))quit=request(3);
   else if(e.type==KG_KEY){edit_key(e.key);index_lines(1);}
   else if(e.type==KG_RESIZE){editor_scroll.dragging=0;drag=0;index_lines(1);}
   else if(e.type==KG_FOCUS&&!e.key){drag=0;editor_scroll.dragging=0;}
   else if(e.type==KG_MOUSE||e.type==KG_POINTER){
    editor_scroll.r=(KwRect){(int)kg_width()-28,40,20,(int)rows*16};editor_scroll.total=line_count;editor_scroll.rows=rows;editor_scroll.top=top;
    if(kw_scroll(&editor_scroll,&e)){top=editor_scroll.top;drag=0;}
    else if(!(e.key&1))drag=0;
    else if(drag){ke_select(&doc,ke_view(&doc)->anchor,hit(&e));}
    else if(e.type==KG_MOUSE&&kw_inside((KwRect){8,40,(int)cols*8,(int)rows*16},e.x,e.y)){unsigned at=hit(&e);ke_select(&doc,at,at);drag=1;}
    
   }
  }
  if(dirty&&!quit){int error=draw();if(error!=-22&&error<0){editor_stats.error=-error;break;}dirty=error==-22;editor_stats.frames++;}
  k32_wait(K32_WAIT_TIME,16);
 }
 if(mode==1)kp_close(&picker);
 return kg_close()?3:0;
}
