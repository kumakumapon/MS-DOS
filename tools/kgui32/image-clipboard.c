/* A pair of checked buffers in the GUI's private 32-bit allocation. */
#include "gui.h"
#include "../k38632/services.h"
#include "sdk/protocol.h"
#include "../k38632/io.h"

extern int gui_copy(u32,void *,unsigned,int);
extern u8 loader_map[1024];

static u32 storage,serial,writer,writing_generation,document_serial;
static unsigned front,width,height,next_width,next_height,offset,total;

int kg_image_clip_register(u32 base,unsigned bytes){
 if(bytes!=sizeof(KgDesktopShared)||!k32_user_access(loader_map,base,bytes,1))return -14;
 storage=base;serial=writer=offset=total=document_serial=0;
 return 0;
}

void kg_image_clip_drop(unsigned owner,u32 generation){
 if(writer==owner&&writing_generation==generation)writer=0;
}

int kg_image_clip_request(KgRequest *r,unsigned owner,u32 generation){
 if(!storage)return -38;
 if(r->operation==KG_RECENT_DOCUMENT){
  if(r->width>26||r->height>1)return -22;
  const char *path=(const char *)r->draw;
  KgRecentDocument doc={.volume=r->width,.bitmap=r->height};
  unsigned n=0;
  while(n<KG_RECENT_PATH-1&&path[n]){
   unsigned char ch=path[n];
   if(ch<=32||ch>=127||ch=='"'||ch=='/'||ch==':'||ch=='='||ch=='>'||ch=='<'||ch=='|'||ch=='&')return -22;
   doc.path[n++]=ch;
  }
  if(n<5||path[n])return -22;
  doc.serial=++document_serial;
  u32 slot=(doc.serial-1)%KG_RECENT_COUNT;
  return gui_copy(storage+__builtin_offsetof(KgDesktopShared,documents)+slot*sizeof doc,&doc,sizeof doc,1);
 }
 if(r->operation==KG_IMAGE_COPY_BEGIN){
  if(!r->width||!r->height||r->width>KG_CLIP_IMAGE_SIDE||r->height>KG_CLIP_IMAGE_SIDE)return -22;
  if(writer&&writer!=owner)return -16;
  writer=owner;writing_generation=generation;next_width=r->width;next_height=r->height;
  offset=0;total=2*r->width*r->height;
  return 0;
 }
 if(r->operation==KG_IMAGE_PASTE_INFO){
  if(!serial)return -2;
  r->width=width;r->height=height;r->count=serial;
  return 0;
 }
 unsigned n=r->count,at=r->width;
 if(r->operation==KG_IMAGE_COPY_DATA){
  if(writer!=owner||writing_generation!=generation)return -116;
  if(!n||n>sizeof r->draw||(n&1)||at!=offset||n>total-offset){writer=0;return -22;}
  int e=gui_copy(storage+(front^1)*KG_CLIP_IMAGE_BYTES+offset,r->draw,n,1);
  if(e){writer=0;return e;}
  offset+=n;
  if(offset==total){
   front^=1;width=next_width;height=next_height;
   if(!++serial)serial=1;
   writer=0;
  }
  return 0;
 }
 unsigned bytes=2*width*height;
 if(r->height!=serial||!serial)return -116;
 if(!n||n>sizeof r->draw||((n|at)&1)||at>bytes||n>bytes-at)return -22;
 return gui_copy(storage+front*KG_CLIP_IMAGE_BYTES+at,r->draw,n,0);
}
