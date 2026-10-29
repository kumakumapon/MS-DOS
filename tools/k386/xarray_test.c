#include "xarray.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* RAM backend: counts transfers and can inject failures. */
static unsigned char *block;static unsigned long block_bytes;static unsigned live,next_token=1,reads,writes,fail_alloc,fail_read,fail_write,fail_release;
static unsigned long last_write_page;
static unsigned b_alloc(unsigned kib,unsigned *t){if(fail_alloc)return 8;assert(!live);block_bytes=kib*1024UL;block=calloc(1,block_bytes);assert(block);*t=next_token++;live=*t;return 0;}
static unsigned b_release(unsigned t){if(!t||t!=live)return 6;if(fail_release)return 8;free(block);block=0;live=0;return 0;}
static unsigned b_read(unsigned t,unsigned long at,void *out,unsigned n){assert(t==live&&n<=XARRAY_PAGE&&at+n<=block_bytes);reads++;if(fail_read)return 8;memcpy(out,block+at,n);return 0;}
static unsigned b_write(unsigned t,unsigned long at,const void *in,unsigned n){assert(t==live&&n<=XARRAY_PAGE&&at+n<=block_bytes);writes++;last_write_page=at/XARRAY_PAGE;if(fail_write)return 8;memcpy(block+at,in,n);return 0;}
static const XArrayBackend ram={b_alloc,b_release,b_read,b_write};
static unsigned char cache[XARRAY_MAX_PAGES*XARRAY_PAGE];
static void reset(void){reads=writes=fail_alloc=fail_read=fail_write=fail_release=0;}
static unsigned char pat(unsigned long i){return (unsigned char)(i*131+(i>>8)*7+3);}
static void fill(XArray *a,unsigned long n){unsigned char b[1];for(unsigned long i=0;i<n;i++){b[0]=pat(i);assert(!xarray_write(a,i,b,1));}}
int main(void){
    XArray a;XArrayStats s;
    /* Argument validation and allocation failure. */
    assert(xarray_open(&a,&ram,0,cache,4)==87);
    assert(xarray_open(&a,&ram,XARRAY_MAX_BYTES+1,cache,4)==87);
    assert(xarray_open(&a,&ram,100,cache,0)==87);
    assert(xarray_open(&a,&ram,100,cache,XARRAY_MAX_PAGES+1)==87);
    assert(xarray_open(&a,&ram,100,0,2)==87&&!live);
    fail_alloc=1;assert(xarray_open(&a,&ram,100,cache,2)==8&&!live);reset();

    /* Size not a multiple of the page: the block is rounded to whole KiB, transfers stop at `bytes`. */
    assert(!xarray_open(&a,&ram,3*XARRAY_PAGE+5,cache,2)&&block_bytes==13*1024UL);
    unsigned char tmp[16];
    assert(xarray_read(&a,3*XARRAY_PAGE+5,tmp,0)==0);          /* empty range at the end */
    assert(xarray_read(&a,3*XARRAY_PAGE+5,tmp,1)==87);
    assert(xarray_read(&a,3*XARRAY_PAGE+6,tmp,0)==87);
    assert(xarray_write(&a,0xffffffffUL,tmp,2)==87&&!writes&&!reads);
    assert(xarray_read(&a,3*XARRAY_PAGE,tmp,5)==0&&reads==1);  /* short final page */
    for(unsigned i=0;i<5;i++)assert(tmp[i]==0);
    assert(!xarray_close(&a)&&!live);

    /* Zero fill, page-crossing and odd-length transfers. */
    reset();assert(!xarray_open(&a,&ram,64*1024UL,cache,4));
    unsigned char big[3*XARRAY_PAGE+11],copy[sizeof big];
    for(unsigned i=0;i<sizeof big;i++)big[i]=pat(i);
    assert(!xarray_write(&a,XARRAY_PAGE-3,big,sizeof big));
    memset(copy,0xee,sizeof copy);assert(!xarray_read(&a,XARRAY_PAGE-3,copy,sizeof copy)&&!memcmp(big,copy,sizeof big));
    assert(!xarray_flush(&a));assert(!memcmp(block+XARRAY_PAGE-3,big,sizeof big));
    assert(block[XARRAY_PAGE-4]==0&&block[XARRAY_PAGE-3+sizeof big]==0);
    assert(!xarray_close(&a));

    /* LRU and write-back accounting with a two-line cache. */
    reset();assert(!xarray_open(&a,&ram,8*XARRAY_PAGE,cache,2));
    unsigned char one=0x11;
    assert(!xarray_write(&a,0*XARRAY_PAGE,&one,1));             /* miss 0 (dirty) */
    assert(!xarray_read(&a,1*XARRAY_PAGE,&one,1));              /* miss 1 */
    assert(!xarray_read(&a,0*XARRAY_PAGE,&one,1));              /* hit 0 -> page 1 is LRU */
    assert(!xarray_read(&a,2*XARRAY_PAGE,&one,1));              /* evicts clean page 1: no write */
    assert(writes==0);
    assert(!xarray_read(&a,3*XARRAY_PAGE,&one,1));              /* evicts dirty page 0 */
    assert(writes==1&&last_write_page==0&&block[0]==0x11);
    assert(!xarray_read(&a,0*XARRAY_PAGE,&one,1)&&one==0x11);   /* refetched value */
    assert(writes==1);                                          /* evicting clean page 2 */
    xarray_stats(&a,&s);assert(s.hits==1&&s.misses==5&&s.writebacks==1);
    assert(!xarray_flush(&a)&&writes==1);                       /* nothing dirty: idempotent */
    one=0x22;assert(!xarray_write(&a,0,&one,1));assert(!xarray_flush(&a)&&writes==2);assert(!xarray_flush(&a)&&writes==2);
    assert(!xarray_close(&a));

    /* Single-line cache still works for crossing transfers. */
    reset();assert(!xarray_open(&a,&ram,4*XARRAY_PAGE,cache,1));
    fill(&a,3*XARRAY_PAGE);
    unsigned char b2[XARRAY_PAGE+8];assert(!xarray_read(&a,XARRAY_PAGE-4,b2,sizeof b2));
    for(unsigned i=0;i<sizeof b2;i++)assert(b2[i]==pat(XARRAY_PAGE-4+i));
    assert(!xarray_close(&a));

    /* Direct page access. */
    reset();assert(!xarray_open(&a,&ram,XARRAY_PAGE+100,cache,2));
    unsigned char *p;unsigned avail;
    assert(xarray_page(&a,XARRAY_PAGE+100,0,&p,&avail)==87);
    assert(!xarray_page(&a,10,1,&p,&avail)&&avail==XARRAY_PAGE-10);p[0]=0x5a;p[avail-1]=0xa5;
    assert(!xarray_page(&a,XARRAY_PAGE+50,0,&p,&avail)&&avail==50);
    assert(!xarray_flush(&a)&&block[10]==0x5a&&block[XARRAY_PAGE-1]==0xa5);
    assert(!xarray_close(&a));

    /* Backend failures leave the array consistent and the data retryable. */
    reset();assert(!xarray_open(&a,&ram,4*XARRAY_PAGE,cache,1));
    one=0x77;assert(!xarray_write(&a,0,&one,1));
    fail_write=1;assert(xarray_read(&a,XARRAY_PAGE,&one,1)==8);   /* eviction write fails: dirty page stays */
    assert(xarray_flush(&a)==8);fail_write=0;
    one=0;assert(!xarray_read(&a,0,&one,1)&&one==0x77);          /* still the cached, dirty value */
    assert(!xarray_flush(&a)&&block[0]==0x77);
    fail_read=1;assert(xarray_read(&a,2*XARRAY_PAGE,&one,1)==8);fail_read=0;
    assert(!xarray_read(&a,2*XARRAY_PAGE,&one,1)&&one==0);       /* failed fetch left no stale line */
    assert(!xarray_read(&a,0,&one,1)&&one==0x77);
    assert(!xarray_close(&a));

    /* Close releases the block even when the final flush fails, and reports the first error. */
    reset();assert(!xarray_open(&a,&ram,XARRAY_PAGE,cache,1));
    one=1;assert(!xarray_write(&a,0,&one,1));fail_write=1;assert(xarray_close(&a)==8&&!live);fail_write=0;
    assert(xarray_close(&a)==6);                                   /* double close: stale token */
    reset();assert(!xarray_open(&a,&ram,XARRAY_PAGE,cache,1));fail_release=1;assert(xarray_close(&a)==8);fail_release=0;
    live=0;free(block);block=0;

    /* Randomized comparison against a flat reference over a 12-page array. */
    reset();srand(12345);
    enum {SIZE=12*XARRAY_PAGE-77};
    static unsigned char ref[SIZE],out[3*XARRAY_PAGE],in[3*XARRAY_PAGE];
    for(unsigned lines=1;lines<=XARRAY_MAX_PAGES;lines++){
        memset(ref,0,sizeof ref);assert(!xarray_open(&a,&ram,SIZE,cache,lines));
        for(unsigned step=0;step<4000;step++){
            unsigned long off=(unsigned long)rand()%SIZE;unsigned n=(unsigned)rand()%sizeof in;
            if(off+n>SIZE)n=(unsigned)(SIZE-off);
            if(rand()&1){for(unsigned i=0;i<n;i++)in[i]=ref[off+i]=(unsigned char)rand();assert(!xarray_write(&a,off,in,n));}
            else{assert(!xarray_read(&a,off,out,n)&&!memcmp(out,ref+off,n));}
            if(!(step%97))assert(!xarray_flush(&a));
        }
        assert(!xarray_flush(&a)&&!memcmp(block,ref,SIZE));
        assert(!xarray_close(&a));
    }
    puts("xarray: paging, LRU, write-back, ranges, odd lengths, direct pages, failures and randomized model passed");
    return 0;
}
