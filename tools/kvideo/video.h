/* Private PC-98 display model. No host display pointers, I/O or DOS calls. */
#ifndef KV98_VIDEO_H
#define KV98_VIDEO_H
#define KV_IO_REQUEST 0x38eu
#define KV_BACKING_BYTES 0xe8000u
#define KV_FONT_BASE 0xc0000u
#define KV_FONT_BYTES (188u*32u)
#define KV_TEXT 1u
#define KV_GRAPH 2u
#define KV_SEMI 4u
#define KV_ANALOG 8u
#define KV_CURSOR 16u
#define KV_GAIJI 32u
#define KV_CHANGED 1u
#define KV_CURSOR_CHANGED 2u
#define KV_MODE_CHANGED 4u
typedef struct {
 unsigned version,size,flags,columns,rows,width,height,pitch,start;
 unsigned text_pitch,text_start,text_cursor;
 unsigned char palette[16][3],valid[24];
 unsigned io_count,font_writes;
} KvMeta;
_Static_assert(sizeof(KvMeta)==128,"private display metadata");
typedef struct {
 unsigned cursor,count;unsigned short mask,pitch;
 unsigned char command,used,need,phase,low,dot,raster,reply_at,reply_count;
 unsigned char parameters[16],pram[16],reply[5];
} KvGdc;
typedef struct {
 KvMeta meta;KvGdc gdc[2];
 unsigned char analog[16][3],digital[4],mode1,cg_high,cg_low,cg_line,palette;
} KvVideo;
typedef struct {
 void *context;
 int (*read)(void *,unsigned,void *,unsigned);
 int (*write)(void *,unsigned,const void *,unsigned);
} KvMemory;
typedef struct {
 unsigned version,size,slot,generation,operation,port,value,width,ticks;
 unsigned handle,offset;int result;
 KvMeta meta;
} KvPacket;
_Static_assert(sizeof(KvPacket)==176,"trusted monitor display packet");
#define KV_READ_BYTES 848u
typedef struct {KvPacket packet;unsigned char data[KV_READ_BYTES];} KvRead;
_Static_assert(sizeof(KvRead)==1024,"bounded private video read");
void kv_reset(KvVideo *);
int kv_out(KvVideo *,unsigned,unsigned,const KvMemory *);
int kv_in(KvVideo *,unsigned,unsigned,unsigned *,const KvMemory *);
unsigned kv_plane(unsigned);
int kv_region(unsigned,unsigned,unsigned,unsigned *);
unsigned kv_semigraph(unsigned,unsigned,unsigned);
#endif
