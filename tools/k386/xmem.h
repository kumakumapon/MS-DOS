#ifndef K386_XMEM_H
#define K386_XMEM_H
#include "abi.h"
enum { KX_QUERY, KX_ALLOC, KX_FREE, KX_READ, KX_WRITE, KX_USAGE };
enum { KX_VERSION=0x100, KX_QUOTA_KB=4096, KX_BLOCKS=8,
       KX_CHUNK=4096, KX_REQUEST=0x389, KX_ZERO_SLICE=16384 };
typedef struct {k32 bytes,initialized;k16 owner,handle,token,reserved;} KXBlock;
typedef struct {
    char signature[8];k16 version,size;k32 serial;
    k16 pending[K386_SESSIONS];KXBlock blocks[K386_SESSIONS][KX_BLOCKS];
} KXMemory;
extern KXMemory k386_xmem;
/* Returns 1 only while a newly allocated block is being initialized. */
int xmem_service(unsigned id,k16 owner,K386Frame *f,k8 *buffer);
void xmem_release(unsigned id,k16 owner); /* owner=0 releases the session */
unsigned xmem_used(unsigned id,k16 owner,unsigned *count);
/* Real-mode driver/guest adapters, replaced by deterministic host tests. */
int xmem_new(k16 kb,k16 *handle);
int xmem_free(k16 handle);
int xmem_read(k16 handle,k32 at,void *out,unsigned count);
int xmem_write(k16 handle,k32 at,const void *in,unsigned count);
int xmem_guest(unsigned id,k32 at,void *buffer,unsigned count,int write);
#endif
