#ifndef KX32_POLICY_H
#define KX32_POLICY_H
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define KX32_BLOCKS 64u
#define KX32_HANDLE 0xff00u
#define KX32_BASE 0x1000000u
typedef struct {u16 first,size;u8 locks,used;} KxBlock;
typedef struct {KxBlock block[KX32_BLOCKS];u16 size;} KxPool;
void kx_space(const KxPool *,u32 *,u32 *);
int kx_index(const KxPool *,unsigned);
int kx_alloc(KxPool *,unsigned);
int kx_free(KxPool *,unsigned);
int kx_resize(KxPool *,unsigned,unsigned);
int kx_range(const KxPool *,unsigned,u32,u32,u32 *);
#endif
