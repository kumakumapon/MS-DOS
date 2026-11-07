#include "picker.h"
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
void kp_close(KpPicker *p){if(p->cursor)k32_dir_close(p->cursor);p->cursor=p->loading=0;}
void kp_open(KpPicker *p){*p=(KpPicker){.loading=1,.focus={.count=4},.list={.rows=1}};kp_workspace_info(&p->location);p->work=!!(p->location.flags&K32_WORK_HDD);}
void kp_save_size(KpPicker *p,const char *name,unsigned bytes){kp_open(p);p->document_bytes=bytes;p->work=p->saving=1;p->focus.count=5;p->focus.current=2;for(unsigned i=0;i<12&&name[i];i++){p->name.text[i]=name[i];p->name.length++;}p->name.cursor=p->name.length;}
void kp_save(KpPicker *p,const char *name){kp_save_size(p,name,0);}
int kp_tick(KpPicker *p){if(!p->loading)return 0;for(unsigned i=0;i<4;i++){K32DirEntry e;int r=p->cursor?k32_dir_next(p->cursor,&e):(p->work?k32_work_first(&e):k32_dir_first(&e));if(r<0){p->error=r;kp_close(p);break;}p->cursor=e.cursor;if(!r){kp_close(p);break;}if(p->count==KP_ENTRIES){p->error=-28;kp_close(p);break;}p->entries[p->count++]=e;}rebuild(p);return 1;}
static void layout(KpPicker *p){p->list.rows=(kg_height()-(p->saving?128:96))/16;kw_reveal(&p->list);p->scroll.r=(KwRect){(int)kg_width()-28,44,20,(int)p->list.rows*16};p->scroll.total=p->list.count;p->scroll.rows=p->list.rows;p->scroll.top=p->list.top;}
int kp_event(KpPicker *p,const KgEvent *e,char name[13]){
 layout(p);if(e->type==KG_KEY&&e->key==27){kp_close(p);return -1;}
 if(!p->saving&&(kw_button(703,e)||(e->type==KG_KEY&&e->key==23))&&(k32_query()&K32_FEATURE_SAVE)){unsigned work=!p->work;kp_close(p);kp_open(p);p->work=work;return 0;}
 if(kw_focus(&p->focus,e))return 0;
 if(e->type==KG_MOUSE&&(e->key&1)){if(p->saving&&e->y>=(int)kg_height()-80&&e->y<(int)kg_height()-48)p->focus.current=2;else if(e->y<40)p->focus.current=0;else if(e->y<(int)kg_height()-(p->saving?80:48))p->focus.current=1;}
 if(p->focus.current==0&&kw_input(&p->filter,e)==2){p->list.selected=p->list.top=0;rebuild(p);}
 if(p->saving&&p->focus.current==2)kw_input(&p->name,e);
 if(p->focus.current==1)kw_list(&p->list,(KwRect){8,44,(int)kg_width()-40,(int)p->list.rows*16},e);
 if(p->saving&&p->focus.current==1&&p->list.count&&(e->type==KG_MOUSE||(e->type==KG_KEY&&(e->key==13||(e->key>=256&&e->key<=263))))){K32DirEntry *d=&p->entries[p->matches[p->list.selected]];p->name=(KwInput){0};for(unsigned i=0;i<12&&d->name[i];i++)p->name.text[p->name.length++]=d->name[i];p->name.cursor=p->name.length;}
 if(kw_scroll(&p->scroll,e))p->list.top=p->scroll.top;
 if(kw_button(702,e)||(e->type==KG_KEY&&e->key==13&&p->focus.current==(p->saving?4u:3u))){kp_close(p);return -1;}
 if(p->saving&&(kw_button(701,e)||(e->type==KG_KEY&&e->key==13&&(p->focus.current==1||p->focus.current==2||p->focus.current==3)))){char path[24];if(k32_work_path(p->name.text,path)){p->error=-22;return 0;}for(unsigned i=0;i<13;i++){name[i]=path[8+i];if(!name[i])break;}kp_close(p);return 1;}
 if(!p->saving&&!p->loading&&(kw_button(701,e)||(e->type==KG_KEY&&e->key==13&&(p->focus.current==1||p->focus.current==2)))&&p->list.count){K32DirEntry *d=&p->entries[p->matches[p->list.selected]];if(d->attributes&16){p->error=-21;return 0;}for(unsigned i=0;i<13;i++)name[i]=d->name[i];kp_close(p);return 1;}return 0;
}
static void append(char *s,unsigned *n,const char *text){while(*text&&*n<79)s[(*n)++]=*text++;s[*n]=0;}
static void number(char *s,unsigned *n,unsigned value){char b[11];unsigned i=10;b[i]=0;do{b[--i]='0'+value%10;value/=10;}while(value);append(s,n,b+i);}
int kp_draw(KpPicker *p){layout(p);unsigned w=kg_width(),h=kg_height();char title[80]={0},status[80]={0};unsigned n=0;
 if(p->saving||p->work){append(title,&n,p->saving?"Save As / ":"Open / ");append(title,&n,p->location.root);append(title,&n,p->location.flags&K32_WORK_HDD?" / HDD":" / FD");}else append(title,&n,"Open / A: root / read only");
 n=0;
 if(p->error)append(status,&n,"Use 8.3 TXT / workspace unavailable");
 else if(p->loading)append(status,&n,"Loading...");
 else if((p->work||p->saving)&&p->location.error)append(status,&n,"Workspace unavailable / Esc: cancel");
 else if((p->work||p->saving)&&p->location.version){
  append(status,&n,"Free ");number(status,&n,p->location.free_bytes/1024);append(status,&n," KiB");
  unsigned c=p->location.sector_bytes*p->location.cluster_sectors;
  if(p->saving&&c&&c<=65536&&p->document_bytes<=K32_WORK_LIMIT){append(status,&n," / temp ");number(status,&n,((p->document_bytes+c-1)/c*c+1023)/1024);append(status,&n," KiB");}
 }else append(status,&n,"Tab: controls / Esc: cancel");
 kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kw_text(8,4,title,(w-16)/8,0x20344a);kw_draw_input((KwRect){8,20,(int)w-20,24},&p->filter,p->focus.current==0);
 if(p->list.count&&p->list.selected>=p->list.top&&p->list.selected<p->list.top+p->list.rows)kg_rect(8,44+(p->list.selected-p->list.top)*16,w-40,16,0xb4c2d0);
 for(unsigned i=0;i<p->list.rows&&p->list.top+i<p->list.count;i++)kg_text(12,44+i*16,p->entries[p->matches[p->list.top+i]].name,0x20344a);
 kw_draw_scroll(&p->scroll);if(p->saving)kw_draw_input((KwRect){8,(int)h-78,(int)w-20,24},&p->name,p->focus.current==2);else if(k32_query()&K32_FEATURE_SAVE)kg_button(703,w-64,0,56,20,p->work?"Root":"Work");kg_button(701,8,h-46,72,28,p->saving?"Save":"Open");kg_button(702,88,h-46,72,28,"Cancel");kw_text(8,h-16,status,(w-16)/8,0x20344a);return kg_present();}
