#include "kgui.h"
#include "widgets.h"
#include "text.h"
#include "ui-widgets.h"

static unsigned fit_text(char out[48], const char *s, unsigned cols) {
 unsigned i=0;
 if(cols>47)cols=47;
 while(i<cols&&s[i]){
  unsigned k=kt_unit(s+i,s[i+1]?2:1);
  if(!k){out[i++]='?';continue;}
  if(k>cols-i)break;
  for(unsigned j=0;j<k;j++)out[i+j]=s[i+j];
  i+=k;
 }
 out[i]=0;
 return i;
}
void kw_text(int x,int y,const char *s,unsigned cols,u32 color){
 char out[48];fit_text(out,s,cols);kg_text(x,y,out,color);
}

void kw_readable(int x,int y,const char *s,unsigned pixels,u32 color){
 char out[48];unsigned large=kg_large_text();
 fit_text(out,s,pixels/(large?16:8));
 if(large)kg_text_double(x,y,out,color);
 else kg_text(x,y,out,color);
}

void kw_draw_focus(KwRect r){
 const int edge=DESKTOP_FOCUS_WIDTH;
 if(r.w<=2*edge||r.h<=2*edge)return;
 kg_rect(r.x,r.y,r.w,edge,DESKTOP_COLOR_FOCUS);
 kg_rect(r.x,r.y+r.h-edge,r.w,edge,DESKTOP_COLOR_FOCUS);
 kg_rect(r.x,r.y+edge,edge,r.h-2*edge,DESKTOP_COLOR_FOCUS);
 kg_rect(r.x+r.w-edge,r.y+edge,edge,r.h-2*edge,DESKTOP_COLOR_FOCUS);
}
int kw_draw_button(KwRect r,unsigned id,const char *text,unsigned role,unsigned state){
 if(r.w<12||r.h<18)return -22;
 char label[48];int n=fit_text(label,text,(r.w-4)/8);
 unsigned fill=desktop_button_fill(role,state);
 int error;
 if(state&KW_DISABLED){
  error=kg_rect(r.x,r.y,r.w,r.h,fill);
  if(!error)error=kg_text(r.x+(r.w-8*n)/2,r.y+(r.h-16)/2,label,DESKTOP_COLOR_MUTED);
 }else{
  error=kg_button_color(id,r.x,r.y,r.w,r.h,label,fill);
 }
 if(!error&&(state&KW_FOCUSED))kw_draw_focus(r);
 return error;
}

void kw_draw_input(KwRect r,const KwInput *v,int focus){
 if(r.w<20||r.h<22)return;
 int edge=focus?DESKTOP_FOCUS_WIDTH:1;
 kg_rect(r.x,r.y,r.w,r.h,focus?DESKTOP_COLOR_FOCUS:DESKTOP_COLOR_BORDER);
 kg_rect(r.x+edge,r.y+edge,r.w-2*edge,r.h-2*edge,DESKTOP_COLOR_PAPER);
 unsigned cell=kg_large_text()&&r.h>=36?16:8;
 unsigned cols=(r.w-12)/cell;if(cols>47)cols=47;
 unsigned start=v->cursor>=cols?v->cursor-cols+1:0;
 unsigned floor=kt_floor(v->text,v->length,start);if(floor<start)start=kt_next(v->text,v->length,floor);
 if(cell==16)kw_readable(r.x+DESKTOP_SPACE_SMALL,r.y+2,v->text+start,cols*cell,DESKTOP_COLOR_INK);
 else kw_text(r.x+DESKTOP_SPACE_SMALL,r.y+DESKTOP_SPACE_SMALL,v->text+start,cols,DESKTOP_COLOR_INK);
 if(focus)kg_rect(r.x+DESKTOP_SPACE_SMALL+(v->cursor-start)*cell,r.y+(cell==16?32:20),cell,DESKTOP_FOCUS_WIDTH,DESKTOP_COLOR_FOCUS);
}
void kw_draw_scroll(const KwScroll *s){
 if(s->r.h<48||s->r.w<12||!s->rows)return;
 int track=s->r.h-32,thumb=s->total>s->rows?(int)((unsigned)track*s->rows/s->total):track;
 if(thumb<12)thumb=12;
 if(thumb>track)thumb=track;
 unsigned max=s->total>s->rows?s->total-s->rows:0;int pos=max?(int)(s->top*(unsigned)(track-thumb)/max):0;
 kg_rect(s->r.x,s->r.y,s->r.w,s->r.h,DESKTOP_COLOR_SECONDARY_BUTTON);kg_text(s->r.x+4,s->r.y,"^",DESKTOP_COLOR_INK);
 kg_text(s->r.x+4,s->r.y+s->r.h-16,"v",DESKTOP_COLOR_INK);
 kg_rect(s->r.x+2,s->r.y+16+pos,s->r.w-4,thumb,s->dragging?DESKTOP_COLOR_FOCUS:DESKTOP_COLOR_BORDER);
}
/* A zero total denotes a scan whose final length is still unknown. */
void kw_draw_progress(KwRect r,const char *label,unsigned done,unsigned total,unsigned cancel){
 int width=r.w-116,left=r.x+116;
 if(width<32||r.h<36)return;
 kw_text(left,r.y,label,width/8,DESKTOP_COLOR_INK);
 kg_rect(left,r.y+23,width,9,DESKTOP_COLOR_BORDER);
 kg_rect(left+1,r.y+24,width-2,7,DESKTOP_COLOR_PAPER);
 if(total){
  /* Divide before scaling for large byte counts without overflowing u32. */
  while(total>0xffff){done=(done>>1)+(done&1);total=(total>>1)+(total&1);}
  int fill=(int)((unsigned)(width-2)*(done>total?total:done)/total);
  if(fill)kg_rect(left+1,r.y+24,fill,7,DESKTOP_COLOR_FOCUS);
 }else {
  int mark=width/5;
  kg_rect(left+1+(done*4u)%(width-2-mark),r.y+24,mark,7,DESKTOP_COLOR_FOCUS);
 }
 kw_draw_button((KwRect){r.x,r.y,108,36},cancel,KU("Cancel"),KW_SECONDARY,KW_FOCUSED);
}
void kw_draw_dialog(unsigned w,unsigned h,const char *a,const char *b){
 if(h<80||w<80)return;
 int x=12,panel=h<112?(int)h:112,y=((int)h-panel)/2;
 int compact=panel<108;
 kg_rect(x,y,w-24,panel,DESKTOP_COLOR_INK);
 kg_rect(x+2,y+2,w-28,panel-4,DESKTOP_COLOR_SURFACE);
 kw_text(x+12,y+(compact?4:12),a,(w-48)/8,DESKTOP_COLOR_INK);
 kw_text(x+12,y+(compact?22:36),b,(w-48)/8,DESKTOP_COLOR_INK);
 int button_y=y+panel-DESKTOP_CONTROL_HEIGHT-(compact?4:10);
 kw_draw_button((KwRect){(int)(w-80)/2,button_y,80,DESKTOP_CONTROL_HEIGHT},900,KU("OK"),KW_PRIMARY,0);
}
