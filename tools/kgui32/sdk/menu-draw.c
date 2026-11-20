#include "kgui.h"
#include "menu.h"
#include "widgets.h"
#include "text.h"
#include "ui-menu.h"
void km_draw(const KmMenu *m){
 unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,DESKTOP_COLOR_SURFACE);
 kg_label(8,10,m->enabled?"App menu":"Finish current dialog",DESKTOP_COLOR_INK);
 kg_button(KM_BACK,w-88,2,80,32,"Back");
 if(!m->enabled)return;
 unsigned first=m->selected/m->rows*m->rows;
 for(unsigned i=first;i<m->count&&i<first+m->rows;i++){
  unsigned y=KM_ROW_HEIGHT+(i-first)*KM_ROW_HEIGHT;
  char label[48];const char *s=ku_lookup(m->labels,m->label_count,m->items[i].label);
  unsigned offset=0,n=0,cols=(w-24)/8;if(cols>47)cols=47;
  if(m->checked&(1u<<i)){label[0]='[';label[1]='x';label[2]=']';label[3]=' ';offset=4;cols-=4;}
  /* Menu labels use complete ASCII/Shift-JIS units. */
  while(s[n]&&n<cols){
   unsigned bytes=kt_unit(s+n,s[n+1]?2:1);
   if(!bytes)break;
   if(bytes>cols-n)break;
   for(unsigned j=0;j<bytes;j++)label[offset+n+j]=s[n+j];
   n+=bytes;
  }
  label[offset+n]=0;
  unsigned state=m->disabled&(1u<<i)?KW_DISABLED:0;
  if(m->keyboard&&i==m->selected)state|=KW_FOCUSED;
  kw_draw_button((KwRect){8,(int)y,(int)w-16,DESKTOP_CONTROL_HEIGHT},KM_ITEM+i,label,
   i==m->selected?KW_PRIMARY:KW_SECONDARY,state);
 }
 if(first)kg_button(KM_PREV,8,h-36,88,32,"Previous");
 if(first+m->rows<m->count)kg_button(KM_NEXT,w-104,h-36,96,32,"Next");
}
