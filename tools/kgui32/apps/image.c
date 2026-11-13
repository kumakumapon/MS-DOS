/* Independent bounded BMP viewer and in-memory image editor. */
#include "../sdk/kgui.h"
#include "../sdk/widgets.h"
#include "../sdk/bitmap.h"
typedef struct {u32 magic,version,frames,events,width,height,image_width,image_height,hash,original_hash,uploads,surface,heartbeat,error,dirty,mode;} ImageStats;
volatile ImageStats image_stats __attribute__((section(".data.gui_stats")))={.magic=0x3149474b,.version=1};
static u16 pixels[256*192],original[256*192],undo_pixels[256*192],candidate[256*192];
static KgBmp image;static KwInput filename;static unsigned handle,sw,sh,undo_valid,focused,paintmode,painting;static int panx,pany,lastx,lasty;
static const char *message;
static unsigned hash(const u16 *p,unsigned n){unsigned v=2166136261u;const u8 *b=(const u8 *)p;for(unsigned i=0;i<n*2;i++)v=(v^b[i])*16777619;return v;}
static void copy(u16 *to,const u16 *from,unsigned n){for(unsigned i=0;i<n;i++)to[i]=from[i];}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void stats(void){image_stats.width=kg_width();image_stats.height=kg_height();image_stats.image_width=image.width;image_stats.image_height=image.height;image_stats.hash=hash(pixels,image.width*image.height);image_stats.dirty=image_stats.hash!=image_stats.original_hash;image_stats.surface=handle;image_stats.mode=focused|paintmode<<1|painting<<2;}
static int upload(void){
 if(!handle||sw!=image.width||sh!=image.height){
  if(handle){int e=kg_surface_destroy(handle);if(e<0)return e;handle=0;}
  int h=kg_surface_create(image.width,image.height);if(h<0)return h;handle=h;sw=image.width;sh=image.height;
 }
 int e=kg_surface_upload(handle,pixels,image.width*image.height*2);if(!e)image_stats.uploads++;return e;
}
static int exact(unsigned h,void *p,unsigned n){u8 *b=p;unsigned at=0;while(at<n){int got=k32_read(h,b+at,n-at);if(got<=0)return got?got:-5;at+=got;}return 0;}
static int load(void){
 unsigned n=filename.length;
 if(n<5||n>12)return -22;
 int opened=k32_open(filename.text);if(opened<0)return opened;
 unsigned h=opened;u8 header[54],line[768],extra;KgBmp next;int e=exact(h,header,sizeof header);
 u32 bytes=e?0:(u32)header[2]|(u32)header[3]<<8|(u32)header[4]<<16|(u32)header[5]<<24;
 if(!e)e=kg_bmp_header(header,sizeof header,bytes,&next);
 if(!e){e=k32_seek(h,bytes);if(e>=0)e=k32_read(h,&extra,1)==0?0:-22;}
 if(!e){e=k32_seek(h,next.offset);if(e>=0)e=0;}
 for(unsigned row=0;!e&&row<next.height;row++){e=exact(h,line,next.stride);if(!e)e=kg_bmp_row(&next,row,line,next.stride,candidate,256*192);}
 int closed=k32_close(h);if(!e&&closed<0)e=closed;if(e)return e;
 /* Rejected files leave the pixels, original, Undo and live surface intact. */
 image=next;copy(pixels,candidate,image.width*image.height);copy(original,pixels,image.width*image.height);undo_valid=painting=0;panx=pany=0;
 image_stats.original_hash=hash(original,image.width*image.height);return upload();
}
static void backup(void){copy(undo_pixels,pixels,image.width*image.height);undo_valid=1;}
static int change(unsigned key){
 if(!image.width)return 0;
 unsigned n=image.width*image.height;
 if(key=='i'){backup();kg_bitmap_invert(pixels,n);}
 else if(key=='h'){backup();kg_bitmap_flip(pixels,image.width,image.height);}
 else if(key=='u'){if(!undo_valid)return 0;for(unsigned i=0;i<n;i++){u16 p=pixels[i];pixels[i]=undo_pixels[i];undo_pixels[i]=p;}}
 else if(key=='r'){backup();copy(pixels,original,n);}
 else return 0;
 return upload();
}
static int draw(void){
 stats();unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 kw_draw_input((KwRect){8,4,160,28},&filename,focused);kg_button('o',176,4,72,28,"Load");kg_button('q',w-80,4,72,28,"Close");
 kg_button('i',8,40,72,28,"Invert");kg_button('h',88,40,72,28,"Flip H");kg_button('u',168,40,72,28,"Undo");kg_button('r',248,40,80,28,paintmode?"Painting":"Paint");
 kg_rect(8,80,w-16,h-116,0x182c46);
 if(handle&&image.width){
  int x=16+panx,y=88+pany,sx=x<8?8-x:0,sy=y<80?80-y:0;x+=sx;y+=sy;
  int width=(int)image.width-sx,height=(int)image.height-sy;
  if(x+width>(int)w-8)width=(int)w-8-x;
  if(y+height>(int)h-36)height=(int)h-36-y;
  if(width>0&&height>0)kg_bitmap(handle,x,y,sx,sy,width,height);
 }
 kg_text(8,h-28,message?message:paintmode?"Paint: drag / P off / U undo":image_stats.dirty?"Modified / memory only":"BMP24 / arrows pan / edits in memory",0x263e58);
 int e=kg_present();if(!e)image_stats.frames++;return e;
}
int main(int argc,char **argv){
 int token=kg_open("Images / BMP24",328,196);if(token<0){image_stats.error=-token;return 1;}
 kg_subscribe(KG_INPUT_TAB|KG_INPUT_KEYS|KG_INPUT_MOTION);
 const char *name=argc>2&&equal(argv[1],"root")?argv[2]:argc>1?argv[1]:"SAMPLE.BMP";
 for(unsigned i=0;name[i]&&i<12;i++)filename.text[i]=name[i];
 filename.length=k32_len(filename.text);filename.cursor=filename.length;
 int e=k32_len(name)>12?-22:load();if(e<0){image_stats.error=-e;message="Unsupported BMP or read error";}
 if(draw()<0)return 2;
 for(;;){
  KgEvent v;int got=kg_poll(&v);if(got<0)return 3;image_stats.heartbeat++;unsigned key=0,dirty=0;e=0;
  if(got){image_stats.events++;
   if(v.type==KG_RESIZE)dirty=1;
   if((v.type==KG_MOUSE||v.type==KG_POINTER)&&!(v.key&1)&&painting){painting=0;dirty=1;}
   if(paintmode&&image.width&&(v.type==KG_MOUSE||v.type==KG_POINTER)&&(v.key&1)&&kw_inside((KwRect){8,80,(int)kg_width()-16,(int)kg_height()-116},v.x,v.y)){
    int x=v.x-16-panx,y=v.y-88-pany;
    if(x>=0&&y>=0&&(unsigned)x<image.width&&(unsigned)y<image.height){
     if(!painting){backup();lastx=x;lasty=y;painting=1;}
     kg_bitmap_line(pixels,image.width,image.height,lastx,lasty,x,y,0xf81f);lastx=x;lasty=y;e=upload();if(!e){message=0;image_stats.error=0;}dirty=1;
    }
   }else if(v.type==KG_MOUSE&&(v.key&1)){if(kw_inside((KwRect){8,4,160,28},v.x,v.y)){focused=1;dirty=1;}else{focused=0;key=v.control;}}
   if(v.type==KG_KEY){
    if(v.key==9){focused^=1;dirty=1;}
    else if(focused&&v.key==13)key='o';
    else if(focused){if(kw_input(&filename,&v)){dirty=1;message=0;}}
    else if(v.key>=256&&v.key<=259){panx+=v.key==258?-8:v.key==259?8:0;pany+=v.key==256?-8:v.key==257?8:0;if(panx < -256)panx=-256;if(panx>256)panx=256;if(pany < -192)pany=-192;if(pany>192)pany=192;dirty=1;}
    else key=v.key;
   }
  }
  if(key=='q'||key=='Q')break;
  if(key=='o'||key=='O'){e=load();focused=0;dirty=1;}
  else if(key=='p'||key=='P'){paintmode^=1;painting=0;dirty=1;}
  else if(key=='i'||key=='h'||key=='u'||key=='r'){e=change(key);dirty=1;}
  if(dirty){if(e<0){image_stats.error=-e;message="Unsupported BMP or read error";}else if(key){image_stats.error=0;message=0;}e=draw();if(e<0){image_stats.error=-e;return 4;}}
  k32_wait(K32_WAIT_TIME,50);
 }
 if(handle)kg_surface_destroy(handle);
 return kg_close()?5:0;
}
