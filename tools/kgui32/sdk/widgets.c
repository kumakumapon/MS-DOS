#include "widgets.h"
static unsigned limit(unsigned top,unsigned total,unsigned rows){unsigned max=total>rows?total-rows:0;return top>max?max:top;}
int kw_inside(KwRect r,int x,int y){return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;}
int kw_focus(KwFocus *f,const KgEvent *e){if(e->type!=KG_KEY||e->key!=9||!f->count)return 0;f->current=(f->current+1)%f->count;return 1;}
int kw_input(KwInput *v,const KgEvent *e){
 if(e->type!=KG_KEY)return 0;
 unsigned k=e->key;
 if(k==258){if(v->cursor)v->cursor--;return 1;}
 if(k==259){if(v->cursor<v->length)v->cursor++;return 1;}
 if(k==260){v->cursor=0;return 1;}
 if(k==261){v->cursor=v->length;return 1;}
 if(k==8||k==264){
  if(k==8){if(!v->cursor)return 0;v->cursor--;}
  else if(v->cursor==v->length)return 0;
  for(unsigned i=v->cursor;i<v->length;i++)v->text[i]=v->text[i+1];
  v->length--;return 2;
 }
 if(k<32||k>126||v->length==KW_INPUT_BYTES-1)return 0;
 for(unsigned i=v->length+1;i>v->cursor;i--)v->text[i]=v->text[i-1];
 v->text[v->cursor++]=(char)k;v->length++;return 2;
}
void kw_reveal(KwList *l){
 if(!l->rows)l->rows=1;
 if(!l->count)l->selected=0;else if(l->selected>=l->count)l->selected=l->count-1;
 if(l->top>l->selected)l->top=l->selected;
 if(l->selected>=l->top+l->rows)l->top=l->selected-l->rows+1;
 l->top=limit(l->top,l->count,l->rows);
}
int kw_list(KwList *l,KwRect r,const KgEvent *e){
 if(e->type==KG_MOUSE&&(e->key&1)&&kw_inside(r,e->x,e->y)){
  unsigned i=l->top+(e->y-r.y)/16;if(i>=l->count)return 0;l->selected=i;return 1;
 }
 if(e->type!=KG_KEY)return 0;
 unsigned n=l->selected,k=e->key;
 if(k==256){if(n)n--;}else if(k==257){if(n+1<l->count)n++;}
 else if(k==260)n=0;else if(k==261)n=l->count?l->count-1:0;
 else if(k==262)n=n>l->rows?n-l->rows:0;
 else if(k==263)n=l->count&&n+l->rows<l->count?n+l->rows:l->count?l->count-1:0;
 else return 0;
 l->selected=n;kw_reveal(l);return 1;
}
/* Scroll position uses total-minus-visible, so the final page reaches EOF. */
int kw_scroll(KwScroll *s,const KgEvent *e){
 if(s->r.h<48||s->r.w<12||!s->rows){s->dragging=0;return 0;}
 if(e->type==KG_FOCUS&&!e->key){s->dragging=0;return 0;}
 if(e->type==KG_RESIZE){s->dragging=0;return 0;}
 if(e->type!=KG_MOUSE&&e->type!=KG_POINTER)return 0;
 if(!(e->key&1)){s->dragging=0;return 0;}
 int track=s->r.h-32,thumb=s->total>s->rows?(int)((unsigned)track*s->rows/s->total):track;
 if(thumb<12)thumb=12;
 if(thumb>track)thumb=track;
 unsigned max=s->total>s->rows?s->total-s->rows:0;
 int travel=track-thumb,pos=max?(int)(s->top*(unsigned)travel/max):0;
 if(s->dragging){
  int n=e->y-s->r.y-16-s->offset;if(n<0)n=0;if(n>travel)n=travel;
  s->top=travel?(unsigned)n*max/(unsigned)travel:0;return 1;
 }
 if(e->type!=KG_MOUSE||!kw_inside(s->r,e->x,e->y))return 0;
 int y=e->y-s->r.y;
 if(y<16){if(s->top)s->top--;}
 else if(y>=s->r.h-16)s->top++;
 else if(y>=16+pos&&y<16+pos+thumb){s->dragging=1;s->offset=y-16-pos;}
 else if(y<16+pos)s->top=s->top>s->rows?s->top-s->rows:0;
 else s->top+=s->rows;
 s->top=limit(s->top,s->total,s->rows);return 1;
}
int kw_button(unsigned id,const KgEvent *e){return e->type==KG_MOUSE&&(e->key&1)&&e->control==id;}
int kw_dialog(const KgEvent *e){return (e->type==KG_KEY&&(e->key==13||e->key==27))||kw_button(900,e);}
