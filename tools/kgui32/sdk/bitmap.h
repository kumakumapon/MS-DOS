#ifndef KG_BITMAP_BUFFER_H
#define KG_BITMAP_BUFFER_H
#include "protocol.h"
#define KG_BITMAP_WIDTH 384u
#define KG_BITMAP_HEIGHT 256u
#define KG_BITMAP_PIXELS (KG_BITMAP_WIDTH*KG_BITMAP_HEIGHT)
#define KG_BITMAP_FILE_BYTES (4096u+KG_BITMAP_PIXELS*4u)
typedef struct {u32 width,height,stride,offset,bytes,topdown,bits,palette;} KgBmp;
int kg_bmp_header(const u8 *,unsigned,u32,KgBmp *);
int kg_bmp_encode(const u16 *,unsigned,unsigned,u8 *,unsigned);
int kg_bmp_row(const KgBmp *,unsigned,const u8 *,unsigned,u16 *,unsigned);
int kg_bmp_decode(const u8 *,unsigned,KgBmp *,u16 *,unsigned);
int kg_bitmap_fill(u16 *,unsigned,unsigned,int,int,int,int,u16);
int kg_bitmap_line(u16 *,unsigned,unsigned,int,int,int,int,u16);
void kg_bitmap_invert(u16 *,unsigned);
void kg_bitmap_flip(u16 *,unsigned,unsigned);
#endif
