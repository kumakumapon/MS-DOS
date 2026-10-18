#include "arena.h"
#include <assert.h>
static void chain(K386Arena *a){assert(a->block[0].segment==0x1000);for(unsigned i=1;i<a->count;i++)assert(a->block[i].segment==a->block[i-1].segment+a->block[i-1].size+1);assert(a->block[a->count-1].segment+a->block[a->count-1].size==0xa000);}
int main(void){K386Arena a,b;arena_init(&a);arena_init(&b);assert(arena_largest(&a)==0x9000);
 assert(arena_alloc(&a,0x1000,0x8000,0x8000)==0x8000);chain(&a);assert(arena_largest(&a)==0x6fff);
 k16 env=arena_alloc(&a,16,0x8000,0),p=arena_alloc(&a,0x1000,0x1011,0);assert(env==0x1000&&p==0x1011);chain(&a);
 assert(arena_free(&a,p,0x8000)==9);k16 max;assert(arena_resize(&a,p,0x8000,0x1011,&max)==8&&max<0x8000);
 assert(!arena_resize(&a,p,0x2000,0x1011,&max));chain(&a);assert(!arena_resize(&a,p,0x100,0x1011,&max));chain(&a);
 arena_release(&a,0x1011);chain(&a);assert(!arena_free(&a,env,0x8000));assert(!arena_free(&a,0x8000,0x8000));chain(&a);assert(a.count==1&&arena_largest(&a)==0x9000);
 assert(b.count==1&&arena_largest(&b)==0x9000);assert(!arena_alloc(&a,0xffff,1,0));
 for(unsigned i=0;i<K386_BLOCKS;i++){arena_alloc(&a,1,1,0);}chain(&a);arena_release(&a,1);assert(a.count==1);return 0;}
