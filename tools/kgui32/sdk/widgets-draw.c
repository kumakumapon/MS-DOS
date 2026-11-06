#include "kgui.h"
#include "widgets.h"
void kw_text(int x,int y,const char *s,unsigned cols,u32 color){char out[48];unsigned i=0;if(cols>47)cols=47;for(;i<cols&&s[i];i++)out[i]=s[i]>=32&&s[i]<=126?s[i]:'?';out[i]=0;kg_text(x,y,out,color);}
void kw_draw_input(KwRect r,const KwInput *v,int focus){
 kg_rect(r.x,r.y,r.w,r.h,focus?0x327ac0:0xb4c2d0);kg_rect(r.x+1,r.y+1,r.w-2,r.h-2,0xffffff);
 unsigned cols=(r.w-12)/8,start=v->cursor>=cols?v->cursor-cols+1:0;
 kw_text(r.x+4,r.y+4,v->text+start,cols,0x20344a);
 if(focus)kg_rect(r.x+4+(v->cursor-start)*8,r.y+20,8,2,0x327ac0);
}
void kw_draw_scroll(const KwScroll *s){
 if(s->r.h<48||s->r.w<12||!s->rows)return;
 int track=s->r.h-32,thumb=s->total>s->rows?(int)((unsigned)track*s->rows/s->total):track;
 if(thumb<12)thumb=12;
 if(thumb>track)thumb=track;
 unsigned max=s->total>s->rows?s->total-s->rows:0;int pos=max?(int)(s->top*(unsigned)(track-thumb)/max):0;
 kg_rect(s->r.x,s->r.y,s->r.w,s->r.h,0xdce7ef);kg_text(s->r.x+4,s->r.y,"^",0x20344a);
 kg_text(s->r.x+4,s->r.y+s->r.h-16,"v",0x20344a);
 kg_rect(s->r.x+2,s->r.y+16+pos,s->r.w-4,thumb,s->dragging?0x327ac0:0x829db5);
}
void kw_draw_dialog(unsigned w,unsigned h,const char *a,const char *b){
 int x=12,y=(h-112)/2;kg_rect(x,y,w-24,112,0x20344a);kg_rect(x+2,y+2,w-28,108,0xf4f7fa);
 kw_text(x+12,y+12,a,(w-48)/8,0x20344a);kw_text(x+12,y+36,b,(w-48)/8,0x20344a);
 kg_button(900,(w-80)/2,y+70,80,28,"OK");
}
