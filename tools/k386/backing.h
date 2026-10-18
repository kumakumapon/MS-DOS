#ifndef K386_BACKING_H
#define K386_BACKING_H
#include "abi.h"
typedef struct { k32 physical,offset; k16 handle; } K386Backing;
enum { K386_RAM=0xa0000, K386_BACKING=0xa5000 };
int backing_init(void);
int backing_a20(int on);
int backing_new(K386Backing *b);
void backing_free(K386Backing *b);
int backing_read(const K386Backing *b,k32 at,void *out,unsigned count);
int backing_write(const K386Backing *b,k32 at,const void *in,unsigned count);
int backing_copy(const K386Backing *b,k32 to,k32 from,unsigned count);
int backing_zero(const K386Backing *b,k32 at,k32 count);
#endif
