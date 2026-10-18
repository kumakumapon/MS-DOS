/* Trusted paragraph/ownership metadata; writable DOS MCBs are only a mirror. */
#include "arena.h"
void arena_init(K386Arena *a){*a=(K386Arena){1,{{0x1000,0x9000,0}}};}
k16 arena_largest(const K386Arena *a){k16 max=0;for(unsigned i=0;i<a->count;i++)if(!a->block[i].owner&&a->block[i].size>max)max=a->block[i].size;return max;}
static int insert(K386Arena *a,unsigned at,K386Block b){if(a->count==K386_BLOCKS)return 0;for(unsigned i=a->count;i>at;i--)a->block[i]=a->block[i-1];a->block[at]=b;a->count++;return 1;}
static void join(K386Arena *a){for(unsigned i=0;i+1<a->count;)if(!a->block[i].owner&&!a->block[i+1].owner){a->block[i].size+=1+a->block[i+1].size;for(unsigned j=i+1;j+1<a->count;j++)a->block[j]=a->block[j+1];a->count--;}else i++;}
k16 arena_alloc(K386Arena *a,k16 size,k16 owner,k16 preferred) {
    if(!size||!owner)return 0;
    for(unsigned i=0;i<a->count;i++){
        K386Block old=a->block[i];k32 end=(k32)old.segment+old.size;
        k16 start=preferred?preferred:old.segment;
        if(old.owner||start<old.segment||(k32)start+size>end)continue;
        unsigned extra=(start!=old.segment)+((k32)start+size<end);
        if(a->count+extra>K386_BLOCKS)return 0;
        if(start!=old.segment){a->block[i].size=start-old.segment-1;insert(a,++i,(K386Block){start,(k16)(end-start),0});}
        a->block[i]=(K386Block){start,size,owner};
        if((k32)start+size<end)insert(a,i+1,(K386Block){(k16)(start+size+1),(k16)(end-start-size-1),0});
        return start;
    }
    return 0;
}
int arena_free(K386Arena *a,k16 segment,k16 owner){for(unsigned i=0;i<a->count;i++)if(a->block[i].segment==segment&&a->block[i].owner==owner&&owner){a->block[i].owner=0;join(a);return 0;}return 9;}
int arena_resize(K386Arena *a,k16 segment,k16 size,k16 owner,k16 *available) {
    for(unsigned i=0;i<a->count;i++)if(a->block[i].segment==segment&&a->block[i].owner==owner&&owner){
        K386Block *b=&a->block[i];unsigned max=b->size;
        if(i+1<a->count&&!a->block[i+1].owner)max+=1+a->block[i+1].size;
        *available=max;
        if(size==b->size)return 0;
        if(size>max||(size<b->size&&a->count==K386_BLOCKS))return 8;
        if(size<b->size){k16 previous=b->size;b->size=size;insert(a,i+1,(K386Block){(k16)(segment+size+1),(k16)(previous-size-1),0});join(a);return 0;}
        if(size==max){b->size=size;for(unsigned j=i+1;j+1<a->count;j++)a->block[j]=a->block[j+1];a->count--;}
        else{b->size=size;a->block[i+1]=(K386Block){(k16)(segment+size+1),(k16)(max-size-1),0};}
        return 0;
    }
    *available=0;return 9;
}
void arena_release(K386Arena *a,k16 owner){for(unsigned i=0;i<a->count;i++)if(a->block[i].owner==owner)a->block[i].owner=0;join(a);}
