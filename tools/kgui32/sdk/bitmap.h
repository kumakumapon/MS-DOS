#ifndef KG_BITMAP_BUFFER_H
#define KG_BITMAP_BUFFER_H
#include "protocol.h"
typedef struct {u32 width,height,stride,offset,bytes,topdown;} KgBmp;
int kg_bmp_header(const u8 *,unsigned,u32,KgBmp *);
int kg_bmp_encode(const u16 *,unsigned,unsigned,u8 *,unsigned);
int kg_bmp_row(const KgBmp *,unsigned,const u8 *,unsigned,u16 *,unsigned);
int kg_bitmap_fill(u16 *,unsigned,unsigned,int,int,int,int,u16);
int kg_bitmap_line(u16 *,unsigned,unsigned,int,int,int,int,u16);
void kg_bitmap_invert(u16 *,unsigned);
void kg_bitmap_flip(u16 *,unsigned,unsigned);
#endif
