/* Bounded surface ownership and transport. No DOS, physical pointers or I/O. */
#include "surface.h"
int kg_surface_operation(unsigned op){return op==KG_SURFACE_CREATE||op==KG_SURFACE_UPLOAD||op==KG_SURFACE_DESTROY;}
int kg_surface_policy(KgSurface *s,KgRequest *r,u32 *serial){
 if(r->operation==KG_SURFACE_CREATE){
  if(s->width||s->pending)return -16;
  if(!r->width||r->width>KG_SURFACE_WIDTH||!r->height||r->height>KG_SURFACE_HEIGHT)return -22;
  if(*serial>=0x7fffffffu)*serial=0;
  *s=(KgSurface){.handle=++*serial,.width=r->width,.height=r->height};return s->handle;
 }
 if(r->width!=s->handle||!s->handle)return -116;
 if(r->operation==KG_SURFACE_STATUS)return s->pending?-11:0;
 if(s->pending)return -11;
 if(!s->width)return -116;
 if(r->operation==KG_SURFACE_DESTROY)return 0;
 if(r->operation!=KG_SURFACE_UPLOAD)return -22;
 u32 n=s->width*s->height*2;
 unsigned offset=s->uploaded==n&&!r->height?0:s->uploaded;
 if(!r->count||r->count>KG_SURFACE_BYTES||(r->count&1)||r->height!=offset||r->height>n||r->count>n-r->height)return -22;
 s->uploaded=offset+r->count;return 0;
}
int kg_surface_draw(const KgSurface *s,const KgDraw *d){
 unsigned sx=d->color&65535,sy=d->color>>16;
 if(!s->width||d->id!=s->handle)return -116;
 if(s->pending||s->uploaded!=s->width*s->height*2)return -11;
 unsigned w=d->w,h=d->h;if(d->op==KG_BITMAP_SCALE){const u8 *p=(const u8 *)d->text;w=p[0]|(unsigned)p[1]<<8;h=p[2]|(unsigned)p[3]<<8;for(unsigned i=4;i<sizeof d->text;i++)if(p[i])return -22;}
 if(d->w<1||d->h<1||!w||!h||sx>=s->width||sy>=s->height||w>s->width-sx||h>s->height-sy)return -22;
 return 0;
}
int kg_surface_receive_n(KgImage *s,u16 *pixels,unsigned capacity,const KgRequest *r,u32 generation,u32 token){
 if(r->version!=1||r->size!=sizeof *r||r->generation!=generation||r->token!=token||!token||!r->revision)return -116;
 u32 handle=kg_surface_word(r,0),offset=kg_surface_word(r,4),width=kg_surface_word(r,8),height=kg_surface_word(r,12);
 if(r->operation==KG_SURFACE_CREATE){
  if(!handle||!width||width>KG_SURFACE_WIDTH||!height||height>KG_SURFACE_HEIGHT||width*height>capacity)return -22;
  *s=(KgImage){.generation=generation,.token=token,.handle=handle,.width=width,.height=height,.revision=r->revision};
  for(unsigned i=0;i<capacity*2;i++)pixels[i]=0;
  return 0;
 }
 if(s->generation!=generation||s->token!=token||s->handle!=handle)return -116;
 if(r->revision==s->revision)return 0;
 if(s->width!=width||s->height!=height)return -116;
 if(r->operation==KG_SURFACE_DESTROY){s->width=s->height=s->uploaded=s->valid=0;s->revision=r->revision;return 0;}
 if(r->operation!=KG_SURFACE_UPLOAD)return -22;
 u32 bytes=width*height*2;
 unsigned previous=s->uploaded==bytes&&!offset?0:s->uploaded;
 if(!r->count||r->count>KG_SURFACE_BYTES||(r->count&1)||offset!=previous||offset>bytes||r->count>bytes-offset)return -22;
 const u8 *p=(const u8 *)r->draw;for(unsigned i=0;i<r->count;i++)((u8 *)(pixels+(1-s->front)*capacity))[offset+i]=p[i];
 s->uploaded=previous+r->count;if(s->uploaded==bytes){s->front^=1;s->valid=1;}s->revision=r->revision;return 0;
}
int kg_surface_receive(KgImage *s,u16 *pixels,const KgRequest *r,u32 generation,u32 token){return kg_surface_receive_n(s,pixels,KG_SURFACE_PIXELS,r,generation,token);}

int kg_surface_ack(KgSurface *s,u32 revision,unsigned operation){
 if(!revision||s->pending!=revision)return -116;
 if(!kg_surface_operation(operation))return -22;
 if(operation==KG_SURFACE_DESTROY)s->width=s->height=s->uploaded=0;
 s->pending=0;return 0;
}
