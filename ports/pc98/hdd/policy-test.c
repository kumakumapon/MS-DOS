#include "policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u8 part[512],boot[1024];
static void w(unsigned at,unsigned n){boot[at]=n;boot[at+1]=n>>8;}
int main(void){
 part[0]=0xa1;part[1]=0x91;part[6]=part[10]=1;part[14]=614&255;part[15]=614>>8;
 memcpy(part+16,"KHDPROOF        ",16);memcpy(boot+3,"KHDPRF01",8);
 w(11,1024);boot[13]=4;w(14,1);boot[16]=2;w(17,512);w(19,40524);boot[21]=0xf8;
 w(22,20);w(24,33);w(26,8);w(28,66);w(510,0xaa55);
 memcpy(boot+43,"KHDPROOF   ",11);memcpy(boot+54,"FAT16   ",8);
 KhdPlan plan;assert(!khd_validate(part,boot,&plan));assert(plan.clusters==10116&&plan.start==264&&plan.sectors==40524);
 const unsigned fields[]={3,11,13,14,16,17,19,21,22,24,26,28,32,43,54,510};
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){boot[fields[i]]^=1;assert(khd_validate(part,boot,&plan));boot[fields[i]]^=1;}
 const unsigned parts[]={0,1,4,5,6,8,9,10,12,13,14,16,32,511};
 for(unsigned i=0;i<sizeof parts/sizeof *parts;i++){part[parts[i]]^=1;assert(khd_validate(part,boot,&plan));part[parts[i]]^=1;}
 assert(!khd_range(0,1,0x500));assert(!khd_range(40523,1,0x9fc00));assert(!khd_range(40524,0,~0u));
 assert(khd_range(40524,1,0x500));assert(khd_range(40523,2,0x500));assert(khd_range(~0u,1,0x500));
 assert(khd_range(0,~0u,0x500));assert(khd_range(0,1,0x4ff));assert(khd_range(0,1,0xa0000));assert(khd_range(0,1,0x9fc01));
 assert(!khd_range(1,2,0x1ff80));assert(khd_range(0,40524,0x500));
 puts("HDD policy: marked geometry, BPB, range, overflow and 64K crossing PASS");
}
