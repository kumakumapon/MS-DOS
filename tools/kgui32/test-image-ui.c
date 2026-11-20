/* Execute the real Images renderer against the real client frame validator.
 * Host drawing adapters collect commands; no DOS service is executed. */
#include <assert.h>
#include <string.h>
#include "sdk/kgui.h"
#include "sdk/widgets.h"
#include "sdk/menu.h"
#include "client-policy.h"
unsigned ku_language;
static KgRequest frame;
static unsigned canvas_w=344,canvas_h=196;
unsigned kg_width(void){return canvas_w;}
unsigned kg_height(void){return canvas_h;}
void kg_menu_enable(unsigned enabled){(void)enabled;}
void kg_begin(void){frame=(KgRequest){.version=1,.size=sizeof frame,.operation=KG_FRAME};}
static KgDraw *command(unsigned op,int x,int y,int w,int h,unsigned color){
 assert(frame.count<KG_COMMANDS);KgDraw *d=&frame.draw[frame.count++];
 *d=(KgDraw){.op=op,.x=x,.y=y,.w=w,.h=h,.color=color};return d;
}
int kg_rect(int x,int y,int w,int h,u32 c){command(KG_RECT,x,y,w,h,c);return 0;}
int kg_text(int x,int y,const char *s,u32 c){assert(strlen(s)<48);strcpy(command(KG_TEXT,x,y,0,0,c)->text,s);return 0;}
int kg_button_color(u32 id,int x,int y,int w,int h,const char *s,u32 color){
 assert(strlen(s)<48);KgDraw *d=command(KG_BUTTON,x,y,w,h,color);d->id=id;strcpy(d->text,s);return 0;
}
int kg_button(u32 id,int x,int y,int w,int h,const char *s){return kg_button_color(id,x,y,w,h,s,DESKTOP_COLOR_SECONDARY_BUTTON);}
int kg_bitmap(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned w,unsigned h){
 (void)handle;(void)x;(void)y;(void)sx;(void)sy;(void)w;(void)h;assert(0);return -1;
}
int kg_bitmap_scaled(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned sw,unsigned sh,unsigned dw,unsigned dh){(void)handle;(void)x;(void)y;(void)sx;(void)sy;(void)sw;(void)sh;(void)dw;(void)dh;assert(0);return -1;}
int kg_bitmap_scaled_clip(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned sw,unsigned sh,unsigned dw,unsigned dh,unsigned cx,unsigned cy,unsigned cw,unsigned ch){(void)cx;(void)cy;(void)cw;(void)ch;return kg_bitmap_scaled(handle,x,y,sx,sy,sw,sh,dw,dh);}
int kg_present(void){return kg_validate(&frame,canvas_w,canvas_h);}
#define main image_guest_main
#include "apps/image.c"
#undef main
static void styled_controls(void){
 KwRect rect={8,8,28,DESKTOP_CONTROL_HEIGHT};
 kg_begin();
 assert(!kw_draw_button(rect,1200,"A\x93\xfa",KW_PRIMARY,KW_FOCUSED));
 assert(frame.count==5&&frame.draw[0].color==DESKTOP_COLOR_PRIMARY_BUTTON&&frame.draw[0].id==1200);
 assert(!strcmp(frame.draw[0].text,"A\x93\xfa"));
 for(unsigned i=1;i<5;i++){
  const KgDraw *edge=&frame.draw[i];
  assert(edge->op==KG_RECT&&edge->color==DESKTOP_COLOR_FOCUS);
  assert(edge->w==DESKTOP_FOCUS_WIDTH||edge->h==DESKTOP_FOCUS_WIDTH);
 }
 assert(!kg_present());
 rect.w=20;kg_begin();
 assert(!kw_draw_button(rect,1200,"A\x93\xfa",KW_DANGER,KW_DISABLED|KW_PRESSED));
 assert(frame.count==2&&frame.draw[0].op==KG_RECT&&frame.draw[0].color==DESKTOP_COLOR_DISABLED);
 assert(frame.draw[1].op==KG_TEXT&&!strcmp(frame.draw[1].text,"A"));
 assert(!kg_present());
 kg_begin();assert(!kw_draw_button(rect,1200,"OK",KW_DANGER,KW_PRESSED));
 assert(frame.count==1&&frame.draw[0].color==DESKTOP_COLOR_DANGER_BUTTON-DESKTOP_COLOR_PRESSED_SHADE);
 assert(!kg_present());
}
int main(void){
 styled_controls();
 for(unsigned lang=0;lang<2;lang++)for(unsigned large=0;large<2;large++){
  ku_language=lang;canvas_w=large?624:344;canvas_h=large?376:196;
  filename=(KwInput){.text="SAMPLE.BMP",.length=10,.cursor=10};
  rescue=(KwInput){.text="RESCUED.BMP",.length=11,.cursor=11};
  for(unsigned mode=0;mode<=4;mode++){
   modal=mode;message=0;assert(!draw());
   message="Inspect TMP/BAK / original kept";assert(!draw());
  }
 }
 KmItem items[KM_MAX];for(unsigned i=0;i<KM_MAX;i++)items[i]=(KmItem){"Save As",KG_KEY,19,0};
 for(unsigned lang=0;lang<2;lang++)for(unsigned size=0;size<2;size++){
  ku_language=lang;canvas_w=size?784:344;canvas_h=size?416:168;
  const KuPair labels[]={{"Save As","\x95\xca\x96\xbc\x95\xdb\x91\xb6"}};
  KmMenu m={.items=items,.count=KM_MAX,.enabled=1,.open=1,.rows=(canvas_h-88)/KM_ROW_HEIGHT,.labels=labels,.label_count=1,.keyboard=1};
  for(unsigned state=0;state<2;state++){
   m.disabled=state?0xffffff:0;
   for(unsigned selected=0;selected<KM_MAX;selected++){m.selected=selected;km_draw(&m);assert(frame.count<=KG_COMMANDS);assert(!kg_validate(&frame,canvas_w,canvas_h));}
  }
  m.enabled=0;km_draw(&m);assert(!kg_validate(&frame,canvas_w,canvas_h));
 }
 return 0;
}
