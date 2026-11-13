#ifndef KG_SURFACE_H
#define KG_SURFACE_H
#include "sdk/protocol.h"
#define KG_SURFACE_WIDTH 256u
#define KG_SURFACE_HEIGHT 192u
#define KG_SURFACE_PIXELS (KG_SURFACE_WIDTH*KG_SURFACE_HEIGHT)
#define KG_SURFACE_BYTES (KG_COMMANDS*sizeof(KgDraw))
typedef struct {u32 handle,width,height,uploaded,pending;} KgSurface;
typedef struct {u32 generation,token,handle,width,height,uploaded,revision,front,valid;} KgImage;
int kg_surface_policy(KgSurface *,KgRequest *,u32 *);
int kg_surface_ack(KgSurface *,u32,unsigned);
int kg_surface_draw(const KgSurface *,const KgDraw *);
int kg_surface_receive(KgImage *,u16 *,const KgRequest *,u32,u32);
int kg_surface_operation(unsigned);
static inline u32 kg_surface_word(const KgRequest *r,unsigned at){const u8 *p=(const u8 *)r->title+at;return p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
static inline void kg_surface_set(KgRequest *r,unsigned at,u32 v){for(unsigned i=0;i<4;i++)r->title[at+i]=v>>(i*8);}
#endif
