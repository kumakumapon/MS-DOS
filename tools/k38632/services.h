#ifndef K32_SERVICES_H
#define K32_SERVICES_H
#include "elf.h"
#ifdef K32_GUI
#define K32_FEATURES 31
#else
#define K32_FEATURES 15
#endif
enum {K32_INFO=0xbfd000,K32_ALLOCS=64,K32_FILES=8};
typedef struct {u32 argc;char arg[8][64];u32 budget;} K32Args;
typedef struct {u32 first,pages;} K32Allocation;
typedef struct {u32 base,limit;K32Allocation block[K32_ALLOCS];} K32Heap;
int k32_arguments(const u8 *tail,unsigned n,K32Args *out);
int k32_filename(const char *in,unsigned n,char out[16]);
void k32_heap_init(K32Heap *h,u32 base,u32 limit);
int k32_heap_find(K32Heap *h,u32 bytes,unsigned *slot,u32 *address,u32 *pages);
int k32_heap_release(K32Heap *h,u32 address,unsigned *slot);
int k32_user_range(const u32 *map,u32 at,u32 n,int write);
#endif
