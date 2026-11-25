#include "picker.h"
#include "../../k38632/sdk/libc.h"
#include "ui-picker.h"
static unsigned upper(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int match(const char *s,const char *f){if(!*f)return 1;for(;*s;s++){unsigned i=0;while(f[i]&&s[i]&&upper(f[i])==upper(s[i]))i++;if(!f[i])return 1;}return 0;}
static void rebuild(KpPicker *p){p->list.count=0;for(unsigned i=0;i<p->count;i++)if(match(p->entries[i].name,p->filter.text))p->matches[p->list.count++]=i;kw_reveal(&p->list);}
void kp_workspace_info(K32WorkInfo *out){
 *out=(K32WorkInfo){.flags=K32_WORK_FD,.max_bytes=K32_WORK_LIMIT,.root="A:\\WORK"};
 K32WorkInfo got;int e=k32_work_info(&got);
 if(e==-38)return; /* SDK4 hosts without the optional information operation */
 if(e<0){out->error=e;return;}
 if(got.version!=1||got.size!=sizeof got||got.drive>25||got.root[0]!=(char)('A'+got.drive)||got.root[1]!=':'||got.root[2]!='\\'||got.root[3]!='W'||got.root[4]!='O'||got.root[5]!='R'||got.root[6]!='K'||got.root[7]){out->error=-22;return;}
 *out=got;
}
static void browse(KpPicker *p){
 p->count=p->cursor=p->list.selected=p->list.top=p->list.count=0;
 p->scroll.dragging=0;p->loading=1;p->error=0;p->filter=(KwInput){0};
}
static void volume(KpPicker *p,unsigned next){p->volume=next;p->work=!next;p->folder[0]=0;browse(p);}
static void up(KpPicker *p){
 char *last=0;
 for(char *c=p->folder;*c;c++)if(*c=='\\')last=c;
 if(last)*last=0;else p->folder[0]=0;
 browse(p);
}
static int join(char out[K32_FS_PATH],const char *folder,const char *name){
 unsigned n=k32_len(folder),m=k32_len(name);
 if(!m||n+(n!=0)+m>=K32_FS_PATH)return 0;
 k32_memcpy(out,folder,n);if(n)out[n++]='\\';k32_memcpy(out+n,name,m+1);return 1;
}
void kp_close(KpPicker *p){p->cursor=p->loading=0;}
void kp_open_kind(KpPicker *p,unsigned kind){
 *p=(KpPicker){.focus={.count=6},.list={.rows=1},.kind=kind};
 kp_workspace_info(&p->location);
 K32Fs drives=k32_fs_request(K32_FS_DRIVES,0,0);
 p->drives=k32_fs(&drives)?1:drives.length;
 volume(p,p->location.flags&K32_WORK_HDD?0:1);
}
void kp_open(KpPicker *p){kp_open_kind(p,0);}
void kp_save_kind(KpPicker *p,const char *name,unsigned bytes,unsigned kind){
 kp_open_kind(p,kind);volume(p,0);p->document_bytes=bytes;p->saving=1;p->focus.count=7;p->focus.current=2;
 for(unsigned i=0;i<12&&name[i];i++){p->name.text[i]=name[i];p->name.length++;}
 p->name.cursor=p->name.length;
}
void kp_save_size(KpPicker *p,const char *name,unsigned bytes){kp_save_kind(p,name,bytes,0);}
void kp_save(KpPicker *p,const char *name){kp_save_size(p,name,0);}
int kp_tick(KpPicker *p){
 if(!p->loading)return 0;
 for(unsigned i=0;i<4;i++){
  K32Fs r=k32_fs_request(p->cursor?K32_FS_NEXT:p->work?K32_FS_LIST:K32_FS_DRIVE_LIST,p->folder,0);
  r.flags=p->cursor||p->work?0:p->volume-1;r.token=p->cursor;
  int e=k32_fs(&r);p->cursor=r.token;
  if(e||!p->cursor){p->error=e;kp_close(p);break;}
  char path[24];
  if(!(r.entry.attributes&16)&&k32_work_path_kind_at(0,r.entry.name,p->kind,path))continue;
  if(p->count==KP_ENTRIES){p->error=-28;kp_close(p);break;}
  p->entries[p->count++]=r.entry;
 }
 rebuild(p);return 1;
}
static void layout(KpPicker *p){
 unsigned h=kg_height(),limit=p->saving?128:96;
 p->list.rows=h>limit?(h-limit)/16:1;if(p->list.rows>20)p->list.rows=20;
 kw_reveal(&p->list);
 p->scroll.r=(KwRect){(int)kg_width()-28,44,20,(int)p->list.rows*16};
 p->scroll.total=p->list.count;p->scroll.rows=p->list.rows;p->scroll.top=p->list.top;
}
int kp_event(KpPicker *p,const KgEvent *e,char name[13]){
 layout(p);
 unsigned key=e->type==KG_KEY?e->key:0;
 unsigned cancel=p->saving?4:3,drive=cancel+1,parent=cancel+2;
 if(key==27||kw_button(702,e)||(key==13&&p->focus.current==cancel)){kp_close(p);return -1;}
 if(key==23){volume(p,p->work?1:0);return 0;}
 if(kw_button(703,e)||key==4||(key==13&&p->focus.current==drive)){
  unsigned next=p->volume;
  do{next=next==26?0:next+1;}while(next&&!(p->drives&(1u<<(next-1))));
  volume(p,next);return 0;
 }
 if(kw_button(704,e)||(key==13&&p->focus.current==parent)||(key==8&&p->focus.current==1)){up(p);return 0;}
 if(kw_focus(&p->focus,e))return 0;
 if(e->type==KG_MOUSE&&(e->key&1)){
  if(p->saving&&e->y>=(int)kg_height()-80&&e->y<(int)kg_height()-48)p->focus.current=2;
  else if(e->y<40)p->focus.current=0;
  else if(e->y<(int)kg_height()-(p->saving?80:48))p->focus.current=1;
 }
 if(!p->focus.current&&kw_input(&p->filter,e)==2){p->list.selected=p->list.top=0;rebuild(p);}
 if(p->saving&&p->focus.current==2)kw_input(&p->name,e);
 int selected=p->focus.current==1&&kw_list(&p->list,(KwRect){8,44,(int)kg_width()-40,(int)p->list.rows*16},e);
 K32DirEntry *d=p->list.count?&p->entries[p->matches[p->list.selected]]:0;
 if(selected&&p->saving&&d&&!(d->attributes&16)){
  p->name=(KwInput){0};
  for(unsigned i=0;i<12&&d->name[i];i++)p->name.text[p->name.length++]=d->name[i];
  p->name.cursor=p->name.length;
 }
 if(kw_scroll(&p->scroll,e)){
  p->list.top=p->scroll.top;
  if(p->list.selected<p->list.top||p->list.selected>=p->list.top+p->list.rows)p->list.selected=p->list.top;
 }
 if(!kw_button(701,e)&&!(key==13&&p->focus.current&&p->focus.current<cancel))return 0;
 if(d&&(d->attributes&16)&&(!p->saving||p->focus.current==1||!p->work)){
  char folder[K32_FS_PATH];
  if(!join(folder,p->folder,d->name)){p->error=-22;return 0;}
  k32_memcpy(p->folder,folder,k32_len(folder)+1);browse(p);return 0;
 }
 if(p->saving){
  char fixed[24];
  if(!p->work){p->error=-13;return 0;}
  if(k32_work_path_kind_at(0,p->name.text,p->kind,fixed)){p->error=-22;return 0;}
  k32_memcpy(name,fixed+8,k32_len(fixed+8)+1);
 }else{
  if(!d||p->loading)return 0;
  k32_memcpy(name,d->name,k32_len(d->name)+1);
 }
 if(!join(p->target,p->folder,name)){p->error=-22;return 0;}
 kp_close(p);return 1;
}
static void append(char *s,unsigned *n,const char *text){while(*text&&*n<79)s[(*n)++]=*text++;s[*n]=0;}
static void number(char *s,unsigned *n,unsigned value){char b[11];unsigned i=10;b[i]=0;do{b[--i]='0'+value%10;value/=10;}while(value);append(s,n,b+i);}
int kp_draw(KpPicker *p){layout(p);unsigned w=kg_width(),h=kg_height();char title[80]={0},status[80]={0};unsigned n=0;
 if(p->work){append(title,&n,p->saving?KU("Save As / "):KU("Open / "));append(title,&n,p->location.root);if(p->folder[0]){append(title,&n,"\\");append(title,&n,p->folder);}
  else append(title,&n,p->location.flags&K32_WORK_HDD?" / HDD":" / FD");
 }else{
  char drive[]="A:";drive[0]='A'+p->volume-1;
  append(title,&n,p->saving?"Save As / ":"Open / ");append(title,&n,drive);
  if(p->folder[0]){append(title,&n,"\\");append(title,&n,p->folder);}
  else append(title,&n," root / read only");
 }
 n=0;
 if(p->error==-13&&!p->work)append(status,&n,"Read only / choose Work to save");
 else if(p->error)append(status,&n,p->kind?"Use 8.3 BMP / workspace unavailable":"Use 8.3 TXT / workspace unavailable");
 else if(p->loading)append(status,&n,"Loading...");
 else if(p->work&&p->location.error)append(status,&n,"Workspace unavailable / Esc: cancel");
 else if(p->work&&p->location.version){
  append(status,&n,KU("Free "));number(status,&n,p->location.free_bytes/1024);append(status,&n," KiB");
  unsigned c=p->location.sector_bytes*p->location.cluster_sectors;
  if(p->saving&&c&&c<=65536&&p->document_bytes<=K32_WORK_LIMIT){append(status,&n,KU(" / temp "));number(status,&n,((p->document_bytes+c-1)/c*c+1023)/1024);append(status,&n," KiB");}
 }else append(status,&n,"Tab: controls / Esc: cancel");
 kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kw_text(8,4,title,(w-148)/8,0x20344a);kg_button(704,w-138,0,48,20,"Up");kg_button(703,w-84,0,76,20,"Drives");kw_draw_input((KwRect){8,20,(int)w-20,24},&p->filter,p->focus.current==0);
 if(p->list.count&&p->list.selected>=p->list.top&&p->list.selected<p->list.top+p->list.rows)kg_rect(8,44+(p->list.selected-p->list.top)*16,w-40,16,0xb4c2d0);
 for(unsigned i=0;i<p->list.rows&&p->list.top+i<p->list.count;i++)kg_text(12,44+i*16,p->entries[p->matches[p->list.top+i]].name,0x20344a);
 kw_draw_scroll(&p->scroll);if(p->saving)kw_draw_input((KwRect){8,(int)h-78,(int)w-20,24},&p->name,p->focus.current==2);kg_button(701,8,h-46,72,28,p->saving?"Save":"Open");kg_button(702,88,h-46,72,28,"Cancel");kw_text(8,h-16,status,(w-16)/8,0x20344a);return kg_present();}
