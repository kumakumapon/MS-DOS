/* Bounded BMP editor; Work writes use the native revision-checked broker. */
#include "../sdk/kgui.h"
#include "../sdk/widgets.h"
#include "../sdk/bitmap.h"
typedef struct {u32 magic,version,frames,events,width,height,image_width,image_height,hash,original_hash,uploads,surface,heartbeat,error,dirty,mode;} ImageStats;
volatile ImageStats image_stats __attribute__((section(".data.gui_stats")))={.magic=0x3149474b,.version=1};
static u16 pixels[256*192],original[256*192],undo_pixels[256*192],candidate[256*192];
static KgBmp image;static KwInput filename;static unsigned handle,sw,sh,undo_valid,focused,paintmode,painting;static int panx,pany,lastx,lasty;
static const char *message;
static unsigned work,loaded_work,modal,pending,kind;
static char current[13],recovery_base[13];
static KwInput rescue;
static K32Work revision,replacement;
static K32RecoveryInfo recovery;
static u8 encoded[K32_BMP_LIMIT];
static unsigned raw_hash(const u8 *p,unsigned n){unsigned v=2166136261u;while(n--)v=(v^*p++)*16777619;return v;}
static void namecopy(char *to,const char *from){unsigned i=0;for(;i<12&&from[i];i++)to[i]=from[i];to[i]=0;}
static K32Work request(unsigned op,const char *name){K32Work r=k32_work_name(op,name);r.flags=K32_WORK_BMP;return r;}

static unsigned hash(const u16 *p,unsigned n){unsigned v=2166136261u;const u8 *b=(const u8 *)p;for(unsigned i=0;i<n*2;i++)v=(v^b[i])*16777619;return v;}
static void copy(u16 *to,const u16 *from,unsigned n){for(unsigned i=0;i<n;i++)to[i]=from[i];}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void stats(void){image_stats.width=kg_width();image_stats.height=kg_height();image_stats.image_width=image.width;image_stats.image_height=image.height;image_stats.hash=hash(pixels,image.width*image.height);image_stats.dirty=image_stats.hash!=image_stats.original_hash;image_stats.surface=handle;image_stats.mode=focused|paintmode<<1|painting<<2|work<<3|modal<<4;}
static int upload(void){
 if(!handle||sw!=image.width||sh!=image.height){
  if(handle){int e=kg_surface_destroy(handle);if(e<0)return e;handle=0;}
  int h=kg_surface_create(image.width,image.height);if(h<0)return h;handle=h;sw=image.width;sh=image.height;
 }
 int e=kg_surface_upload(handle,pixels,image.width*image.height*2);if(!e)image_stats.uploads++;return e;
}
static int exact(unsigned h,void *p,unsigned n){u8 *b=p;unsigned at=0;while(at<n){unsigned chunk=n-at;if(chunk>4096)chunk=4096;int got=k32_read(h,b+at,chunk);if(got<=0)return got?got:-5;at+=got;}return 0;}
static int decode(unsigned h,KgBmp *next,const K32WorkRevision *expected){
 u8 header[54],extra;int e=exact(h,header,54);
 unsigned bytes=e?0:(u32)header[2]|(u32)header[3]<<8|(u32)header[4]<<16|(u32)header[5]<<24;
 if(!e&&(bytes>K32_BMP_LIMIT||kg_bmp_header(header,54,bytes,next)))e=-22;
 if(!e){e=k32_seek(h,0);if(e>=0)e=exact(h,encoded,bytes);}
 if(!e){int got=k32_read(h,&extra,1);if(got)e=got<0?got:-22;}
 if(!e&&expected&&(bytes!=expected->length||raw_hash(encoded,bytes)!=expected->hash))e=-116;
 for(unsigned row=0;!e&&row<next->height;row++)e=kg_bmp_row(next,row,encoded+next->offset+row*next->stride,next->stride,candidate,256*192);
 int closed=k32_close(h);return e?e:closed;
}
static int load(void){
 unsigned n=filename.length;if(n<5||n>12)return -22;
 K32Work next_revision=request(K32_WORK_STAT,filename.text);int e=0;
 if(work){e=k32_work(&next_revision);if(e<0)return e;if(!next_revision.exists)return -2;}
 K32Work opened=request(K32_WORK_OPEN,filename.text);
 int h=work?k32_work(&opened):k32_open(filename.text);if(h<0)return h;
 KgBmp next;K32WorkRevision expected={next_revision.exists,next_revision.length,next_revision.hash,0};
 e=decode(h,&next,work?&expected:0);if(e)return e;
 /* A rejected file never changes the image, baseline or Undo. */
 image=next;copy(pixels,candidate,image.width*image.height);copy(original,pixels,image.width*image.height);undo_valid=painting=0;panx=pany=0;
 revision=next_revision;loaded_work=work;namecopy(current,filename.text);
 image_stats.original_hash=hash(original,image.width*image.height);return upload();
}
static int save(unsigned approved){
 if(!image.width)return -22;
 K32Work r=request(K32_WORK_STAT,filename.text);int e;
 if(approved)r=replacement;
 else if(loaded_work&&equal(current,filename.text))r=revision;
 else {e=k32_work(&r);if(e<0)return e;if(r.exists){replacement=r;modal=1;return 2;}}
 int bytes=kg_bmp_encode(pixels,image.width,image.height,encoded,sizeof encoded);if(bytes<0)return bytes;
 r.operation=K32_WORK_SAVE;r.data=(unsigned)encoded;r.bytes=bytes;
 e=k32_work(&r);if(e<0)return e;
 revision=request(K32_WORK_STAT,r.name);revision.exists=1;revision.length=bytes;revision.hash=raw_hash(encoded,bytes);
 namecopy(current,r.name);loaded_work=1;copy(original,pixels,image.width*image.height);image_stats.original_hash=hash(original,image.width*image.height);
 message=e?"Saved / BAK retained: inspect recovery":"Saved to Work";return 0;
}
static int inspect(void){
 K32Work r=request(K32_WORK_REC_INFO,filename.text);r.data=(unsigned)&recovery;r.bytes=sizeof recovery;
 int e=k32_work(&r);if(e)return e;namecopy(recovery_base,filename.text);modal=2;kind=0;
 rescue=(KwInput){0};namecopy(rescue.text,"RESCUED.BMP");rescue.cursor=rescue.length=k32_len(rescue.text);return 0;
}
static int select_candidate(unsigned selected){
 K32WorkRevision *v=&recovery.file[selected];if(v->error||!v->exists)return v->error?v->error:-2;
 K32Work r=request(K32_WORK_REC_OPEN_TMP+selected-1,recovery_base);int h=k32_work(&r);if(h<0)return h;
 KgBmp next;int e=decode(h,&next,v);if(!e){kind=selected;modal=4;}return e;
}
static int restore(void){
 K32RecoveryRestore target={.expected=recovery};namecopy(target.destination,rescue.text);
 K32Work r=request(K32_WORK_REC_RESTORE_TMP+kind-1,recovery_base);r.data=(unsigned)&target;r.bytes=sizeof target;
 int e=k32_work(&r);if(!e){modal=0;message="Recovered to unused BMP / image kept";}return e;
}
static const char *error_text(int e){return e==-116?"File changed / original kept":e==-17?"TMP or destination exists / kept":e==-117?"Inspect TMP/BAK / original kept":e==-28?"Disk full / original kept":e==-13?"Protected file / original kept":"Invalid BMP or I/O error / original kept";}
static void hex(char out[9],unsigned v){for(unsigned i=0;i<8;i++)out[i]="0123456789ABCDEF"[v>>(28-i*4)&15];out[8]=0;}
static int dialog(void){
 unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 if(modal==1||modal==3){
  kg_text(8,8,modal==1?"Replace existing Work BMP?":"Unsaved image",0x263e58);
  kg_text(8,32,filename.text,0x263e58);
  kg_text(8,56,modal==1?"R: replace / Esc: cancel":"S: save / D: discard / Esc: cancel",0x263e58);
  kg_button(modal==1?'R':'S',8,h-64,88,28,modal==1?"Replace":"Save");
  if(modal==3)kg_button('D',104,h-64,88,28,"Discard");
  kg_button(27,w-88,h-64,80,28,"Cancel");
 }else {
  kg_text(8,4,"Work BMP recovery / original kept",0x263e58);
  kg_text(8,28,recovery_base,0x263e58);
  if(modal==2){
   const char *labels[]={"BMP","TMP","BAK"};
   for(unsigned i=0;i<3;i++){char value[9];hex(value,recovery.file[i].hash);kg_text(8,52+i*24,labels[i],0x263e58);kg_text(56,52+i*24,recovery.file[i].error?"Error":recovery.file[i].exists?value:"Absent",0x263e58);}
   kg_button('T',8,h-64,88,28,"Inspect TMP");kg_button('B',104,h-64,88,28,"Inspect BAK");
  }else {
   kg_text(8,52,kind==1?"Valid BMP in TMP":"Valid BMP in BAK",0x263e58);
   kw_draw_input((KwRect){8,76,(int)w-24,28},&rescue,1);
   kg_button('R',8,h-64,96,28,"Restore");
   kg_text(8,h-88,"Only move to an unused BMP name",0x263e58);
  }
  kg_button(27,w-88,h-64,80,28,"Cancel");
 }
 if(message)kw_text(8,h-24,message,(w-16)/8,0x263e58);
 stats();int e=kg_present();if(!e)image_stats.frames++;return e;
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
 if(modal)return dialog();
 stats();unsigned w=kg_width(),h=kg_height();kg_begin();kg_rect(0,0,w,h,0xeaf0f5);
 kw_draw_input((KwRect){8,4,160,28},&filename,focused);kg_button('o',176,4,72,28,"Load");kg_button('q',w-80,4,72,28,"Close");
 kg_button('i',8,40,72,28,"Invert");kg_button('h',88,40,72,28,"Flip H");kg_button('u',168,40,72,28,"Undo");kg_button('p',248,40,80,28,paintmode?"Painting":"Paint");
 kg_rect(8,80,w-16,h-116,0x182c46);
 if(handle&&image.width){
  int x=16+panx,y=88+pany,sx=x<8?8-x:0,sy=y<80?80-y:0;x+=sx;y+=sy;
  int width=(int)image.width-sx,height=(int)image.height-sy;
  if(x+width>(int)w-8)width=(int)w-8-x;
  if(y+height>(int)h-36)height=(int)h-36-y;
  if(width>0&&height>0)kg_bitmap(handle,x,y,sx,sy,width,height);
 }
 kw_text(8,h-28,message?message:paintmode?"Paint / U:undo":image_stats.dirty?"Modified / Ctrl S:save":"Ctrl W:Root/Work Ctrl R:Recovery",(w-96)/8,0x263e58);
 kg_button(19,w-80,h-32,72,24,"Save");
 int e=kg_present();if(!e)image_stats.frames++;return e;
}
int main(int argc,char **argv){
 int token=kg_open("Images / BMP24",328,196);if(token<0){image_stats.error=-token;return 1;}
 kg_subscribe(KG_INPUT_TAB|KG_INPUT_KEYS|KG_INPUT_MOTION);
 unsigned fault=argc>1&&equal(argv[1],"fault");
 work=argc>2&&equal(argv[1],"work");
 const char *name=fault?"SAMPLE.BMP":argc>2&&(equal(argv[1],"root")||work)?argv[2]:argc>1?argv[1]:"SAMPLE.BMP";
 for(unsigned i=0;name[i]&&i<12;i++)filename.text[i]=name[i];
 filename.length=k32_len(filename.text);filename.cursor=filename.length;
 int e=k32_len(name)>12?-22:load();if(e<0){image_stats.error=-e;message=error_text(e);}
 if(draw()<0)return 2;
 if(fault)*(volatile unsigned *)0x10000=1;
 for(;;){
  KgEvent v;int got=kg_poll(&v);if(got<0)return 3;image_stats.heartbeat++;unsigned key=0,dirty=0;e=0;
  if(got){image_stats.events++;
   if(modal){
    unsigned action=v.type==KG_KEY?v.key:v.type==KG_MOUSE&&(v.key&1)?v.control:0;
    unsigned old=modal;
    if(action==27){modal=pending=0;message=0;}
    else if(old==1&&(action=='R'||action=='r')){e=save(1);if(!e){modal=0;key=pending;pending=~0u;}}
    else if(old==3&&(action=='D'||action=='d')){modal=0;key=pending;pending=~0u;}
    else if(old==3&&(action=='S'||action=='s')){e=save(0);if(!e){modal=0;key=pending;pending=~0u;}}
    else if(old==2&&(action=='T'||action=='t'||action=='B'||action=='b'))e=select_candidate(action=='T'||action=='t'?1:2);
    else if(old==4&&(action=='R'||action=='r')&&v.type==KG_MOUSE)e=restore();
    else if(old==4&&action==13)e=restore();
    else if(old==4)kw_input(&rescue,&v);
    if(e<0){image_stats.error=-e;message=error_text(e);pending=0;}else if(action)image_stats.error=0;
    if(!key){if(draw()<0)return 4;k32_wait(K32_WAIT_TIME,50);continue;}
   }
   else {
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
    if(v.key==19||v.key==23||v.key==18){key=v.key;focused=0;}
    else if(v.key==9){focused^=1;dirty=1;}
    else if(focused&&v.key==13)key='o';
    else if(focused){if(kw_input(&filename,&v)){dirty=1;message=0;}}
    else if(v.key>=256&&v.key<=259){panx+=v.key==258?-8:v.key==259?8:0;pany+=v.key==256?-8:v.key==257?8:0;if(panx < -256)panx=-256;if(panx>256)panx=256;if(pany < -192)pany=-192;if(pany>192)pany=192;dirty=1;}
    else key=v.key;
   }
   }
  }
  if((key=='q'||key=='Q'||key=='o'||key=='O')&&image_stats.dirty&&pending!=~0u){pending=key;modal=3;message=0;draw();continue;}
  if(key=='q'||key=='Q')break;
  if(pending==~0u)pending=0;
  if(key==19||key=='s'||key=='S'){e=save(0);dirty=1;}
  else if(key==23){work^=1;message=work?"Work / enter BMP name then Load":"A: root / read only";dirty=1;}
  else if(key==18){e=inspect();dirty=1;}
  else if(key=='o'||key=='O'){e=load();focused=0;dirty=1;}
  else if(key=='p'||key=='P'){paintmode^=1;painting=0;dirty=1;}
  else if(key=='i'||key=='h'||key=='u'||key=='r'){e=change(key);dirty=1;}
  if(dirty){if(e<0){image_stats.error=-e;message=error_text(e);}else if(key){image_stats.error=0;if(key!=19&&key!=23&&key!=18&&key!='s'&&key!='S')message=0;}e=draw();if(e<0){image_stats.error=-e;return 4;}}
  k32_wait(K32_WAIT_TIME,50);
 }
 if(handle)kg_surface_destroy(handle);
 return kg_close()?5:0;
}
