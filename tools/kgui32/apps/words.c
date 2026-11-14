/* A second independent consumer of the common IME, with explicit persistence. */
#include "../sdk/kgui.h"
#include "../sdk/widgets.h"
#include "../sdk/text.h"
#include "../sdk/ime-store.h"
#include "../../k38632/sdk/libc.h"
#include "ui-words.h"
volatile unsigned words_stats[20] __attribute__((section(".data.gui_stats")))={0x32574b47,1};
static KiStore store;static KiIme ime;static KiDictionary system_dictionary,user_dictionary;
static unsigned char system_bytes[KI_DICTIONARY];static KwInput reading,value;
static unsigned field,modal,inspected;static K32Revision inspected_revision;static const char *note;
static unsigned digest(const void *p,unsigned n){const unsigned char *s=p;unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^s[i])*16777619u;return h;}
static void refresh_user(void){if(ki_user_view(&store.words,&user_dictionary))user_dictionary=(KiDictionary){0};}
static void report(void){words_stats[4]=digest(reading.text,reading.length);words_stats[5]=digest(value.text,value.length);words_stats[6]=store.words.dirty;words_stats[7]=user_dictionary.entries;words_stats[10]=field;words_stats[11]=ime.n;words_stats[12]=ime.pending;words_stats[13]=ime.count;words_stats[14]=ime.selected;words_stats[15]=modal;words_stats[16]=store.words.bytes;words_stats[17]=digest(store.words.data,store.words.bytes);words_stats[18]=inspected;words_stats[19]=ime.scheme;}
static void system_load(void){
 int h=k32_open("KGIME.DIC");if(h<0)return;unsigned n=0;int error=0;
 while(n<sizeof system_bytes){unsigned count=sizeof system_bytes-n;if(count>4096)count=4096;int got=k32_read((unsigned)h,system_bytes+n,count);if(got<0){error=got;break;}if(!got)break;n+=(unsigned)got;}
 if(!error&&n==sizeof system_bytes){char extra;int got=k32_read((unsigned)h,&extra,1);if(got)error=-27;}
 if(k32_close((unsigned)h))error=-5;
 if(!error)ki_dictionary(&system_dictionary,system_bytes,n);
}
static const char *error_note(int e){return e==-116?"File changed / edits kept":e==-17?"TMP or destination exists / kept":e==-117?"Inspect TMP/BAK / original kept":e==-28?"Disk full / original kept":e==-13?"Protected file / original kept":e==-16?"Commit or cancel input / file busy":"Dictionary error / original kept";}
static void failure(int e){words_stats[9]=e<0?(unsigned)-e:0;note=e<0?error_note(e):"Saved to Work";}
static unsigned candidate_rows(void){unsigned n=(kg_height()-104)/20;return n>6?6:n?n:1;}
static int draw(void){
 unsigned w=kg_width(),h=kg_height();report();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 if(modal){kg_label(8,8,modal==2?"Unsaved user dictionary":"Reload user dictionary?",0x20344a);kg_label(8,36,"Save, discard or cancel",0x20344a);kg_button(201,8,72,88,28,"Save");kg_button(202,104,72,88,28,"Discard");kg_button(203,200,72,88,28,"Cancel");if(note)kw_label(8,h-20,note,(w-16)/8,0x20344a);return kg_present();}
 if(ki_active(&ime)){
  char text[KI_TEXT+1],line[48];ki_preview(&ime,text);kg_label(8,4,field?"IME / Word":"IME / Reading",0x20344a);kw_text(8,28,text,(w-16)/8>47?47:(w-16)/8,0x174e87);
  unsigned rows=candidate_rows(),first=ime.selected/rows*rows;
  if(ime.selecting)for(unsigned i=0;i<rows&&first+i<ime.count;i++){
   unsigned index=first+i,n=kt_prefix(ime.candidates[index],ki_length(ime.candidates[index]),(w-40)/8>43?43:(w-40)/8);line[0]=index==ime.selected?'>':' ';line[1]='1'+i;line[2]=' ';for(unsigned j=0;j<n;j++)line[j+3]=ime.candidates[index][j];line[n+3]=0;kg_button(300+i,8,56+i*20,w-16,18,line);
  }
  else kg_label(8,64,"Space: convert / Enter: commit",0x20344a);
  kg_button(310,8,h-44,88,24,"Commit");kg_button(311,104,h-44,88,24,"Cancel");kg_button(312,200,h-44,56,24,"Prev");kg_button(313,264,h-44,64,24,"Next");kw_label(8,h-18,ime.note?ime.note:"F4: off / F5: kana / F6 F7: script",(w-16)/8,0x20344a);return kg_present();
 }
 kg_label(8,4,"User dictionary",0x20344a);kg_label(8,28,"Reading",0x20344a);kw_draw_input((KwRect){80,24,(int)w-88,24},&reading,!field);kg_label(8,64,"Word",0x20344a);kw_draw_input((KwRect){80,60,(int)w-88,24},&value,field);
 kg_button(101,8,h-76,72,24,"Add");kg_button(102,88,h-76,72,24,"Delete");kg_button(103,168,h-76,72,24,"Save");kg_button(104,248,h-76,88,24,"Reload");
 kg_button(105,8,h-44,96,24,"Inspect TMP");kg_button(106,112,h-44,96,24,"Inspect BAK");kg_button(107,216,h-44,112,24,"Restore");
 char status[48];k32_snprintf(status,sizeof status,KU("%u readings / %s / F4 IME / F9 reload"),user_dictionary.entries,KU(store.words.dirty?"modified":"saved"));kw_label(8,h-18,note?note:status,(w-16)/8,0x20344a);return kg_present();
}
static int save(void){if(reading.length||value.length)return -16;return ki_store_save(&store);}
static int reload(void){int e=ki_store_load(&store);if(!e){refresh_user();inspected=0;note="User dictionary reloaded";}return e;}
static void clear_fields(void){reading=value=(KwInput){0};}
static void accept(void){char text[KI_TEXT+1];int n=ki_commit_text(&ime,text);if(n>0){if(kw_insert(field?&value:&reading,text,(unsigned)n))ki_clear(&ime);else ime.note="Field limit / input kept";}}
int main(void){
 if(kg_open("User dictionary",480,264)<0)return 1;
 if(kg_subscribe(KG_INPUT_TAB|KG_INPUT_KEYS|KG_INPUT_CLOSE|KG_INPUT_MODIFIERS))return 2;
 ki_store_init(&store);int e=ki_store_load(&store);if(e)failure(e);refresh_user();system_load();int dirty=1,quit=0;
 while(!quit){KgEvent event;int got=kg_poll(&event);words_stats[8]++;if(got<0)return 3;
  if(got){words_stats[3]++;dirty=1;
   if(event.type==KG_QUIT){if(ki_active(&ime)){kg_cancel_close();ime.note="Commit or cancel before closing";}else if(store.words.dirty||reading.length||value.length){kg_cancel_close();modal=2;}else quit=1;}
   else if(modal){unsigned action=event.type==KG_MOUSE&&(event.key&1)?event.control:event.type==KG_KEY?(event.key==27?203:event.key=='s'?201:event.key=='d'?202:0):0;
    if(action==203){modal=0;note=0;}else if(action==202){if(modal==2)quit=1;else{e=reload();failure(e);}modal=0;}else if(action==201){e=save();failure(e);if(e>=0){if(modal==2)quit=1;else reload();modal=0;}}
   }else if(event.type==KG_KEY){
    unsigned key=event.key;
    if(ime.selecting&&key>='1'&&key<='6'){unsigned index=ime.selected/candidate_rows()*candidate_rows()+key-'1';if(index<ime.count)ime.selected=index;}
    else {char text[KI_TEXT+1];int consumed=ki_key_event(&ime,key,event.control,&system_dictionary,&user_dictionary,text);
     if(consumed==2){if(kw_insert(field?&value:&reading,text,ki_length(text)))ki_clear(&ime);else ime.note="Field limit / input kept";}
     else if(!consumed){
      if(key==9)field^=1;
      else if(key==19){e=save();failure(e);}
      else if(key==288){if(store.words.dirty)modal=1;else {e=reload();if(e)failure(e);}}
      else if(key==22){char pasted[64];int n=kg_clip_get(pasted,63);if(n>0&&!kw_insert(field?&value:&reading,pasted,(unsigned)n))note="Field limit / clipboard kept";}
      else if(key==21){*(field?&value:&reading)=(KwInput){0};}
      else if(key==13)field^=1;
      else {kw_input(field?&value:&reading,&event);note=0;}
     }
    }
   }else if(event.type==KG_MOUSE&&(event.key&1)){
    if(ki_active(&ime)){
     if(event.control==310)accept();else if(event.control==311)ki_clear(&ime);else if(event.control==312||event.control==313){if(ime.selecting)ki_move_candidate(&ime,event.control==312?-1:1);else ki_convert_user(&ime,&system_dictionary,&user_dictionary);}else if(event.control>=300&&event.control<300+candidate_rows()&&ime.selecting){unsigned at=ime.selected/candidate_rows()*candidate_rows()+event.control-300;if(at<ime.count)ime.selected=at;}else ime.note="Commit or cancel before this action";
    }else{
     unsigned action=event.control;e=0;
     if(kw_inside((KwRect){80,24,(int)kg_width()-88,24},event.x,event.y))field=0;
     else if(kw_inside((KwRect){80,60,(int)kg_width()-88,24},event.x,event.y))field=1;
     else if(action==101){e=ki_user_add(&store.words,reading.text,value.text);if(!e){clear_fields();refresh_user();note="Added / save explicitly";}}
     else if(action==102){e=ki_user_remove(&store.words,reading.text);if(!e){clear_fields();refresh_user();note="Deleted / save explicitly";}}
     else if(action==103){e=save();if(e>=0)note=e?"Saved / BAK retained":"Saved to Work";}
     else if(action==104){if(store.words.dirty)modal=1;else e=reload();}
     else if(action==105||action==106){KiDictionary candidate;unsigned variant=action-104;e=ki_store_inspect(variant,&candidate,&inspected_revision);if(!e){inspected=variant;note="Valid candidate / restore to RECIME.DIC";}}
     else if(action==107){e=inspected?ki_store_restore(inspected,"RECIME.DIC",&inspected_revision):-22;if(!e){inspected=0;note="Recovered / active dictionary kept";}}
     if(e<0)failure(e);else words_stats[9]=0;
    }
   }
  }
  if(dirty&&!quit){int result=draw();if(result<0&&result!=-22){words_stats[9]=(unsigned)-result;return 4;}if(!result){dirty=0;words_stats[2]++;}}
  k32_wait(K32_WAIT_TIME,16);
 }
 return kg_close()?5:0;
}
