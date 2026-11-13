#include "kgui.h"
#include "text.h"
static KgRequest request;static unsigned token,width,height,surface_handle,surface_width,surface_height;
static void copy(char *to,const char *from,unsigned n){unsigned i=0;while(from[i]){unsigned k=kt_unit(from+i,from[i+1]?2:1);if(!k)k=1;if(k>=n-i)break;for(unsigned j=0;j<k;j++)to[i+j]=from[i+j];i+=k;}to[i]=0;}
static int call(unsigned op){request.version=1;request.size=sizeof request;request.operation=op;request.token=token;return k32_call(13,(u32)&request,sizeof request);}
int kg_open(const char *title,unsigned w,unsigned h){
 if(!(k32_query()&KG_FEATURE))return -38;
 request=(KgRequest){.width=w,.height=h};copy(request.title,title,sizeof request.title);
 int e=call(KG_OPEN);if(e<0)return e;token=e;width=request.width;height=request.height;surface_handle=0;return e;
}
void kg_begin(void){request.count=0;request.width=width;request.height=height;}
static KgDraw *add(unsigned op,int x,int y,int w,int h,u32 color){
 if(request.count==KG_COMMANDS)return 0;
 KgDraw *d=&request.draw[request.count++];*d=(KgDraw){.op=op,.x=x,.y=y,.w=w,.h=h,.color=color};return d;
}
int kg_rect(int x,int y,int w,int h,u32 c){return add(KG_RECT,x,y,w,h,c)?0:-28;}
int kg_text(int x,int y,const char *s,u32 c){KgDraw *d=add(KG_TEXT,x,y,0,0,c);if(!d)return -28;copy(d->text,s,sizeof d->text);return 0;}
int kg_button(u32 id,int x,int y,int w,int h,const char *s){KgDraw *d=add(KG_BUTTON,x,y,w,h,0xdce7ef);if(!d)return -28;d->id=id;copy(d->text,s,sizeof d->text);return 0;}
int kg_present(void){return call(KG_FRAME);}
int kg_poll(KgEvent *e){int n=call(KG_POLL);if(n>=0){*e=request.event;width=request.width;height=request.height;}return n;}
int kg_close(void){int e=call(KG_CLOSE);if(!e){token=0;surface_handle=0;}return e;}
int kg_capabilities(void){return call(KG_CAPABILITIES);}
int kg_subscribe(unsigned flags){request.count=flags;return call(KG_SUBSCRIBE);}
unsigned kg_width(void){return width;}
unsigned kg_height(void){return height;}

int kg_clip_set(const char *s,unsigned n){if(n>1024)return -22;for(unsigned i=0;i<n;i++)((char *)request.draw)[i]=s[i];request.count=n;return call(KG_CLIP_SET);}
int kg_clip_get(char *s,unsigned n){if(n>1024)n=1024;request.count=n;int e=call(KG_CLIP_GET);if(e>0)for(int i=0;i<e;i++)s[i]=((char *)request.draw)[i];return e;}
int kg_cancel_close(void){return call(KG_CANCEL_CLOSE);}

int kg_edit_document(const char *name,unsigned work){copy(request.title,name,sizeof request.title);request.count=work;return call(KG_EDIT_DOCUMENT);}
int kg_edit_status(void){return call(KG_EDIT_STATUS);}

/* Surface transfers are bounded, acknowledged between desktop turns, and
 * must precede kg_begin(). Caller pixels always remain in its private pages. */
static int surface_wait(unsigned handle){
 for(unsigned n=0;n<2048;n++){request.width=handle;int e=call(KG_SURFACE_STATUS);if(e!=-11)return e;k32_wait(K32_WAIT_TIME,16);}return -110;
}
int kg_surface_create(unsigned w,unsigned h){
 if(!(kg_capabilities()&KG_CAP_BITMAP))return -38;
 request.width=w;request.height=h;request.count=0;int handle=call(KG_SURFACE_CREATE);if(handle<0)return handle;
 int e=surface_wait(handle);if(e<0)return e;surface_handle=handle;surface_width=w;surface_height=h;return handle;
}
int kg_surface_upload(unsigned handle,const u16 *pixels,unsigned bytes){
 if(handle!=surface_handle)return -116;
 if(!bytes||bytes!=surface_width*surface_height*2||(bytes&1)||bytes>256*192*2)return -22;
 for(unsigned at=0;at<bytes;){unsigned n=bytes-at;if(n>sizeof request.draw)n=sizeof request.draw;
  request.width=handle;request.height=at;request.count=n;
  for(unsigned i=0;i<n;i++)((u8 *)request.draw)[i]=((const u8 *)pixels)[at+i];
  int e=call(KG_SURFACE_UPLOAD);if(e<0)return e;e=surface_wait(handle);if(e<0)return e;at+=n;
 }return 0;
}
int kg_surface_destroy(unsigned handle){if(handle!=surface_handle)return -116;request.width=handle;int e=call(KG_SURFACE_DESTROY);if(e>=0)e=surface_wait(handle);if(!e)surface_handle=0;return e;}
int kg_bitmap(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned w,unsigned h){
 if(handle!=surface_handle)return -116;
 if(sx>=surface_width||sy>=surface_height||!w||w>surface_width-sx||!h||h>surface_height-sy)return -22;
 KgDraw *d=add(KG_BITMAP,x,y,w,h,sy<<16|sx);if(!d)return -28;d->id=handle;return 0;
}
