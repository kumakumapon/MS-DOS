#include "kgui.h"
#include "text.h"
#include "i18n.h"
#include "menu.h"
#include "../../desktop-style.h"
static KmMenu menu;static unsigned input_flags;
void kg_menu(const KmItem *items,unsigned count){menu=(KmMenu){.items=items,.count=count<=KM_MAX?count:0,.enabled=1};}
void kg_menu_enable(unsigned enabled){menu.enabled=enabled;}
void kg_menu_state(unsigned enabled,unsigned checked){menu.disabled=~enabled;menu.checked=checked;}
void kg_menu_labels(const KuPair *labels,unsigned count){menu.labels=labels;menu.label_count=count;}
unsigned ku_language;static unsigned language_dirty,large_text;
static KgRequest request;static unsigned token,width,height,surface_handle[2],surface_width[2],surface_height[2];
static void copy(char *to,const char *from,unsigned n){unsigned i=0;while(from[i]){unsigned k=kt_unit(from+i,from[i+1]?2:1);if(!k)k=1;if(k>=n-i)break;for(unsigned j=0;j<k;j++)to[i+j]=from[i+j];i+=k;}to[i]=0;}
static int call(unsigned op){request.version=1;request.size=sizeof request;request.operation=op;request.token=token;return k32_call(13,(u32)&request,sizeof request);}
int kg_open(const char *title,unsigned w,unsigned h){
 if(!(k32_query()&KG_FEATURE))return -38;
 request=(KgRequest){.width=w,.height=h};copy(request.title,title,sizeof request.title);
 int e=call(KG_OPEN);if(e<0)return e;token=e;width=request.width;height=request.height;surface_handle[0]=surface_handle[1]=0;kg_subscribe(0);return e;
}
void kg_begin(void){request.count=0;request.width=width;request.height=height;}
static KgDraw *add(unsigned op,int x,int y,int w,int h,u32 color){
 if(request.count==KG_COMMANDS)return 0;
 KgDraw *d=&request.draw[request.count++];*d=(KgDraw){.op=op,.x=x,.y=y,.w=w,.h=h,.color=color};return d;
}
int kg_rect(int x,int y,int w,int h,u32 c){return add(KG_RECT,x,y,w,h,c)?0:-28;}
static int draw_text(unsigned op,int x,int y,const char *s,u32 c){KgDraw *d=add(op,x,y,0,0,c);if(!d)return -28;copy(d->text,s,sizeof d->text);return 0;}
int kg_text(int x,int y,const char *s,u32 c){return draw_text(KG_TEXT,x,y,s,c);}
int kg_text_double(int x,int y,const char *s,u32 c){return draw_text(KG_TEXT_DOUBLE,x,y,s,c);}
unsigned kg_large_text(void){return large_text;}
int kg_button_color(u32 id,int x,int y,int w,int h,const char *s,u32 color){KgDraw *d=add(KG_BUTTON,x,y,w,h,color);if(!d)return -28;d->id=id;copy(d->text,s,sizeof d->text);return 0;}
int kg_button(u32 id,int x,int y,int w,int h,const char *s){return kg_button_color(id,x,y,w,h,s,DESKTOP_COLOR_SECONDARY_BUTTON);}
int kg_present(void){if(menu.open)km_draw(&menu);return call(KG_FRAME);}
int kg_poll(KgEvent *e){int n=call(KG_POLL);if(n>=0){
 *e=request.event;width=request.width;height=request.height;
 unsigned language=!!(e->control&KG_LANGUAGE_JA),large=!!(e->control&KG_LARGE_TEXT);e->control&=~(KG_LANGUAGE_JA|KG_LARGE_TEXT);
 if(large!=large_text){large_text=large;language_dirty=1;}
 if(language!=ku_language){ku_language=language;language_dirty=1;}
 if(!n&&language_dirty){language_dirty=0;*e=(KgEvent){.type=KG_RESIZE,.width=width,.height=height,.seconds=e->seconds};return 1;}
 if(n){unsigned was=menu.open;int handled=km_event(&menu,e,height);
  if(was!=menu.open)kg_subscribe(input_flags);
  if(was&&!menu.open&&!handled)language_dirty=1;
  if(handled==1)*e=(KgEvent){.type=KG_RESIZE,.width=width,.height=height,.seconds=e->seconds};
 }
 }return n;}
int kg_close(void){int e=call(KG_CLOSE);if(!e){token=0;surface_handle[0]=surface_handle[1]=0;}return e;}
int kg_date(unsigned *year,unsigned *month,unsigned *day){
 int result=call(KG_DATE);
 if(result<0)return result;
 unsigned y=(unsigned)result>>16,m=(unsigned)result>>8&255u,d=(unsigned)result&255u;
 if(y<1980||y>2099||m<1||m>12||d<1||d>31)return -5;
 *year=y;*month=m;*day=d;
 return 0;
}
int kg_preferences_changed(void){return call(KG_PREFERENCES_CHANGED);}
int kg_capabilities(void){return call(KG_CAPABILITIES);}
int kg_subscribe(unsigned flags){input_flags=flags;request.count=flags|KG_INPUT_LANGUAGE|(menu.count?KG_INPUT_MENU:0)|(menu.open?KG_INPUT_TAB:0);int e=call(KG_SUBSCRIBE);if(e==-22){request.count=flags;e=call(KG_SUBSCRIBE);}return e;}
unsigned kg_width(void){return width;}
unsigned kg_height(void){return height;}

int kg_clip_set(const char *s,unsigned n){if(n>1024)return -22;for(unsigned i=0;i<n;i++)((char *)request.draw)[i]=s[i];request.count=n;return call(KG_CLIP_SET);}
int kg_clip_get(char *s,unsigned n){if(n>1024)n=1024;request.count=n;int e=call(KG_CLIP_GET);if(e>0)for(int i=0;i<e;i++)s[i]=((char *)request.draw)[i];return e;}
int kg_note_document(const char *path,unsigned volume,unsigned bitmap){
 char *out=(char *)request.draw;
 unsigned n=0;for(;n<KG_RECENT_PATH-1&&path[n];n++)out[n]=path[n];
 if(path[n])return -22;
 out[n]=0;request.width=volume;request.height=bitmap;
 return call(KG_RECENT_DOCUMENT);
}
int kg_note_move(const char *path,const char *destination){
 char *out=(char *)request.draw;
 const char *paths[]={path,destination};
 for(unsigned i=0;i<2;i++,out+=KG_RECENT_PATH){
  unsigned n=0;
  for(;n<KG_RECENT_PATH-1&&paths[i][n];n++)out[n]=paths[i][n];
  if(!n||paths[i][n])return -22;
  out[n]=0;
 }
 return call(KG_RECENT_MOVE);
}
int kg_recent_volume(const char *arg){
 if(arg[0]!='@'||!arg[1]||arg[2])return -1;
 if(arg[1]=='-')return 0;
 return arg[1]>='A'&&arg[1]<='Z'?arg[1]-'A'+1:-1;
}
int kg_clip_image_copy(const u16 *pixels,unsigned w,unsigned h){
 if(!w||!h||w>KG_CLIP_IMAGE_SIDE||h>KG_CLIP_IMAGE_SIDE)return -27;
 request.width=w;request.height=h;
 int e=call(KG_IMAGE_COPY_BEGIN);if(e<0)return e;
 const u8 *bytes=(const u8 *)pixels;
 for(unsigned at=0,total=w*h*2;at<total;){
  unsigned n=total-at;if(n>sizeof request.draw)n=sizeof request.draw;
  request.width=at;request.count=n;
  kg_transport_copy(request.draw,bytes+at,n);
  e=call(KG_IMAGE_COPY_DATA);if(e<0)return e;at+=n;
 }
 return 0;
}
int kg_clip_image_paste(u16 *pixels,unsigned capacity,unsigned *width,unsigned *height){
 int e=call(KG_IMAGE_PASTE_INFO);if(e<0)return e;
 unsigned w=request.width,h=request.height,serial=request.count;
 if(!w||!h||w>KG_CLIP_IMAGE_SIDE||h>KG_CLIP_IMAGE_SIDE||w*h>capacity)return -27;
 u8 *bytes=(u8 *)pixels;
 for(unsigned at=0,total=w*h*2;at<total;){
  unsigned n=total-at;if(n>sizeof request.draw)n=sizeof request.draw;
  request.width=at;request.height=serial;request.count=n;
  e=call(KG_IMAGE_PASTE_DATA);if(e<0)return e;
  kg_transport_copy(bytes+at,request.draw,n);at+=n;
 }
 *width=w;*height=h;return 0;
}
int kg_cancel_close(void){return call(KG_CANCEL_CLOSE);}

int kg_edit_document(const char *name,unsigned work){copy(request.title,name,sizeof request.title);request.count=work;return call(KG_EDIT_DOCUMENT);}
int kg_edit_status(void){return call(KG_EDIT_STATUS);}

/* Surface transfers are bounded, acknowledged between desktop turns, and
 * must precede kg_begin(). Caller pixels always remain in its private pages. */
static int surface_wait(unsigned handle){
 for(unsigned n=0;n<2048;n++){request.width=handle;int e=call(KG_SURFACE_STATUS);if(e!=-11)return e;k32_wait(K32_WAIT_TIME,16);}return -110;
}
static int create_surface(unsigned index,unsigned w,unsigned h){
 if(!(kg_capabilities()&KG_CAP_BITMAP))return -38;
 request.width=w;request.height=h;request.count=index;int handle=call(KG_SURFACE_CREATE);if(handle<0)return handle;
 int e=surface_wait(handle);if(e<0)return e;surface_handle[index]=handle;surface_width[index]=w;surface_height[index]=h;return handle;
}
int kg_surface_create(unsigned w,unsigned h){return create_surface(0,w,h);}
int kg_surface_create_extra(unsigned w,unsigned h){if(!(kg_capabilities()&KG_CAP_SURFACES))return -38;return create_surface(1,w,h);}
static int surface_index(unsigned handle){if(handle&&handle==surface_handle[0])return 0;if(handle&&handle==surface_handle[1])return 1;return -1;}
int kg_surface_upload(unsigned handle,const u16 *pixels,unsigned bytes){
 int index=surface_index(handle);if(index<0)return -116;
 if(!bytes||bytes!=surface_width[index]*surface_height[index]*2||(bytes&1)||bytes>384*256*2)return -22;
 for(unsigned at=0;at<bytes;){unsigned n=bytes-at;if(n>sizeof request.draw)n=sizeof request.draw;
  request.width=handle;request.height=at;request.count=n;
  kg_transport_copy(request.draw,(const u8 *)pixels+at,n);
  int e=call(KG_SURFACE_UPLOAD);if(e<0)return e;e=surface_wait(handle);if(e<0)return e;at+=n;
 }return 0;
}
int kg_surface_destroy(unsigned handle){int index=surface_index(handle);if(index<0)return -116;request.width=handle;int e=call(KG_SURFACE_DESTROY);if(e>=0)e=surface_wait(handle);if(!e)surface_handle[index]=0;return e;}
int kg_bitmap(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned w,unsigned h){
 int index=surface_index(handle);if(index<0)return -116;
 if(sx>=surface_width[index]||sy>=surface_height[index]||!w||w>surface_width[index]-sx||!h||h>surface_height[index]-sy)return -22;
 KgDraw *d=add(KG_BITMAP,x,y,w,h,sy<<16|sx);if(!d)return -28;d->id=handle;return 0;
}

int kg_bitmap_scaled(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned sw,unsigned sh,unsigned dw,unsigned dh){
 int index=surface_index(handle);if(index<0)return -116;
 if(sx>=surface_width[index]||sy>=surface_height[index]||!sw||!sh||sw>surface_width[index]-sx||sh>surface_height[index]-sy||!dw||dw>4096||!dh||dh>4096)return -22;
 KgDraw *d=add(KG_BITMAP_SCALE,x,y,dw,dh,sy<<16|sx);if(!d)return -28;d->id=handle;d->text[0]=sw;d->text[1]=sw>>8;d->text[2]=sh;d->text[3]=sh>>8;return 0;
}
int kg_bitmap_scaled_clip(unsigned handle,int x,int y,unsigned sx,unsigned sy,unsigned sw,unsigned sh,unsigned dw,unsigned dh,unsigned cx,unsigned cy,unsigned cw,unsigned ch){
 if(!cw||!ch||cx>=width||cy>=height||cw>width-cx||ch>height-cy)return -22;
 int e=kg_bitmap_scaled(handle,x,y,sx,sy,sw,sh,dw,dh);if(e)return e;
 KgDraw *d=&request.draw[request.count-1];unsigned clip[4]={cx,cy,cw,ch};for(unsigned i=0;i<4;i++){d->text[4+i*2]=clip[i];d->text[5+i*2]=clip[i]>>8;}return 0;
}
