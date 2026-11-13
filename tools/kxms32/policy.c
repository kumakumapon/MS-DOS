/* Original bounded supplement policy. The old XMS allocator is never changed. */
#include "policy.h"
static unsigned next(const KxPool *p,unsigned at,unsigned *end){
 unsigned first=p->size;*end=p->size;
 for(unsigned i=0;i<KX32_BLOCKS;i++){const KxBlock *b=p->block+i;if(b->used&&b->size&&b->first>=at&&b->first<first){first=b->first;*end=first+b->size;}}
 return first;
}
void kx_space(const KxPool *p,u32 *total,u32 *largest){
 unsigned at=0;*total=*largest=0;
 while(at<p->size){unsigned end,first=next(p,at,&end),gap=first-at;*total+=gap;if(gap>*largest)*largest=gap;if(first==p->size)break;at=end;}
}
int kx_index(const KxPool *p,unsigned h){return h>=KX32_HANDLE&&h<KX32_HANDLE+KX32_BLOCKS&&p->block[h-KX32_HANDLE].used?(int)(h-KX32_HANDLE):-1;}
int kx_alloc(KxPool *p,unsigned n){
 if(n>p->size)return -0xa0;
 unsigned id=0;while(id<KX32_BLOCKS&&p->block[id].used)id++;if(id==KX32_BLOCKS)return -0xa1;
 unsigned at=0;for(;;){unsigned end,first=next(p,at,&end);if(n<=first-at){p->block[id]=(KxBlock){at,n,0,1};return KX32_HANDLE+id;}if(first==p->size)return -0xa0;at=end;}
}
int kx_free(KxPool *p,unsigned h){int i=kx_index(p,h);if(i<0)return -0xa2;if(p->block[i].locks)return -0xab;p->block[i]=(KxBlock){0};return 0;}
int kx_resize(KxPool *p,unsigned h,unsigned n){
 int i=kx_index(p,h);if(i<0)return -0xa2;KxBlock *b=p->block+i;if(b->locks)return -0xab;
 unsigned end=p->size;
 for(unsigned j=0;j<KX32_BLOCKS;j++)if(j!=(unsigned)i&&p->block[j].used&&p->block[j].size&&p->block[j].first>=b->first&&p->block[j].first<end)end=p->block[j].first;
 if(n>end-b->first)return -0xa0;
 b->size=n;return 0;
}
int kx_range(const KxPool *p,unsigned h,u32 offset,u32 bytes,u32 *address){
 int i=kx_index(p,h);if(i<0)return 0;u32 n=(u32)p->block[i].size*1024;
 if(offset>n||bytes>n-offset)return 0;
 *address=KX32_BASE+(u32)p->block[i].first*1024+offset;return 1;
}
