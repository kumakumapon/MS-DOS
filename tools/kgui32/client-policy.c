/* No DOS, physical pointers or rendering. Shared with native policy tests. */
#include "client-policy.h"
#include "document.h"
#include "sdk/text.h"
static int text(const char *p,unsigned n){return kt_cstr(p,n)>=0;}
int kg_validate(const KgRequest *r,unsigned width,unsigned height){
 if(r->version!=1||r->size!=sizeof *r||r->operation<KG_OPEN||r->operation>KG_SURFACE_DESTROY)return -22;
 if(r->operation==KG_OPEN)return r->width<160||r->width>640||r->height<80||r->height>400||!r->title[0]||!text(r->title,32)?-22:0;
 if(r->operation==KG_SUBSCRIBE&&(r->count&~KG_CAP_SUPPORTED))return -22;
 if(r->operation==KG_EDIT_DOCUMENT){char command[64];return kg_document_command(r->count,r->title,command);}
 if(r->operation!=KG_FRAME)return 0;
 if(r->count>KG_COMMANDS)return -22;
 for(unsigned i=0;i<r->count;i++){
  const KgDraw *d=&r->draw[i];
  if(d->op==KG_BITMAP){if(d->x < -800||d->x>800||d->y < -600||d->y>600||d->w<1||d->w>256||d->h<1||d->h>192)return -22;continue;}
  if(d->op<KG_RECT||d->op>KG_BUTTON||d->x<0||d->y<0||(unsigned)d->x>=width||(unsigned)d->y>=height||d->color>0xffffff)return -22;
  if(d->op==KG_TEXT){
   if(!text(d->text,48)||height-(unsigned)d->y<16)return -22;
   unsigned n=0;while(d->text[n])n++;if(n>(width-(unsigned)d->x)/8)return -22;
  }else {
   if(d->w<=0||d->h<=0||(unsigned)d->w>width-(unsigned)d->x||(unsigned)d->h>height-(unsigned)d->y)return -22;
   if(d->op==KG_BUTTON){if(d->w<12||d->h<18||!text(d->text,48))return -22;unsigned n=0;while(d->text[n])n++;if(n>(unsigned)(d->w-4)/8)return -22;}
  }
 }
 return 0;
}
int kg_policy(KgClient *c,KgRequest *r,u32 generation,u32 *serial,unsigned width,unsigned height){
 int e=kg_validate(r,c->width,c->height);if(e)return e;
 if(r->operation==KG_OPEN){
  if(c->token&&c->generation==generation)return -16;
  unsigned w=r->width+16,h=r->height+104;
  if(w<360)w=360;
  if(w>width-32)w=width-32;
  if(h<272)h=272;
  if(h>height-80)h=height-80;
  *c=(KgClient){.generation=generation,.token=++*serial,.revision=c->revision+1,.width=w-16,.height=h-104};
  if(!c->token)c->token=++*serial;
  for(unsigned i=0;i<32;i++)c->title[i]=r->title[i];
  r->token=c->token;r->width=c->width;r->height=c->height;return (int)c->token;
 }
 if(c->generation!=generation||!c->token||c->token!=r->token)return -116;
 if(r->operation==KG_EDIT_DOCUMENT){
  if(c->edit_pending)return -16;
  c->edit_pending=1;c->edit_namespace=r->count;c->edit_result=-11;
  for(unsigned i=0;i<13;i++){c->edit_name[i]=r->title[i];if(!r->title[i])break;}
  return 0;
 }
 if(r->operation==KG_EDIT_STATUS)return c->edit_result;
 if(r->operation>=KG_SURFACE_CREATE){
  int result=kg_surface_policy(&c->surface,r,serial);
  if(result>=0&&kg_surface_operation(r->operation)){if(++c->revision==0)c->revision=1;c->surface.pending=c->revision;}
  return result;
 }
 if(r->operation==KG_CAPABILITIES)return KG_CAP_SUPPORTED|KG_CAP_BITMAP;
 if(r->operation==KG_SUBSCRIBE){c->input_flags=r->count;c->revision++;return 0;}
 if(r->operation==KG_CANCEL_CLOSE){if(!(c->input_flags&KG_INPUT_CLOSE))return -22;c->cancelled++;c->revision++;return 0;}
 if(r->operation==KG_CLIP_SET||r->operation==KG_CLIP_GET)return r->count>1024?-22:0;
 if(r->operation==KG_CLOSE){c->surface=(KgSurface){0};c->edit_pending=0;c->input_flags=0;c->token=0;c->head=c->count=0;c->revision++;return 0;}
 if(r->operation==KG_FRAME){
  if(c->surface.pending)return -11;
  for(unsigned i=0;i<r->count;i++)if(r->draw[i].op==KG_BITMAP){int e=kg_surface_draw(&c->surface,&r->draw[i]);if(e)return e;}
  c->revision++;return 0;
 }
 r->event=(KgEvent){0};
 if(c->count){r->event=c->event[c->head];c->head=(c->head+1)%KG_EVENTS;c->count--;}
 r->width=c->width;r->height=c->height;return r->event.type?1:0;
}
void kg_enqueue(KgClient *c,const KgEvent *e){
 if(!c->token)return;
 if(e->type==KG_RESIZE){c->width=e->width;c->height=e->height;}
 /* Coalesce motion and reserve space for releases/keys. SDK1 never opts in. */
 if(e->type==KG_POINTER){
  if(!(c->input_flags&KG_INPUT_MOTION))return;
  if(c->count&&c->event[(c->head+c->count-1)%KG_EVENTS].type==KG_POINTER){c->event[(c->head+c->count-1)%KG_EVENTS]=*e;return;}
  if(c->count>=KG_EVENTS-2){c->dropped++;return;}
 }
 if(c->count&&e->type==KG_RESIZE&&c->event[(c->head+c->count-1)%KG_EVENTS].type==KG_RESIZE){c->event[(c->head+c->count-1)%KG_EVENTS]=*e;return;}
 if(c->count==KG_EVENTS){c->head=(c->head+1)%KG_EVENTS;c->count--;c->dropped++;}
 unsigned at=(c->head+c->count++)%KG_EVENTS;c->event[at]=*e;
 if(e->type==KG_KEY&&!(c->input_flags&KG_INPUT_MODIFIERS))c->event[at].control=0;
 c->received++;
}
