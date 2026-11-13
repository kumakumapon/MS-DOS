#include <assert.h>
#include <string.h>
#include "client-policy.h"
static u16 data[KG_SURFACE_PIXELS*2];static KgRequest r;static KgClient c;static u32 serial=0;static KgImage image;
static int op(unsigned operation){r.operation=operation;return kg_policy(&c,&r,7,&serial,640,400);}
static void receive(unsigned offset){r.generation=7;r.revision=c.surface.pending;kg_surface_set(&r,0,c.surface.handle);kg_surface_set(&r,4,offset);kg_surface_set(&r,8,c.surface.width);kg_surface_set(&r,12,c.surface.height);assert(!kg_surface_receive(&image,data,&r,7,r.token));}
static void ack(void){assert(!kg_surface_ack(&c.surface,c.surface.pending,r.operation));}
int main(void){
 r=(KgRequest){.version=1,.size=sizeof r,.width=344,.height=196};strcpy(r.title,"Test");assert(op(KG_OPEN)==1);r.token=1;r.width=3;r.height=2;
 assert(op(KG_SURFACE_CREATE)==2&&c.surface.pending);receive(0);assert(!image.valid);KgSurface old=c.surface;assert(kg_surface_ack(&c.surface,c.surface.pending+1,r.operation)==-116&&!memcmp(&old,&c.surface,sizeof old));ack();
 r.width=2;r.height=0;r.count=14;old=c.surface;assert(op(KG_SURFACE_UPLOAD)==-22&&!memcmp(&old,&c.surface,sizeof old));
 r.count=4;((u16 *)r.draw)[0]=0xf800;((u16 *)r.draw)[1]=0x7e0;assert(!op(KG_SURFACE_UPLOAD));receive(0);assert(!image.valid);assert(op(KG_SURFACE_UPLOAD)==-11);ack();
 r.height=4;r.count=8;for(unsigned i=0;i<4;i++)((u16 *)r.draw)[i]=0x1f+i;assert(!op(KG_SURFACE_UPLOAD));receive(4);ack();assert(image.valid&&image.front==1&&data[KG_SURFACE_PIXELS]==0xf800);
 r.width=2;r.height=0;r.count=4;((u16 *)r.draw)[0]=0x7ff;((u16 *)r.draw)[1]=0xf81f;assert(!op(KG_SURFACE_UPLOAD));receive(0);assert(image.valid&&image.front==1&&data[KG_SURFACE_PIXELS]==0xf800);ack();
 r.height=4;r.count=8;assert(!op(KG_SURFACE_UPLOAD));receive(4);ack();assert(image.front==0&&data[0]==0x7ff);
 r.count=1;r.draw[0]=(KgDraw){.op=KG_BITMAP,.x=-2,.y=-1,.w=3,.h=2,.id=2};assert(!op(KG_FRAME));
 old=c.surface;r.draw[0].id=99;assert(op(KG_FRAME)==-116&&!memcmp(&old,&c.surface,sizeof old));r.draw[0].id=2;r.draw[0].color=3;assert(op(KG_FRAME)==-22);r.draw[0].color=0;
 r.token=99;assert(op(KG_FRAME)==-116);r.token=1;r.width=2;assert(!op(KG_SURFACE_DESTROY));receive(0);ack();assert(!c.surface.width&&!image.valid);r.count=1;assert(op(KG_FRAME)==-116);
 assert(!op(KG_CLOSE));assert(!c.surface.handle&&!c.token);assert(op(KG_SURFACE_STATUS)==-116);
 return 0;
}
