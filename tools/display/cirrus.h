#ifndef KDESK_CIRRUS_H
#define KDESK_CIRRUS_H
#include "../filer/filer.h"
unsigned cirrus_identity(void);
int cirrus_open(unsigned width,unsigned height);
void cirrus_close(void);
void cirrus_read(u32 offset,unsigned segment,unsigned offset16,unsigned count);
void cirrus_fill(u32 offset,unsigned count,u16 color);
int cirrus_blit(u32 source,u32 destination,unsigned widthBytes,unsigned heightRows,unsigned pitch);
u16 cirrus_pixel(u32 offset);
void cirrus_write(u32 offset,unsigned segment,unsigned offset16,unsigned count);
unsigned cirrus_width(void);
unsigned cirrus_height(void);
#endif
