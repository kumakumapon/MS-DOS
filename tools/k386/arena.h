#ifndef K386_ARENA_H
#define K386_ARENA_H
#include "abi.h"
enum { K386_BLOCKS=32 };
typedef struct {k16 segment,size,owner;} K386Block;
typedef struct {k16 count;K386Block block[K386_BLOCKS];} K386Arena;
void arena_init(K386Arena *a);
k16 arena_largest(const K386Arena *a);
k16 arena_alloc(K386Arena *a,k16 size,k16 owner,k16 preferred);
int arena_free(K386Arena *a,k16 segment,k16 owner);
int arena_resize(K386Arena *a,k16 segment,k16 size,k16 owner,k16 *available);
void arena_release(K386Arena *a,k16 owner);
#endif
