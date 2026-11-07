#include "picker.h"
static unsigned upper(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int match(const char *s,const char *f){if(!*f)return 1;for(;*s;s++){unsigned i=0;while(f[i]&&s[i]&&upper(f[i])==upper(s[i]))i++;if(!f[i])return 1;}return 0;}
static void rebuild(KpPicker *p){p->list.count=0;for(unsigned i=0;i<p->count;i++)if(match(p->entries[i].name,p->filter.text))p->matches[p->list.count++]=i;kw_reveal(&p->list);}
void kp_close(KpPicker *p){if(p->cursor)k32_dir_close(p->cursor);p->cursor=p->loading=0;}
void kp_open(KpPicker *p){*p=(KpPicker){.loading=1,.focus={.count=4},.list={.rows=1}};}
int kp_tick(KpPicker *p){if(!p->loading)return 0;for(unsigned i=0;i<4;i++){K32DirEntry e;int r=p->cursor?k32_dir_next(p->cursor,&e):k32_dir_first(&e);if(r<0){p->error=r;kp_close(p);break;}p->cursor=e.cursor;if(!r){kp_close(p);break;}if(p->count==KP_ENTRIES){p->error=-28;kp_close(p);break;}p->entries[p->count++]=e;}rebuild(p);return 1;}
static void layout(KpPicker *p){p->list.rows=(kg_height()-96)/16;kw_reveal(&p->list);p->scroll.r=(KwRect){(int)kg_width()-28,44,20,(int)p->list.rows*16};p->scroll.total=p->list.count;p->scroll.rows=p->list.rows;p->scroll.top=p->list.top;}
int kp_event(KpPicker *p,const KgEvent *e,char name[13]){
 layout(p);if(e->type==KG_KEY&&e->key==27){kp_close(p);return -1;}
 if(kw_focus(&p->focus,e))return 0;
 if(e->type==KG_MOUSE&&(e->key&1)){if(e->y<40)p->focus.current=0;else if(e->y<(int)kg_height()-48)p->focus.current=1;}
 if(p->focus.current==0&&kw_input(&p->filter,e)==2){p->list.selected=p->list.top=0;rebuild(p);}
 if(p->focus.current==1)kw_list(&p->list,(KwRect){8,44,(int)kg_width()-40,(int)p->list.rows*16},e);
 if(kw_scroll(&p->scroll,e))p->list.top=p->scroll.top;
 if(kw_button(702,e)||(e->type==KG_KEY&&e->key==13&&p->focus.current==3)){kp_close(p);return -1;}
 if(!p->loading&&(kw_button(701,e)||(e->type==KG_KEY&&e->key==13&&(p->focus.current==1||p->focus.current==2)))&&p->list.count){K32DirEntry *d=&p->entries[p->matches[p->list.selected]];if(d->attributes&16){p->error=-21;return 0;}for(unsigned i=0;i<13;i++)name[i]=d->name[i];kp_close(p);return 1;}return 0;
}
int kp_draw(KpPicker *p){layout(p);unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);kg_text(8,4,"Open / A: root / read only",0x20344a);kw_draw_input((KwRect){8,20,(int)w-20,24},&p->filter,p->focus.current==0);
 if(p->list.count&&p->list.selected>=p->list.top&&p->list.selected<p->list.top+p->list.rows)kg_rect(8,44+(p->list.selected-p->list.top)*16,w-40,16,0xb4c2d0);
 for(unsigned i=0;i<p->list.rows&&p->list.top+i<p->list.count;i++)kg_text(12,44+i*16,p->entries[p->matches[p->list.top+i]].name,0x20344a);
 kw_draw_scroll(&p->scroll);kg_button(701,8,h-46,88,28,"Open");kg_button(702,104,h-46,88,28,"Cancel");kg_text(8,h-16,p->error?"Unavailable file / root limit":p->loading?"Loading...":"Tab: controls / Esc: cancel",0x20344a);return kg_present();}
