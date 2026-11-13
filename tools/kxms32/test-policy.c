#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "policy.h"
int main(void){
 KxPool p={.size=16384};u32 total,big,at;
 kx_space(&p,&total,&big);assert(total==16384&&big==16384);
 unsigned a=kx_alloc(&p,4100),b=kx_alloc(&p,4100),c=kx_alloc(&p,4100);assert(a>=KX32_HANDLE&&b!=a&&c!=b);
 assert(kx_range(&p,a,4096,4194304,&at)&&at==0x1001000);
 assert(!kx_range(&p,a,~0u,2,&at));assert(!kx_range(&p,a,4100*1024,1,&at));
 KxPool before=p;assert(kx_alloc(&p,4100)==-0xa0&&!memcmp(&p,&before,sizeof p));
 assert(kx_free(&p,b)==0);kx_space(&p,&total,&big);assert(total==8184&&big==4100);
 unsigned d=kx_alloc(&p,4100);assert(d==b&&p.block[d-KX32_HANDLE].first==4100);
 p.block[0].locks=1;before=p;assert(kx_free(&p,a)==-0xab&&kx_resize(&p,a,1)==-0xab&&!memcmp(&p,&before,sizeof p));p.block[0].locks=0;
 assert(kx_resize(&p,a,4200)==-0xa0);assert(!kx_resize(&p,a,1));
 assert(!kx_free(&p,a));assert(kx_free(&p,a)==-0xa2&&!kx_range(&p,a,0,0,&at));
 for(unsigned i=0;i<KX32_BLOCKS;i++)p.block[i]=(KxBlock){0};
 unsigned handles[KX32_BLOCKS];for(unsigned i=0;i<KX32_BLOCKS;i++){int h=kx_alloc(&p,1);assert(h>=0);handles[i]=h;for(unsigned j=0;j<i;j++)assert(p.block[h-KX32_HANDLE].first!=p.block[handles[j]-KX32_HANDLE].first);}
 before=p;assert(kx_alloc(&p,1)==-0xa1&&!memcmp(&p,&before,sizeof p));
 for(unsigned i=0;i<KX32_BLOCKS;i++)assert(!kx_free(&p,handles[i]));
 kx_space(&p,&total,&big);assert(total==16384&&big==16384);
 p.size=0;assert(kx_alloc(&p,1)==-0xa0);assert(kx_alloc(&p,0)>=0);assert(kx_index(&p,KX32_HANDLE-1)<0);
 puts("High XMS: fragmented pools, boundaries, locks, exhaustion, atomic failures and complete coalescing passed");return 0;
}
