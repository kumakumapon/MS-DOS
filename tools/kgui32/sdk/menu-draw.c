#include "kgui.h"
#include "menu.h"
#include "widgets.h"
#include "ui-menu.h"
void km_draw(const KmMenu *m){
 unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 kg_label(8,10,m->enabled?"App menu":"Finish current dialog",0x20344a);
 kg_button(KM_BACK,w-88,2,80,32,"Back");
 if(!m->enabled)return;
 unsigned first=m->selected/m->rows*m->rows;
 for(unsigned i=first;i<m->count&&i<first+m->rows;i++){
  unsigned y=40+(i-first)*40;
  if(i==m->selected)kg_rect(6,y-2,w-12,40,0x327ac0);
  char label[48];const char *s=ku_lookup(m->labels,m->label_count,m->items[i].label);
  unsigned offset=0,n=0,cols=(w-24)/8;if(cols>47)cols=47;
  if(m->checked&(1u<<i)){label[0]='[';label[1]='x';label[2]=']';label[3]=' ';offset=4;cols-=4;}
  /* Menu labels use complete ASCII/Shift-JIS units. */
  while(s[n]&&n<cols){
   unsigned c=(unsigned char)s[n],bytes=(c>=0x81&&c<=0x9f)||(c>=0xe0&&c<=0xef)?2:1;
   if(bytes>cols-n)break;
   for(unsigned j=0;j<bytes;j++)label[offset+n+j]=s[n+j];
   n+=bytes;
  }
  label[offset+n]=0;
  if(m->disabled&(1u<<i)){
   kg_rect(8,y,w-16,36,0xd7dfe7);
   kg_text(12,y+10,label,0x526377);
  }else kg_button(KM_ITEM+i,8,y,w-16,36,label);
 }
 if(first)kg_button(KM_PREV,8,h-36,88,32,"Previous");
 if(first+m->rows<m->count)kg_button(KM_NEXT,w-104,h-36,96,32,"Next");
}
