#include "layout.h"
static int same_program(const char *a,const char *b){
 for(unsigned i=0;i<16;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}return 0;
}
static int equal_rect(Rect a,Rect b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
static int valid_rect(Rect r){return r.x>=0&&r.x<800&&r.y>=0&&r.y<600&&r.w>0&&r.w<=800&&r.h>0&&r.h<=600;}
int layout_validate(WindowLayout *layout){
 if(layout->magic!=LAYOUT_MAGIC||layout->version!=1||layout->next>=LAYOUT_APPS){
  *layout=(WindowLayout){.magic=LAYOUT_MAGIC,.version=1};return 0;
 }
 layout->panes&=3;
 for(unsigned i=0;i<2;i++)if(!valid_rect(layout->pane[i]))layout->panes&=~(1u<<i);
 for(unsigned i=0;i<LAYOUT_APPS;i++){
  AppLayout *a=&layout->apps[i];
  if(a->program[15]||!valid_rect(a->rect)||a->rect.w<176||a->rect.h<184||a->mode>3)*a=(AppLayout){0};
 }
 return 1;
}
int layout_restore(const WindowLayout *layout,Scene *s,int id,const char *program){
 if(id<2){
  if(!(layout->panes&(1u<<id)))return 0;
  Rect r=s->window[id].r;r.x=layout->pane[id].x;r.y=layout->pane[id].y;
  scene_restore_window(s,id,r,0);return 1;
 }
 if(!program||!*program)return 0;
 for(unsigned i=0;i<LAYOUT_APPS;i++){
  const AppLayout *a=&layout->apps[i];
  if(a->program[0]&&same_program(a->program,program)){
   scene_restore_window(s,id,a->rect,a->mode);return 1;
  }
 }
 return 0;
}
int layout_remember(WindowLayout *layout,const Scene *s,int id,const char *program){
 Rect r=s->window[id].r;
 if(id<2){
  unsigned bit=1u<<id;
  if((layout->panes&bit)&&equal_rect(layout->pane[id],r))return 0;
  layout->panes|=bit;layout->pane[id]=r;return 1;
 }
 if(!program||!*program)return 0;
 unsigned mode=s->placement[id-2];
 if(mode)r=s->restored[id-2];
 for(unsigned i=0;i<LAYOUT_APPS;i++){
  AppLayout *a=&layout->apps[i];
  if(a->program[0]&&same_program(a->program,program)){
   if(a->mode==mode&&equal_rect(a->rect,r))return 0;
   a->rect=r;a->mode=mode;return 1;
  }
 }
 AppLayout *a=&layout->apps[layout->next];
 *a=(AppLayout){.rect=r,.mode=mode};
 for(unsigned i=0;i<sizeof a->program-1&&program[i];i++)a->program[i]=program[i];
 layout->next=(layout->next+1)%LAYOUT_APPS;return 1;
}
