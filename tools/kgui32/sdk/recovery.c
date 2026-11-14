#include "recovery.h"
#include "text.h"
#include "ui-recovery.h"
static void copy(char *to,const char *from){unsigned i=0;for(;i<12&&from[i];i++)to[i]=from[i];to[i]=0;}
static int same(const char *a,const char *b){unsigned i=0;for(;a[i]&&a[i]==b[i];i++);return a[i]==b[i];}
static unsigned hash(const char *p,unsigned n){unsigned h=2166136261u;while(n--)h=(h^(unsigned char)*p++)*16777619u;return h;}
static void number(char *to,unsigned v){char b[11];unsigned n=0,i=0;do{b[n++]='0'+v%10;v/=10;}while(v);while(n)to[i++]=b[--n];to[i]=0;}
void kr_close(KrRecovery *r){if(r->cursor)k32_dir_close(r->cursor);r->cursor=r->loading=0;}
static void refresh(KrRecovery *r){kr_close(r);r->count=r->list.count=r->list.selected=r->list.top=0;r->stage=r->loading=1;r->mode=r->error=0;r->focus=(KwFocus){.count=4};}
void kr_open(KrRecovery *r,char *preview){r->preview=preview;r->note=0;r->changed=0;r->restored[0]=0;kp_workspace_info(&r->location);refresh(r);}
int kr_tick(KrRecovery *r){
 if(!r->loading)return 0;
 for(unsigned step=0;step<4&&r->loading;step++){
  K32DirEntry e;int got=r->cursor?k32_dir_next(r->cursor,&e):k32_recovery_first(r->stage,&e);
  if(got<0){r->error=got;kr_close(r);break;}
  r->cursor=e.cursor;
  if(!got){kr_close(r);if(++r->stage<=2)r->loading=1;break;}
  if(e.attributes&16)continue;
  char name[13],path[24];copy(name,e.name);unsigned n=k32_len(name);if(n<5)continue;
  name[n-3]='T';name[n-2]='X';name[n-1]='T';if(k32_work_path(name,path))continue;
  unsigned i=0;for(;i<r->count;i++)if(same(r->names[i],name))break;if(i<r->count)continue;
  if(r->count==KR_ENTRIES){r->error=-28;kr_close(r);break;}copy(r->names[r->count++],name);
 }
 r->list.count=r->count;kw_reveal(&r->list);return 1;
}
static void layout(KrRecovery *r){
 unsigned h=kg_height(),w=kg_width();r->cols=(w-40)/8;if(r->cols>47)r->cols=47;
 r->rows=(h-156)/16;if(!r->rows)r->rows=1;
 r->list.rows=(h-112)/16;if(r->list.rows>18)r->list.rows=18;
 kw_reveal(&r->list);
}
static void index_preview(KrRecovery *r){
 r->lines=r->top=0;unsigned col=0,start=1;
 for(unsigned at=0;at<r->bytes;){unsigned c=(unsigned char)r->preview[at];if(c=='\r'){at++;continue;}unsigned n=kt_next(r->preview,r->bytes,at)-at;if(c!='\n'&&col+n>r->cols){start=1;col=0;}if(start){r->offsets[r->lines++]=at;start=0;}if(c=='\n'){start=1;col=0;}else {col+=n;if(col==r->cols){start=1;col=0;}}at+=n;}
}
static void preview(KrRecovery *r,unsigned kind){
 r->kind=kind;r->preview_valid=0;r->bytes=r->lines=r->top=0;r->error=0;
 K32WorkRevision v=r->info.file[kind];if(v.error||!v.exists){r->error=v.error?v.error:-2;return;}
 if(v.length>K32_WORK_LIMIT){r->error=-27;return;}
 int h=k32_recovery_open(r->base,kind);if(h<0){r->error=h;return;}
 while(r->bytes<v.length){unsigned n=v.length-r->bytes;if(n>4096)n=4096;int got=k32_read(h,r->preview+r->bytes,n);if(got<=0){r->error=got?got:-5;break;}r->bytes+=got;}
 int closed=k32_close(h);if(!r->error&&closed<0)r->error=closed;
 if(!r->error&&(r->bytes!=v.length||hash(r->preview,r->bytes)!=v.hash))r->error=-116;
 if(!r->error&&!kt_valid(r->preview,r->bytes,KT_LINES|KT_CRLF))r->error=-22;
 if(r->error){r->bytes=0;return;}
 r->preview_valid=1;index_preview(r);
}
static void inspect(KrRecovery *r){
 if(r->loading||!r->count)return;
 kr_close(r);copy(r->base,r->names[r->list.selected]);
 r->error=k32_recovery_info(r->base,&r->info);r->mode=1;r->focus=(KwFocus){.count=7,.current=1};
 r->name=(KwInput){0};copy(r->name.text,r->info.file[0].exists?"RESCUED.TXT":r->base);r->name.length=r->name.cursor=k32_len(r->name.text);
 if(!r->error)preview(r,r->info.file[2].exists?2:1);
}
static void prepare(KrRecovery *r){
 char path[24];if(!r->kind||!r->info.file[r->kind].exists||r->info.file[r->kind].error||!r->preview_valid){r->note="Select a readable TMP or BAK";return;}
 if(k32_work_path(r->name.text,path)){r->error=-22;return;}
 copy(r->request.destination,path+8);r->request.expected=r->info;r->error=0;r->mode=2;
}
int kr_event(KrRecovery *r,const KgEvent *e){
 layout(r);
 if(e->type==KG_RESIZE&&r->preview_valid)index_preview(r);
 if(r->mode==2){
  if(kw_button(870,e)||(e->type==KG_KEY&&e->key==13)){
   r->error=k32_recovery_restore(r->base,r->kind,&r->request);r->mode=1;
   if(!r->error){copy(r->restored,r->request.destination);r->changed++;r->note="Recovered as / other files kept";refresh(r);}return 0;
  }
  if(kw_button(871,e)||(e->type==KG_KEY&&e->key==27))r->mode=1;
  return 0;
 }
 if(e->type==KG_KEY&&e->key==27){if(r->mode){refresh(r);return 0;}kr_close(r);return 1;}
 if(kw_focus(&r->focus,e))return 0;
 if(!r->mode){
  if(e->type==KG_MOUSE&&(e->key&1)&&e->y>=44&&e->y<(int)kg_height()-68)r->focus.current=0;
  if(!r->focus.current)kw_list(&r->list,(KwRect){8,44,(int)kg_width()-40,(int)r->list.rows*16},e);
  if(kw_button(850,e)||(e->type==KG_KEY&&e->key==13&&r->focus.current<2))inspect(r);
  if(kw_button(863,e)||(e->type==KG_KEY&&e->key==13&&r->focus.current==2))refresh(r);
  if(kw_button(862,e)||(e->type==KG_KEY&&e->key==13&&r->focus.current==3)){kr_close(r);return 1;}
 }else{
  if(e->type==KG_MOUSE&&(e->key&1)&&e->y>=(int)kg_height()-108&&e->y<(int)kg_height()-84)r->focus.current=1;
  if(r->focus.current==1)kw_input(&r->name,e);
  for(unsigned i=0;i<3;i++)if(kw_button(851+i,e)||(e->type==KG_KEY&&e->key==13&&r->focus.current==2+i))preview(r,i);
  if(e->type==KG_KEY){unsigned max=r->lines>r->rows?r->lines-r->rows:0;
   if(e->key==262)r->top=r->top>r->rows?r->top-r->rows:0;
   if(e->key==263)r->top=r->top+r->rows<max?r->top+r->rows:max;
  }
  if(kw_button(860,e)||(e->type==KG_KEY&&e->key==13&&(r->focus.current==1||r->focus.current==5)))prepare(r);
  if(kw_button(861,e))refresh(r);
  if(kw_button(862,e)||(e->type==KG_KEY&&e->key==13&&r->focus.current==6)){kr_close(r);return 1;}
 }
 return 0;
}
static const char *error_text(int e){return e==-116?"Changed: Back then Refresh":e==-17?"Destination exists: use another TXT":e==-13?"Protected file: original kept":e==-27?"Over 64 KiB: original kept":e==-22?"Use Shift-JIS / valid 8.3 TXT":e==-2?"Candidate missing":e?"I/O error: original kept":0;}
int kr_draw(KrRecovery *r){
 layout(r);unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 kg_label(8,4,r->mode?"Work recovery / inspect":"Work recovery / TMP and BAK",0x20344a);
 if(r->mode==2){
  kg_label(8,28,"Move selected candidate to unused TXT?",0x20344a);kg_text(8,52,r->base,0x20344a);kg_label(8,76,r->kind==1?"Source: TMP":"Source: BAK",0x20344a);kg_text(8,100,r->request.destination,0x20344a);
  kg_button(870,8,h-68,88,28,"Move");kg_button(871,104,h-68,88,28,"Cancel");kw_label(8,h-24,"Existing TXT and other candidates kept",(w-16)/8,0x20344a);return kg_present();
 }
 if(!r->mode){
  kw_text(8,24,r->changed?r->restored:r->location.root,(w-16)/8,0x20344a);
  if(r->list.count&&r->list.selected>=r->list.top&&r->list.selected<r->list.top+r->list.rows)kg_rect(8,44+(r->list.selected-r->list.top)*16,w-40,16,0xb4d6f0);
  for(unsigned i=0;i<r->list.rows&&i+r->list.top<r->count;i++)kg_text(12,44+i*16,r->names[i+r->list.top],0x20344a);
  kg_button(850,8,h-64,88,28,"Inspect");kg_button(863,104,h-64,88,28,"Refresh");kg_button(862,200,h-64,72,28,"Close");
 }else{
  const char *label[3]={"TXT present","TMP present","BAK present"},*absent[3]={"TXT absent","TMP absent","BAK absent"},*bad[3]={"TXT error","TMP error","BAK error"};
  for(unsigned i=0;i<3;i++)kg_button(851+i,8+i*((w-16)/3),24,(w-24)/3,24,r->info.file[i].error?bad[i]:r->info.file[i].exists?label[i]:absent[i]);
  for(unsigned row=0;row<r->rows&&row+r->top<r->lines;row++){
   unsigned at=r->offsets[row+r->top],n=0;char line[48];
   while(at<r->bytes&&n<r->cols&&r->preview[at]!='\n'){char c=r->preview[at];if(c=='\r'){at++;continue;}unsigned k=kt_next(r->preview,r->bytes,at)-at;if(k>r->cols-n)break;for(unsigned j=0;j<k;j++)line[n++]=c=='\t'?' ':r->preview[at+j];at+=k;}line[n]=0;kg_text(8,52+row*16,line,0x20344a);
  }
  kg_label(8,h-102,"Save as",0x20344a);kw_draw_input((KwRect){88,(int)h-108,(int)w-96,24},&r->name,r->focus.current==1);
  kg_button(860,8,h-76,104,28,"Restore");kg_button(861,120,h-76,72,28,"Back");kg_button(862,200,h-76,72,28,"Close");
  char n[11];number(n,r->bytes);kg_text(8,h-40,n,0x466581);kg_label(104,h-40,r->kind==1?"TMP preview":r->kind==2?"BAK preview":"TXT preview",0x466581);
 }
 const char *err=error_text(r->error);kw_label(8,h-20,err?err:r->loading?"Loading...":r->note?r->note:r->count?"Inspect before explicit recovery":"No TMP/BAK candidates",(w-16)/8,0x20344a);
 return kg_present();
}
