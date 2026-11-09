#include "kgui.h"
static KgRequest request;static unsigned token,width,height;
static void copy(char *to,const char *from,unsigned n){unsigned i=0;for(;i+1<n&&from[i];i++)to[i]=from[i];to[i]=0;}
static int call(unsigned op){request.version=1;request.size=sizeof request;request.operation=op;request.token=token;return k32_call(13,(u32)&request,sizeof request);}
int kg_open(const char *title,unsigned w,unsigned h){
 if(!(k32_query()&KG_FEATURE))return -38;
 request=(KgRequest){.width=w,.height=h};copy(request.title,title,sizeof request.title);
 int e=call(KG_OPEN);if(e<0)return e;token=e;width=request.width;height=request.height;return e;
}
void kg_begin(void){request.count=0;}
static KgDraw *add(unsigned op,int x,int y,int w,int h,u32 color){
 if(request.count==KG_COMMANDS)return 0;
 KgDraw *d=&request.draw[request.count++];*d=(KgDraw){.op=op,.x=x,.y=y,.w=w,.h=h,.color=color};return d;
}
int kg_rect(int x,int y,int w,int h,u32 c){return add(KG_RECT,x,y,w,h,c)?0:-28;}
int kg_text(int x,int y,const char *s,u32 c){KgDraw *d=add(KG_TEXT,x,y,0,0,c);if(!d)return -28;copy(d->text,s,sizeof d->text);return 0;}
int kg_button(u32 id,int x,int y,int w,int h,const char *s){KgDraw *d=add(KG_BUTTON,x,y,w,h,0xdce7ef);if(!d)return -28;d->id=id;copy(d->text,s,sizeof d->text);return 0;}
int kg_present(void){return call(KG_FRAME);}
int kg_poll(KgEvent *e){int n=call(KG_POLL);if(n>=0){*e=request.event;width=request.width;height=request.height;}return n;}
int kg_close(void){int e=call(KG_CLOSE);if(!e)token=0;return e;}
int kg_capabilities(void){return call(KG_CAPABILITIES);}
int kg_subscribe(unsigned flags){request.count=flags;return call(KG_SUBSCRIBE);}
unsigned kg_width(void){return width;}
unsigned kg_height(void){return height;}

int kg_clip_set(const char *s,unsigned n){if(n>1024)return -22;for(unsigned i=0;i<n;i++)((char *)request.draw)[i]=s[i];request.count=n;return call(KG_CLIP_SET);}
int kg_clip_get(char *s,unsigned n){if(n>1024)n=1024;request.count=n;int e=call(KG_CLIP_GET);if(e>0)for(int i=0;i<e;i++)s[i]=((char *)request.draw)[i];return e;}
int kg_cancel_close(void){return call(KG_CANCEL_CLOSE);}

int kg_edit_document(const char *name,unsigned work){copy(request.title,name,sizeof request.title);request.count=work;return call(KG_EDIT_DOCUMENT);}
int kg_edit_status(void){return call(KG_EDIT_STATUS);}
