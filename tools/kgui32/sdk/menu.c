#include "menu.h"
int km_event(KmMenu *m,KgEvent *e,unsigned height){
 if(!m->items||!m->count||m->count>KM_MAX)return 0;
 m->rows=height>88?(height-88)/40:1;if(!m->rows)m->rows=1;if(m->rows>8)m->rows=8;
 if(e->type==KG_QUIT){m->open=0;return 0;}
 if(e->type==KG_FOCUS&&!e->key){m->open=0;return 0;}
 if(e->type==KG_KEY&&(e->key==KG_MENU_KEY||e->key==280)){m->open=!m->open;m->selected=0;return 1;}
 if(!m->open)return 0;
 unsigned id=e->type==KG_MOUSE&&(e->key&1)?e->control:0;
 if(id==KM_BACK||(e->type==KG_KEY&&e->key==27)){m->open=0;return 1;}
 if(!m->enabled)return 1;
 unsigned page=m->selected/m->rows,first=page*m->rows,chosen=KM_MAX;
 if(id==KM_PREV){m->selected=page?first-m->rows:0;return 1;}
 if(id==KM_NEXT){if(first+m->rows<m->count)m->selected=first+m->rows;return 1;}
 if(id>=KM_ITEM&&id<KM_ITEM+m->count&&id-KM_ITEM>=first&&id-KM_ITEM<first+m->rows)chosen=id-KM_ITEM;
 if(e->type==KG_KEY){
  unsigned k=e->key;
  if(k==256)m->selected=m->selected?m->selected-1:m->count-1;
  if(k==257||k==9)m->selected=(m->selected+1)%m->count;
  if(k==262)m->selected=m->selected>m->rows?m->selected-m->rows:0;
  if(k==263)m->selected=m->selected+m->rows<m->count?m->selected+m->rows:m->count-1;
  if(k==13)chosen=m->selected;
 }
 if(chosen<m->count&&!(m->disabled&(1u<<chosen))){const KmItem *i=&m->items[chosen];unsigned seconds=e->seconds;
  *e=(KgEvent){.type=i->type,.key=i->key,.control=i->control,.x=-1,.y=-1,.seconds=seconds};m->open=0;return 2;
 }
 return 1;
}
