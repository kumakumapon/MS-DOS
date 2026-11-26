#include "menu.h"
void km_layout(KmMenu *m,unsigned height){
 m->rows=height>88?(height-88)/(m->large?KM_LARGE_ROW_HEIGHT:KM_ROW_HEIGHT):1;
 if(!m->rows)m->rows=1;
 if(m->rows>8)m->rows=8;
}
int km_event(KmMenu *m,KgEvent *e,unsigned height){
 if(!m->items||!m->count||m->count>KM_MAX)return 0;
 km_layout(m,height);
 if(e->type==KG_QUIT){m->open=0;return 0;}
 if(e->type==KG_FOCUS&&!e->key){m->open=0;return 0;}
 if(e->type==KG_KEY&&(e->key==KG_MENU_KEY||e->key==280)){m->open=!m->open;m->selected=0;m->keyboard=e->key==280;return 1;}
 if(!m->open)return 0;
 unsigned id=e->type==KG_MOUSE&&(e->key&1)?e->control:0;
 if(e->type==KG_MOUSE&&(e->key&1))m->keyboard=0;
 if(id==KM_BACK||(e->type==KG_KEY&&e->key==27)){m->open=0;return 1;}
 if(!m->enabled)return 1;
 unsigned page=m->selected/m->rows,first=page*m->rows,chosen=KM_MAX;
 if(id==KM_PREV){m->selected=page?first-m->rows:0;return 1;}
 if(id==KM_NEXT){if(first+m->rows<m->count)m->selected=first+m->rows;return 1;}
 if(id>=KM_ITEM&&id<KM_ITEM+m->count&&id-KM_ITEM>=first&&id-KM_ITEM<first+m->rows)chosen=id-KM_ITEM;
 if(e->type==KG_KEY){
  unsigned k=e->key;
  if(k==13||k==9||k==256||k==257||k==262||k==263)m->keyboard=1;
  if(k==256||k==257||k==9){
   unsigned step=k==256||(k==9&&(e->control&KG_MOD_SHIFT))?m->count-1:1;
   for(unsigned i=0;i<m->count;i++){
    m->selected=(m->selected+step)%m->count;
    if(!(m->disabled&(1u<<m->selected)))break;
   }
  }
  if(k==262)m->selected=m->selected>m->rows?m->selected-m->rows:0;
  if(k==263)m->selected=m->selected+m->rows<m->count?m->selected+m->rows:m->count-1;
  if(k==13)chosen=m->selected;
 }
 if(chosen<m->count&&!(m->disabled&(1u<<chosen))){const KmItem *i=&m->items[chosen];unsigned seconds=e->seconds;
  *e=(KgEvent){.type=i->type,.key=i->key,.control=i->control,.x=-1,.y=-1,.seconds=seconds};m->open=0;return 2;
 }
 return 1;
}
