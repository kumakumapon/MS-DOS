/* Original SDK4 editor. Writes use only the bounded workspace broker. */
#include "../sdk/editor.h"
#include "../sdk/picker.h"
#include "../document.h"
#include "../sdk/recovery.h"
#include "../sdk/text.h"
#include "../sdk/ime.h"
#include "../sdk/ime-store.h"
#include "ui-editor.h"
static KeDocument doc;static KpPicker picker;static KrRecovery recovery;
static KwInput find_text,replace_text;static KwFocus find_focus;static unsigned replacing,recovery_suggested;
static const char *notice,*find_note;
static unsigned lines[KE_BYTES+1],line_count,top,cols,rows,drag,mark,mode,pending;
static unsigned in_work;static K32Work revision,save_request;
static K32WorkInfo destination;
static char filename[13],clipboard[1024],load_buffer[KE_BYTES];
static KiIme ime;static KiDictionary ime_dictionary;
static KiDictionary user_dictionary;static KiStore user_store;
static unsigned char dictionary_bytes[KI_DICTIONARY];
static unsigned dictionary_checked,ime_target;
/* Extension is app-private diagnostics. The original 64-byte v4 sample stays. */
static const char *message;static KwScroll editor_scroll;
volatile struct {u32 magic,version,frames,events,bytes,hash,cursor,anchor,dirty,mode,top,heartbeat,error,history,redo,workspace;
 u32 ime_magic,ime_scheme,ime_script,ime_bytes,ime_pending,ime_cursor,ime_candidates,ime_selected,ime_selecting,ime_target,ime_dictionary,ime_hash;
 u32 user_readings,user_error;
} editor_stats __attribute__((section(".data.gui_stats")))={.magic=0x3345474b,.version=4,.ime_magic=0x31454d49};
static void layout(void){cols=(kg_width()-48)/8;if(cols>92)cols=92;rows=(kg_height()-80)/16;if(rows>21)rows=21;}
static void budget_rows(void){
 KeVersion *v=ke_view(&doc);unsigned used=0,limit=rows,actual=0;
 for(unsigned row=0;row<limit;row++){unsigned cost=0;if(row+top<line_count){unsigned a=lines[row+top],b=row+top+1<line_count?lines[row+top+1]:v->n;if(b>a&&v->text[b-1]=='\n')b--;while(a<b){a+=kt_prefix(v->text+a,b-a,47);cost++;}}
  if(used+cost>KG_COMMANDS-16)break;
  used+=cost;actual++;}
 rows=actual?actual:1;
}
static unsigned row_at(unsigned at){unsigned r=0;while(r+1<line_count&&lines[r+1]<=at)r++;return r;}
static void index_lines(int follow){
 layout();KeVersion *v=ke_view(&doc);unsigned col=0;line_count=1;lines[0]=0;
 for(unsigned i=0;i<v->n;){unsigned n=kt_next(v->text,v->n,i)-i;
  if(v->text[i]=='\n'){i++;lines[line_count++]=i;col=0;continue;}
  if(col+n>cols){lines[line_count++]=i;col=0;}
  i+=n;col+=n;if(col==cols){lines[line_count++]=i;col=0;}
 }
 unsigned r=row_at(v->cursor);
 for(unsigned pass=0;pass<22;pass++){layout();budget_rows();unsigned old=top;if(follow){if(r<top)top=r;if(r>=top+rows)top=r-rows+1;}
  unsigned max=line_count>rows?line_count-rows:0;if(top>max)top=max;if(top==old)break;}
 layout();budget_rows();
}
static void report(void){KeVersion *v=ke_view(&doc);unsigned hash=2166136261u;for(unsigned i=0;i<v->n;i++)hash=(hash^(unsigned char)v->text[i])*16777619u;
 editor_stats.bytes=v->n;editor_stats.hash=hash;editor_stats.cursor=v->cursor;editor_stats.anchor=v->anchor;editor_stats.dirty=ke_dirty(&doc);editor_stats.mode=mode;editor_stats.top=top;editor_stats.history=doc.current;editor_stats.redo=doc.count-doc.current-1;editor_stats.workspace=destination.flags|(destination.drive<<8);
 editor_stats.ime_scheme=ime.scheme;editor_stats.ime_script=ime.katakana;editor_stats.ime_bytes=ime.n;editor_stats.ime_pending=ime.pending;editor_stats.ime_cursor=ime.cursor;editor_stats.ime_candidates=ime.count;editor_stats.ime_selected=ime.selected;editor_stats.ime_selecting=ime.selecting;editor_stats.ime_target=ime_target;editor_stats.ime_dictionary=dictionary_checked?(ime_dictionary.data?1:2):0;
 hash=2166136261u;for(unsigned i=0;i<ime.n;i++)hash=(hash^(unsigned char)ime.reading[i])*16777619u;editor_stats.ime_hash=hash;
 editor_stats.user_readings=user_dictionary.entries;editor_stats.user_error=user_store.error<0?-user_store.error:0;
}
static void ime_load(void){
 if(dictionary_checked)return;
 dictionary_checked=1;
 ki_store_init(&user_store);int user_error=ki_store_load(&user_store);if(!user_error)ki_user_view(&user_store.words,&user_dictionary);
 int h=k32_open("KGIME.DIC");if(h<0)return;
 unsigned n=0;int error=0;while(n<KI_DICTIONARY){int got=k32_read(h,dictionary_bytes+n,KI_DICTIONARY-n>4096?4096:KI_DICTIONARY-n);if(got<0){error=got;break;}if(!got)break;n+=got;}
 if(!error&&n==KI_DICTIONARY){char extra;int got=k32_read(h,&extra,1);if(got)error=-27;}
 int closed=k32_close(h);if(!error&&closed<0)error=closed;
 if(!error)ki_dictionary(&ime_dictionary,dictionary_bytes,n);
}
static void ime_reload(void){ime_load();int error=ki_store_load(&user_store);if(!error)ki_user_view(&user_store.words,&user_dictionary);notice=error?"User dictionary reload failed / kept":"User dictionary reloaded";}
static unsigned ime_rows(void){unsigned n=(kg_height()-112)/20;return n>6?6:n?n:1;}
static void ime_commit(void){
 char text[KI_TEXT+1];int bytes=ki_commit_text(&ime,text);if(bytes<=0)return;const char *s=text;unsigned n=(unsigned)bytes;
 if(ime_target){if(!kw_insert(ime_target==1?&find_text:&replace_text,s,n)){ime.note="Field limit: 63 bytes / input kept";return;}}
 else if(ke_replace(&doc,s,n)<0){ime.note="Document limit: 64 KiB / input kept";return;}
 ki_clear(&ime);drag=mark=0;
}
static int ime_event(const KgEvent *e){
 int active=ki_active(&ime),editable=(mode==0&&!message)||(mode==5&&(find_focus.current==0||(replacing&&find_focus.current==1)));
 if(e->type==KG_KEY&&e->key==288&&editable){if(active){ime.note="Commit or cancel before reloading words";return 1;}ime_reload();return 1;}
 if(e->type==KG_QUIT&&active){kg_cancel_close();ime.note="Commit or cancel IME, then close again";return 1;}
 if(e->type==KG_KEY&&(e->key==283||e->key==10||e->key==284)&&editable){
  if(active){ime.note="Commit or cancel before changing input";return 1;}
  if(e->key==10)ime.scheme=(ime.scheme+1)%3;
  else if(e->key==283)ime.scheme=ime.scheme?KI_OFF:KI_ROMAJI;
  else ime.scheme=ime.scheme==KI_KANA?KI_ROMAJI:KI_KANA;
  notice=ime.scheme==KI_ROMAJI?"IME: Romaji / F4: off / F5: kana":ime.scheme==KI_KANA?"IME: JIS kana / F4: off / F5: romaji":"IME: Off / F4: romaji";return 1;
 }
 if(!active&&!editable)return 0;
 if(e->type==KG_KEY&&(e->key==285||e->key==286)&&ime.scheme){ime.katakana=e->key==286;ime.selecting=ime.count=0;return 1;}
 if(active){
  if(e->type==KG_KEY){unsigned k=e->key;
   if(k==27){if(ime.selecting){ime.selecting=ime.count=0;ime.note=0;}else ki_clear(&ime);return 1;}
   if(k==26){ki_clear(&ime);return 1;}
   if(k==13){ime_commit();return 1;}
   if(k==32){if(ime.selecting)ki_move_candidate(&ime,(e->control&KG_MOD_SHIFT)?-1:1);else{ime_load();ki_convert_user(&ime,&ime_dictionary,&user_dictionary);}return 1;}
   if(ime.selecting){
    if(k==256||k==257||k==262||k==263)ki_move_candidate(&ime,k==256?-1:k==257?1:k==262?-(int)ime_rows():(int)ime_rows());
    else if(k==260)ime.selected=0;else if(k==261)ime.selected=ime.count-1;
    else if(k>='1'&&k<='6'){unsigned at=ime.selected/ime_rows()*ime_rows()+k-'1';if(k-'1'<ime_rows()&&at<ime.count)ime.selected=at;}
    else ime.note="Esc: edit reading / Enter: commit";
    return 1;
   }
   if(ki_key_event(&ime,k,e->control,&ime_dictionary,&user_dictionary,0))return 1;
   ime.note="Commit or cancel IME before this action";return 1;
  }
  if(kw_button(2190,e)){ime_commit();return 1;}
  if(kw_button(2191,e)){ki_clear(&ime);return 1;}
  if(kw_button(2192,e)||kw_button(2193,e)){if(ime.selecting)ki_move_candidate(&ime,kw_button(2192,e)?-(int)ime_rows():(int)ime_rows());else{ime_load();ki_convert_user(&ime,&ime_dictionary,&user_dictionary);}return 1;}
  for(unsigned i=0;i<ime_rows();i++)if(kw_button(2200+i,e)&&ime.selecting){unsigned at=ime.selected/ime_rows()*ime_rows()+i;if(at<ime.count)ime.selected=at;return 1;}
  return e->type==KG_MOUSE||e->type==KG_POINTER;
 }
 if(e->type==KG_KEY&&ime.scheme&&((e->key>=33&&e->key<=126)||(e->key>=0xa1&&e->key<=0xdf))&&!(e->control&(KG_MOD_CTRL|KG_MOD_ALT))){
  ime_target=mode==5?find_focus.current+1:0;drag=mark=0;ki_key_event(&ime,e->key,e->control,&ime_dictionary,&user_dictionary,0);return 1;
 }
 return 0;
}
static int ime_draw(void){
 unsigned w=kg_width(),h=kg_height(),rows=ime_rows();char text[KI_TEXT+1];ki_preview(&ime,text);
 kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kg_label(8,4,ime_target==1?"IME / Find":ime_target==2?"IME / Replace":"IME / Document",0x20344a);
 kg_label(w-176,4,ime.scheme==KI_KANA?(ime.katakana?"Kana / Kata":"Kana / Hira"):(ime.katakana?"Romaji / Kata":"Romaji / Hira"),0x20344a);
 /* At minimum size show the cursor's whole-character window, at most 2 rows. */
 unsigned n=ki_length(text),capacity=(w-16)/8;if(capacity>47)capacity=47;
 unsigned start=0,visible=capacity*2,caret=ime.cursor+ime.pending;
 if(n>visible){start=caret>capacity?kt_floor(text,n,caret-capacity):0;if(n-start<visible)start=kt_next(text,n,kt_floor(text,n,n-visible));}
 unsigned a=kt_prefix(text+start,n-start,capacity),begin=start;char line[48];for(unsigned i=0;i<a;i++)line[i]=text[start+i];line[a]=0;kg_text(8,24,line,0x174e87);
 start+=a;a=kt_prefix(text+start,n-start,capacity);for(unsigned i=0;i<a;i++)line[i]=text[start+i];line[a]=0;kg_text(8,40,line,0x174e87);
 if(!ime.selecting&&caret>=begin&&caret<=start+a){unsigned second=caret>start||(caret==start&&a);unsigned col=caret-(second?start:begin);if(col<capacity)kg_rect(8+col*8,second?54:38,8,2,0x327ac0);}
 if(ime.selecting){unsigned first=ime.selected/rows*rows;for(unsigned i=0;i<rows&&first+i<ime.count;i++){
   unsigned at=first+i;if(at==ime.selected)kg_rect(4,58+i*20,w-8,20,0x327ac0);
   line[0]='1'+i;line[1]=':';line[2]=' ';unsigned limit=capacity>5?capacity-5:1,bytes=kt_prefix(ime.candidates[at],ki_length(ime.candidates[at]),limit);
   for(unsigned j=0;j<bytes;j++)line[j+3]=ime.candidates[at][j];
   line[bytes+3]=0;
   kg_button(2200+i,8,60+i*20,w-16,18,line);
  }}else kg_label(8,64,"Space: convert / Enter: commit",0x20344a);
 kg_button(2190,8,h-44,88,24,"Commit");kg_button(2191,104,h-44,88,24,"Cancel");kg_button(2192,200,h-44,56,24,"Prev");kg_button(2193,264,h-44,64,24,ime.selecting?"Next":"Convert");
 char status[48]="Candidate ";unsigned p=10;
 if(ime.selected+1>=10){status[p++]='0'+(ime.selected+1)/10;}
 status[p++]='0'+(ime.selected+1)%10;status[p++]='/';
 if(ime.count>=10){status[p++]='0'+ime.count/10;}
 status[p++]='0'+ime.count%10;
 const char *hint=" / PgUp Dn / Esc: reading";for(unsigned i=0;hint[i];i++)status[p++]=hint[i];status[p]=0;
 kw_label(8,h-18,ime.note?ime.note:ime.selecting?status:"F6: hira / F7: kata / Esc: cancel",(w-16)/8,0x20344a);return kg_present();
}
static unsigned content_hash(const char *s,unsigned n){unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^(unsigned char)s[i])*16777619u;return h;}
static void load(const char *name,unsigned work){
 K32Work before=k32_work_name(K32_WORK_STAT,name);if(work&&k32_work(&before)<0){message="Cannot inspect workspace file";return;}
 K32Work opened=k32_work_name(K32_WORK_OPEN,name);
 int h=work?k32_work(&opened):k32_open(name);if(h<0){message="Cannot open file";return;}unsigned n=0;int error=0;
 while(n<KE_BYTES){int got=k32_read(h,load_buffer+n,KE_BYTES-n>4096?4096:KE_BYTES-n);if(got<0){error=got;break;}if(!got)break;n+=got;}
 if(!error&&n==KE_BYTES){char extra;int got=k32_read(h,&extra,1);if(got)error=-27;}
 int closed=k32_close(h);if(!error&&closed<0)error=closed;
 if(work&&!error&&(!before.exists||before.length!=n||before.hash!=content_hash(load_buffer,n)))error=-116;
 if(error||ke_load(&doc,load_buffer,n)){message="Open failed: Shift-JIS, max 64 KiB";editor_stats.error=error?-error:22;return;}
 for(unsigned i=0;i<13;i++){filename[i]=name[i];if(!name[i])break;}
 in_work=work;revision=before;
 kp_workspace_info(&destination);
 top=0;index_lines(1);
}
static int perform(unsigned what){
 if(what==1)return 1;
 if(what==2){ke_load(&doc,"",0);filename[0]=0;in_work=0;top=0;index_lines(1);}
 if(what==3){kp_open(&picker);mode=1;}
 return 0;
}
static int request(unsigned what){drag=mark=0;if(ke_dirty(&doc)){pending=what;mode=2;return 0;}return perform(what);}
static void cancel_pending(void){if(pending==1)kg_cancel_close();pending=0;}
static int commit_save(void){
 KeVersion *v=ke_view(&doc);int serialized=ke_serialize(&doc,load_buffer,sizeof load_buffer);
 if(serialized<0){mode=0;editor_stats.error=-serialized;message="Serialized text exceeds 64 KiB";cancel_pending();return 0;}
 save_request.operation=K32_WORK_SAVE;save_request.flags=v->crlf?K32_WORK_CRLF:0;save_request.data=(unsigned)load_buffer;save_request.bytes=serialized;
 int e=k32_work(&save_request);mode=0;kp_workspace_info(&destination);
 if(e<0){editor_stats.error=-e;recovery_suggested=e==-117||e==-17;message=e==-116?"File changed: reopen or Save As":e==-28?"Disk full: document kept":e==-117?"Recovery needed: keep TMP/BAK":e==-17?"Reserved TMP exists: use another name":"Save failed: document kept";cancel_pending();return 0;}
 for(unsigned i=0;i<13;i++){filename[i]=save_request.name[i];if(!filename[i])break;}
 in_work=1;revision=save_request;revision.exists=1;revision.length=serialized;revision.hash=content_hash(load_buffer,serialized);ke_saved(&doc);editor_stats.error=0;recovery_suggested=0;
 unsigned next=pending;pending=0;if(next)return perform(next);
 if(e==1)message="Saved; old BAK retained for recovery";
 return 0;
}
static int save(int as){
 drag=mark=0;
 if(!as&&in_work&&filename[0]){save_request=revision;return commit_save();}
 int bytes=ke_serialize(&doc,load_buffer,sizeof load_buffer);if(bytes<0){message="Serialized text exceeds 64 KiB";editor_stats.error=-bytes;cancel_pending();return 0;}
 kp_save_size(&picker,in_work?filename:"",bytes);mode=3;return 0;
}
static void copy_selection(int cut){KeVersion *v=ke_view(&doc);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;if(b-a>1024){message="Clipboard limit: 1024 bytes";return;}if(b==a)return;if(kg_clip_set(v->text+a,b-a)<0){message="Clipboard unavailable";return;}if(cut)ke_replace(&doc,"",0);}
static unsigned hit(const KgEvent *e){unsigned r=top+(e->y<40?0:(unsigned)(e->y-40)/16);if(r>=line_count)return ke_view(&doc)->n;unsigned col=e->x<8?0:(unsigned)(e->x-8)/8;if(col>cols)col=cols;unsigned at=lines[r]+col,end=r+1<line_count?lines[r+1]:ke_view(&doc)->n;KeVersion *v=ke_view(&doc);if(end>lines[r]&&v->text[end-1]=='\n')end--;return kt_floor(v->text,v->n,at>end?end:at);}
static void edit_key(unsigned k,unsigned modifiers){
 KeVersion *v=ke_view(&doc);unsigned at=v->cursor,r=row_at(at),col=at-lines[r];
 if(k==1){ke_select(&doc,0,v->n);return;}
 if(k==2){mark=!mark;if(mark)v->anchor=v->cursor;return;}
 if(k==3){copy_selection(0);return;}if(k==24){copy_selection(1);return;}
 if(k==22){int n=kg_clip_get(clipboard,sizeof clipboard);if(n>0&&ke_replace(&doc,clipboard,n)<0)message="Paste rejected: text or size limit";return;}
 if(k==26||k==25){ke_undo(&doc,k==25);return;}
 if(k>=256&&k<=263){
  if(k==258)at=kt_prev(v->text,v->n,at);
  if(k==259)at=kt_next(v->text,v->n,at);
  if(k==260)at=lines[r];
  if(k==261){at=r+1<line_count?lines[r+1]:v->n;if(at>lines[r]&&v->text[at-1]=='\n')at--;}
  if(k==256||k==257||k==262||k==263){unsigned step=k>=262?rows:1;r=(k==256||k==262)?(r>step?r-step:0):(r+step<line_count?r+step:line_count-1);at=lines[r]+col;unsigned end=r+1<line_count?lines[r+1]:v->n;if(end>lines[r]&&v->text[end-1]=='\n')end--;if(at>end)at=end;}
  ke_select(&doc,(mark||(modifiers&KG_MOD_SHIFT))?v->anchor:at,at);return;
 }
 if(k==8||k==264){if(v->anchor==v->cursor)v->anchor=k==8?kt_prev(v->text,v->n,v->cursor):kt_next(v->text,v->n,v->cursor);ke_replace(&doc,"",0);return;}
 char c=k==13?'\n':(char)k;if(k==13||k==9||(k>=32&&k<=126)||(k>=0xa1&&k<=0xdf))if(ke_replace(&doc,&c,1)<0)message="Document limit: 64 KiB";
}
static void search_open(unsigned replace){replacing=replace;find_focus=(KwFocus){.count=replace?6:3};find_note=0;mode=5;drag=mark=0;}
static int search_next(void){KeVersion *v=ke_view(&doc);int e=ke_find(&doc,find_text.text,find_text.length,v->cursor,1);if(e>0){index_lines(1);notice="Found / F3: next / Ctrl H: replace";}else find_note=e<0?"Enter a nonempty search":"Not found";return e;}
static int selected_match(void){KeVersion *v=ke_view(&doc);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;if(b-a!=find_text.length||a==b)return 0;for(unsigned i=0;i<b-a;i++)if(v->text[a+i]!=find_text.text[i])return 0;return 1;}
static void search_event(const KgEvent *e){
 if(e->type==KG_KEY&&e->key==27){mode=0;return;}
 if(kw_focus(&find_focus,e))return;
 if(e->type==KG_MOUSE&&(e->key&1)){if(e->y>=32&&e->y<58)find_focus.current=0;if(replacing&&e->y>=76&&e->y<102)find_focus.current=1;}
 if(e->type==KG_KEY&&e->key==22&&(find_focus.current==0||(replacing&&find_focus.current==1))){int n=kg_clip_get(clipboard,sizeof clipboard);if(n>0&&!kw_insert(find_focus.current?&replace_text:&find_text,clipboard,n))find_note="Paste needs single-line Shift-JIS / max 63 bytes";return;}
 if(!find_focus.current)kw_input(&find_text,e);
 if(replacing&&find_focus.current==1)kw_input(&replace_text,e);
 unsigned action=0;
 if(kw_button(840,e))action=1;
 if(kw_button(841,e))action=2;
 if(kw_button(842,e))action=3;
 if(kw_button(843,e))action=4;
 if(e->type==KG_KEY&&e->key==13){unsigned f=find_focus.current;if(!replacing)action=f==2?4:1;else action=f==0||f==2?1:f==1||f==3?2:f==4?3:4;}
 if(action==4){mode=0;return;}
 if(action==1){if(search_next()>0)mode=0;}
 if(action==2){if(!selected_match()&&search_next()<=0)return;int r=ke_replace(&doc,replace_text.text,replace_text.length);if(r<0)find_note="Replacement exceeds 64 KiB";else{mode=0;notice="Replaced / Ctrl Z: undo";index_lines(1);}}
 if(action==3){int r=ke_replace_all(&doc,find_text.text,find_text.length,replace_text.text,replace_text.length);if(r<0)find_note=r==-27?"Replacement exceeds 64 KiB":"Enter a nonempty search";else if(!r)find_note="Not found";else{mode=0;notice="Replaced all / Ctrl Z: one undo";index_lines(1);}}
}
static int search_draw(void){unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kg_label(8,4,replacing?"Replace / literal, case sensitive":"Find / literal, wraps at end",0x20344a);
 kg_label(8,18,"Find",0x20344a);kw_draw_input((KwRect){8,34,(int)w-20,24},&find_text,find_focus.current==0);
 if(replacing){kg_label(8,62,"Replace with",0x20344a);kw_draw_input((KwRect){8,78,(int)w-20,24},&replace_text,find_focus.current==1);}
 kg_button(840,8,h-72,80,28,"Find");if(replacing){kg_button(841,96,h-72,88,28,"Replace");kg_button(842,192,h-72,88,28,"All");}
 kg_button(843,8,h-40,80,24,"Cancel");kw_label(96,h-36,find_note?find_note:"Tab: controls / Esc: cancel",(w-104)/8,0x20344a);return kg_present();}
static void recover_open(void){message=0;drag=mark=0;kr_open(&recovery,load_buffer);mode=6;}
static void tools_event(const KgEvent *e){
 if(e->type==KG_KEY&&e->key==27){mode=0;return;}
 if(kw_button(620,e)){search_open(0);return;}if(kw_button(621,e)){search_open(1);return;}if(kw_button(622,e)){recover_open();return;}
 if(kw_button(626,e)){mode=8;return;}
 if(kw_button(623,e)){ke_set_eol(&doc,0);mode=0;notice="LF selected / Ctrl Z: undo";}if(kw_button(624,e)){ke_set_eol(&doc,1);mode=0;notice="CRLF selected / Ctrl Z: undo";}if(kw_button(625,e))mode=0;
}
static int tools_draw(void){unsigned h=kg_height(),w=kg_width();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kg_label(8,4,"Editor tools",0x20344a);
 kg_button(620,8,28,128,24,"Find / Ctrl F");kg_button(621,144,28,144,24,"Replace / Ctrl H");kg_button(622,8,60,128,24,"Recovery");
 kg_button(626,144,60,144,24,"IME / F4");
 kg_button(623,8,100,128,24,"Use LF");kg_button(624,144,100,128,24,"Use CRLF");kg_button(625,8,h-48,80,28,"Back");return kg_present();}
static void selection_rects(unsigned a,unsigned b){
 if(a==b)return;
 unsigned first=row_at(a),last=row_at(b),lo=first>top?first:top,hi=last<top+rows?last:top+rows-1;if(lo>hi)return;
 unsigned x=a-lines[first],z=b-lines[last];if(x>cols)x=cols;if(z>cols)z=cols;
 if(lo==hi){unsigned l=lo==first?x:0,r=lo==last?z:cols;if(r>l)kg_rect(8+l*8,40+(lo-top)*16,(r-l)*8,16,0xb4d6f0);return;}
 if(lo==first&&x){kg_rect(8+x*8,40+(lo-top)*16,(cols-x)*8,16,0xb4d6f0);lo++;}
 if(hi==last&&z<cols){if(z)kg_rect(8,40+(hi-top)*16,z*8,16,0xb4d6f0);hi--;}
 if(lo<=hi)kg_rect(8,40+(lo-top)*16,cols*8,(hi-lo+1)*16,0xb4d6f0);
}
static int draw(void){
 layout();budget_rows();report();if(ki_active(&ime))return ime_draw();if(mode==1||mode==3)return kp_draw(&picker);if(mode==5)return search_draw();if(mode==6)return kr_draw(&recovery);if(mode==7)return tools_draw();
 if(mode==8){unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kg_label(8,4,"IME / input settings",0x20344a);
  kg_button(2210,8,30,88,24,"Off");kg_button(2211,104,30,96,24,"Romaji");kg_button(2212,208,30,112,24,"JIS kana");
  kg_label(8,64,ime.scheme==KI_ROMAJI?"Selected: Romaji":ime.scheme==KI_KANA?"Selected: JIS kana":"Selected: Off",0x20344a);
  kg_button(2213,8,88,128,24,"Hiragana / F6");kg_button(2214,144,88,128,24,"Katakana / F7");kg_button(2215,8,h-40,80,24,"Back");kg_button(2216,104,h-40,216,24,"Reload words / F9");return kg_present();}
 unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xffffff);
 if(mode==2){kg_label(8,8,"Unsaved text",0x20344a);kw_label(8,32,"Save before continuing?",(w-16)/8,0x20344a);kg_button(803,8,64,64,28,"Save");kg_button(801,80,64,80,28,"Discard");kg_button(802,8,100,80,28,"Cancel");return kg_present();}
 if(mode==4){kw_label(8,8,"Replace workspace file?",(w-16)/8,0x20344a);kg_text(8,32,save_request.name,0x20344a);kg_button(811,8,64,100,28,"Replace");kg_button(812,8,100,100,28,"Cancel");return kg_present();}
 if(message){kw_draw_dialog(w,h,"Editor",message);if(recovery_suggested)kg_button(627,8,4,104,24,"Recovery");return kg_present();}
 kg_button(601,8,4,64,28,"New");kg_button(602,80,4,64,28,"Open");if(w>=224)kg_button(603,152,4,64,28,"Save");if(w>=312)kg_button(604,224,4,80,28,"Save As");kg_button(605,308,4,w>=380?64:28,28,w>=380?"Tools":"..");
 KeVersion *v=ke_view(&doc);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;
 selection_rects(a,b);
 for(unsigned i=0;i<rows&&i+top<line_count;i++){
  unsigned start=lines[i+top],end=i+top+1<line_count?lines[i+top+1]:v->n;if(end>start&&v->text[end-1]=='\n')end--;
  for(unsigned part=0;start+part<end;){char text[48];unsigned n=kt_prefix(v->text+start+part,end-start-part,47);for(unsigned j=0;j<n;j++){char c=v->text[start+part+j];text[j]=c=='\t'?' ':c;}text[n]=0;kg_text(8+part*8,40+i*16,text,0x20344a);part+=n;}
 }
 unsigned r=row_at(v->cursor);if(r>=top&&r<top+rows){unsigned c=v->cursor-lines[r];if(c<cols)kg_rect(8+c*8,54+(r-top)*16,8,2,0x327ac0);}
 KwScroll scroll={.r={(int)w-28,40,20,(int)rows*16},.total=line_count,.rows=rows,.top=top};kw_draw_scroll(&scroll);
 char status[64];unsigned n=0;const char *parts[]={ke_dirty(&doc)?"Modified / ":"",v->crlf?"CRLF / ":"LF / ",!in_work&&filename[0]?"A: root":destination.root," / ",filename[0]?filename:"Untitled"};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;parts[i][j]&&n<sizeof status-1;j++)status[n++]=parts[i][j];
 status[n]=0;kw_text(8,h-32,status,(w-16)/8,0x466581);
 kw_label(8,h-16,notice?notice:"Ctrl F:Find H:Replace T:Recovery / Tools",(w-16)/8,0x466581);return kg_present();
}
int main(int argc,char **argv){
 ke_load(&doc,"",0);if(kg_open("Editor / workspace TXT",576,320)<0)return 1;
 if((kg_capabilities()&KG_CAP_SUPPORTED)!=KG_CAP_SUPPORTED||kg_subscribe(KG_CAP_SUPPORTED))return 2;
 index_lines(1);if(!(k32_query()&K32_FEATURE_SAVE))return 2;
 kp_workspace_info(&destination);
 if(argc>1&&k32_len(argv[1])==5&&argv[1][0]=='f'&&argv[1][1]=='a'&&argv[1][2]=='u'&&argv[1][3]=='l'&&argv[1][4]=='t'){load("EDITTEST.TXT",0);draw();*(volatile u32 *)0x10000=1;}
 if(argc>1){
  unsigned work=argc==3&&k32_len(argv[1])==4&&argv[1][0]=='w'&&argv[1][1]=='o'&&argv[1][2]=='r'&&argv[1][3]=='k';
  unsigned root=argc==3&&k32_len(argv[1])==4&&argv[1][0]=='r'&&argv[1][1]=='o'&&argv[1][2]=='o'&&argv[1][3]=='t';char command[64];
  if((!root&&!work)||kg_document_command(work,argv[2],command)){message="Invalid document request";editor_stats.error=22;}
  else load(argv[2],work);
 }
 int dirty=1,quit=0;
 while(!quit){KgEvent e;int got=kg_poll(&e);editor_stats.heartbeat++;if(got<0)break;
  if((mode==1||mode==3)&&kp_tick(&picker))dirty=1;
  if(mode==6&&kr_tick(&recovery))dirty=1;
  if(got){editor_stats.events++;dirty=1;
   /* Resize/focus apply to the document even while a modal owns input. */
   if(e.type==KG_RESIZE){editor_scroll.dragging=0;drag=0;index_lines(1);}
   if(e.type==KG_FOCUS&&!e.key){drag=0;editor_scroll.dragging=0;}
   if(ime_event(&e)){index_lines(1);}
   else if(e.type==KG_QUIT){if(mode==1||mode==3)kp_close(&picker);if(mode==6)kr_close(&recovery);mode=0;message=0;pending=0;quit=request(1);}
   else if(mode==5)search_event(&e);
   else if(mode==6){unsigned changed=recovery.changed;if(kr_event(&recovery,&e)){mode=0;index_lines(1);}if(recovery.changed!=changed)editor_stats.error=0;}
   else if(mode==7)tools_event(&e);
   else if(mode==8){
    if(kw_button(2216,&e)||(e.type==KG_KEY&&e.key==288)){ime_reload();mode=0;}
    if(kw_button(2210,&e))ime.scheme=KI_OFF;
    if(kw_button(2211,&e))ime.scheme=KI_ROMAJI;
    if(kw_button(2212,&e))ime.scheme=KI_KANA;
    if(kw_button(2213,&e))ime.katakana=0;
    if(kw_button(2214,&e))ime.katakana=1;
    if(kw_button(2215,&e)||(e.type==KG_KEY&&e.key==27))mode=0;
   }
   else if(mode==2){if(kw_button(803,&e)||(e.type==KG_KEY&&e.key=='s')){mode=0;quit=save(0);}else if(kw_button(801,&e)||(e.type==KG_KEY&&e.key=='d')){mode=0;unsigned next=pending;pending=0;quit=perform(next);}else if(kw_button(802,&e)||(e.type==KG_KEY&&e.key==27)){mode=0;cancel_pending();}}
   else if(mode==1||mode==3){unsigned was=mode;char name[13];int selected=kp_event(&picker,&e,name);if(selected){mode=0;if(selected<0)cancel_pending();else if(was==1)load(name,picker.work);else{save_request=k32_work_name(K32_WORK_STAT,name);int error=k32_work(&save_request);if(error<0){message="Workspace unavailable / file protected";editor_stats.error=-error;cancel_pending();}else if(save_request.exists)mode=4;else quit=commit_save();}}}
   else if(mode==4){if(kw_button(811,&e)||(e.type==KG_KEY&&e.key=='r'))quit=commit_save();else if(kw_button(812,&e)||(e.type==KG_KEY&&e.key==27)){mode=0;cancel_pending();}}
   else if(message){if(kw_button(627,&e)||(e.type==KG_KEY&&e.key==20))recover_open();else if(kw_dialog(&e))message=0;}
   else if(kw_button(605,&e)){mode=7;drag=mark=0;}
   else if(e.type==KG_KEY&&e.key==6)search_open(0);
   else if(e.type==KG_KEY&&e.key==8&&(e.control&KG_MOD_CTRL))search_open(1);
   else if(e.type==KG_KEY&&e.key==20)recover_open();
   else if(e.type==KG_KEY&&e.key==282){if(!find_text.length)search_open(0);else search_next();}
   else if(kw_button(601,&e)||(e.type==KG_KEY&&e.key==14))quit=request(2);
   else if(kw_button(602,&e)||(e.type==KG_KEY&&e.key==15))quit=request(3);
   else if(kw_button(603,&e)||(e.type==KG_KEY&&e.key==19))quit=save(0);
   else if(kw_button(604,&e)||(e.type==KG_KEY&&e.key==23))quit=save(1);
   else if(e.type==KG_KEY){notice=0;edit_key(e.key,e.control);index_lines(1);}
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
 if(mode==1||mode==3)kp_close(&picker);
 if(mode==6)kr_close(&recovery);
 return kg_close()?3:0;
}
