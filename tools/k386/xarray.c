#include "xarray.h"
#ifdef XARRAY_KAPP
#include "sdk.h"
static unsigned kb_alloc(unsigned kib,unsigned *token){return kapp_xms_alloc(kib,token);}
static unsigned kb_free(unsigned token){return kapp_xms_free(token);}
static unsigned kb_read(unsigned token,unsigned long at,void *out,unsigned n){return kapp_xms_read(token,(unsigned)at,out,n);}
static unsigned kb_write(unsigned token,unsigned long at,const void *in,unsigned n){return kapp_xms_write(token,(unsigned)at,in,n);}
const XArrayBackend xarray_kapp_backend={kb_alloc,kb_free,kb_read,kb_write};
#endif
static unsigned page_length(const XArray *a,unsigned long page){
    unsigned long start=page*XARRAY_PAGE,left=a->bytes-start;
    return left<XARRAY_PAGE?(unsigned)left:XARRAY_PAGE;
}
static unsigned write_back(XArray *a,XArrayLine *l){
    unsigned long page=l->page;
    unsigned error=a->backend->write(a->token,page*XARRAY_PAGE,a->cache+(unsigned)(l-a->line)*XARRAY_PAGE,page_length(a,page));
    if(error)return error;
    l->dirty=0;a->stats.writebacks++;return 0;
}
/* Resident line for a page: hit, or evict the least recently used line. */
static unsigned fetch(XArray *a,unsigned long page,XArrayLine **found){
    XArrayLine *victim=0;
    for(unsigned i=0;i<a->lines;i++){
        XArrayLine *l=&a->line[i];
        if(l->valid&&l->page==page){l->stamp=++a->clock;a->stats.hits++;*found=l;return 0;}
        if(!victim||(victim->valid&&(!l->valid||l->stamp<victim->stamp)))victim=l;
    }
    if(victim->valid&&victim->dirty){unsigned error=write_back(a,victim);if(error)return error;}
    victim->valid=0;
    unsigned error=a->backend->read(a->token,page*XARRAY_PAGE,a->cache+(unsigned)(victim-a->line)*XARRAY_PAGE,page_length(a,page));
    if(error)return error;
    victim->page=page;victim->valid=1;victim->dirty=0;victim->stamp=++a->clock;
    a->stats.misses++;*found=victim;return 0;
}
static int in_range(const XArray *a,unsigned long offset,unsigned long count){
    return offset<=a->bytes&&count<=a->bytes-offset;
}
unsigned xarray_open(XArray *a,const XArrayBackend *backend,unsigned long bytes,void *cache,unsigned lines){
    if(!a||!backend||!cache||!bytes||bytes>XARRAY_MAX_BYTES||!lines||lines>XARRAY_MAX_PAGES)return 87;
    unsigned token=0,error=backend->alloc((unsigned)((bytes+1023)/1024),&token);
    if(error)return error;
    a->backend=backend;a->token=token;a->bytes=bytes;a->clock=0;a->cache=cache;a->lines=lines;
    a->stats.hits=a->stats.misses=a->stats.writebacks=0;
    for(unsigned i=0;i<XARRAY_MAX_PAGES;i++){a->line[i].valid=a->line[i].dirty=0;a->line[i].page=a->line[i].stamp=0;}
    return 0;
}
unsigned xarray_flush(XArray *a){
    unsigned first=0;
    for(unsigned i=0;i<a->lines;i++)
        if(a->line[i].valid&&a->line[i].dirty){unsigned error=write_back(a,&a->line[i]);if(error&&!first)first=error;}
    return first;
}
unsigned xarray_close(XArray *a){
    unsigned error=xarray_flush(a),freed=a->backend->release(a->token);
    a->token=0;a->bytes=0;a->lines=0;
    return error?error:freed;
}
static unsigned transfer(XArray *a,unsigned long offset,unsigned char *buffer,unsigned count,int write){
    if(!in_range(a,offset,count))return 87;
    while(count){
        XArrayLine *l;unsigned error=fetch(a,offset/XARRAY_PAGE,&l);
        if(error)return error;
        unsigned inside=(unsigned)(offset%XARRAY_PAGE),n=XARRAY_PAGE-inside;
        if(n>count)n=count;
        unsigned char *page=a->cache+(unsigned)(l-a->line)*XARRAY_PAGE+inside;
        for(unsigned i=0;i<n;i++){if(write)page[i]=buffer[i];else buffer[i]=page[i];}
        if(write)l->dirty=1;
        offset+=n;buffer+=n;count-=n;
    }
    return 0;
}
unsigned xarray_read(XArray *a,unsigned long offset,void *out,unsigned count){return transfer(a,offset,out,count,0);}
unsigned xarray_write(XArray *a,unsigned long offset,const void *in,unsigned count){return transfer(a,offset,(unsigned char *)in,count,1);}
unsigned xarray_page(XArray *a,unsigned long offset,int will_write,unsigned char **page,unsigned *avail){
    if(offset>=a->bytes)return 87;
    XArrayLine *l;unsigned error=fetch(a,offset/XARRAY_PAGE,&l);
    if(error)return error;
    unsigned inside=(unsigned)(offset%XARRAY_PAGE);
    if(will_write)l->dirty=1;
    *page=a->cache+(unsigned)(l-a->line)*XARRAY_PAGE+inside;
    *avail=page_length(a,offset/XARRAY_PAGE)-inside;
    return 0;
}
void xarray_stats(const XArray *a,XArrayStats *stats){*stats=a->stats;}
