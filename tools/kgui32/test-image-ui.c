/* Execute the real Images renderer against the real client frame validator.
 * Host drawing adapters collect commands; no DOS service is executed. */
#include <assert.h>
#include <string.h>
#include "sdk/kgui.h"
#include "sdk/widgets.h"
#include "client-policy.h"
unsigned ku_language;
static KgRequest frame;
static unsigned canvas_w=344,canvas_h=196;
unsigned kg_width(void){return canvas_w;}
unsigned kg_height(void){return canvas_h;}
void kg_begin(void){frame=(KgRequest){.version=1,.size=sizeof frame,.operation=KG_FRAME};}
static KgDraw *command(unsigned op,int x,int y,int w,int h,unsigned color){
 assert(frame.count<KG_COMMANDS);KgDraw *d=&frame.draw[frame.count++];
 *d=(KgDraw){.op=op,.x=x,.y=y,.w=w,.h=h,.color=color};return d;
}
int kg_rect(int x,int y,int w,int h,u32 c){command(KG_RECT,x,y,w,h,c);return 0;}
int kg_text(int x,int y,const char *s,u32 c){assert(strlen(s)<48);strcpy(command(KG_TEXT,x,y,0,0,c)->text,s);return 0;}
int kg_button(u32 id,int x,int y,int w,int h,const char *s){
 assert(strlen(s)<48);KgDraw *d=command(KG_BUTTON,x,y,w,h,0);d->id=id;strcpy(d->text,s);return 0;
}
int kg_bitmap(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned w,unsigned h){
 (void)handle;(void)x;(void)y;(void)sx;(void)sy;(void)w;(void)h;assert(0);return -1;
}
int kg_bitmap_scaled(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned sw,unsigned sh,unsigned dw,unsigned dh){(void)handle;(void)x;(void)y;(void)sx;(void)sy;(void)sw;(void)sh;(void)dw;(void)dh;assert(0);return -1;}
int kg_bitmap_scaled_clip(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned sw,unsigned sh,unsigned dw,unsigned dh,unsigned cx,unsigned cy,unsigned cw,unsigned ch){(void)cx;(void)cy;(void)cw;(void)ch;return kg_bitmap_scaled(handle,x,y,sx,sy,sw,sh,dw,dh);}
int kg_present(void){return kg_validate(&frame,canvas_w,canvas_h);}
#define main image_guest_main
#include "apps/image.c"
#undef main
int main(void){
 for(unsigned lang=0;lang<2;lang++)for(unsigned large=0;large<2;large++){
  ku_language=lang;canvas_w=large?624:344;canvas_h=large?376:196;
  filename=(KwInput){.text="SAMPLE.BMP",.length=10,.cursor=10};
  rescue=(KwInput){.text="RESCUED.BMP",.length=11,.cursor=11};
  for(unsigned mode=0;mode<=4;mode++){
   modal=mode;message=0;assert(!draw());
   message="Inspect TMP/BAK / original kept";assert(!draw());
  }
 }
 return 0;
}
