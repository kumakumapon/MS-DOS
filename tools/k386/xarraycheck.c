/* Paged XMS array demo and self-check (LARGE-DATA.md). Each scenario verifies
 * its own result and prints cache statistics; wall time is not available to
 * V86 applications (port I/O is trapped), so cost is judged from miss counts. */
#include "../filer/filer.h"
#include "sdk.h"
#include "xarray.h"
enum { LINES=4, SORT_VALUES=32768, FILL_BYTES=2048*1024 };
static unsigned char cache[2*LINES*XARRAY_PAGE];
static XArray a,b;
static int fail(const char *message){p_output("XARRAY FAIL: ");p_output(message);p_output("\r\n");return 1;}
static void number(unsigned long value){char text[12];fd_number(text,value);p_output(text);}
static void report(const char *name,const XArray *array){
    XArrayStats s;xarray_stats(array,&s);
    p_output("XARRAY OK ");p_output(name);p_output(" hits=");number(s.hits);
    p_output(" misses=");number(s.misses);p_output(" writebacks=");number(s.writebacks);p_output("\r\n");
}
static unsigned char pattern(unsigned long at){return (unsigned char)(at^(at>>8)^(at>>16));}
/* mode 0: expect zeros, 1: write the pattern, 2: expect the pattern. */
static int sweep(XArray *x,unsigned long size,int mode){
    for(unsigned long at=0;at<size;){
        unsigned char *page;unsigned n;
        if(xarray_page(x,at,mode==1,&page,&n))return 1;
        for(unsigned i=0;i<n;i++){
            if(mode==1)page[i]=pattern(at+i);
            else if(page[i]!=(mode?pattern(at+i):0))return 2;
        }
        at+=n;
    }
    return 0;
}
static int scenario_fill(void){
    if(xarray_open(&a,&xarray_kapp_backend,FILL_BYTES,cache,LINES))return fail("open 2 MiB");
    if(sweep(&a,FILL_BYTES,0))return fail("new array is not zero");
    if(sweep(&a,FILL_BYTES,1))return fail("pattern write");
    if(sweep(&a,FILL_BYTES,2))return fail("pattern verify");
    report("fill 2 MiB",&a);
    return xarray_close(&a)?fail("close"):0;
}
static int scenario_sequential(void){
    enum { PAGES=256 };
    unsigned char byte;XArrayStats s;
    if(xarray_open(&a,&xarray_kapp_backend,PAGES*(unsigned long)XARRAY_PAGE,cache,LINES))return fail("open 1 MiB");
    for(unsigned long page=0;page<PAGES;page++){
        if(xarray_read(&a,page*XARRAY_PAGE,&byte,1)||xarray_read(&a,page*XARRAY_PAGE+XARRAY_PAGE-1,&byte,1))return fail("sequential read");
    }
    xarray_stats(&a,&s);
    if(s.misses!=PAGES||s.hits!=PAGES||s.writebacks)return fail("sequential statistics");
    report("sequential 1 MiB",&a);
    return xarray_close(&a)?fail("close"):0;
}
static int get(XArray *x,unsigned long index,unsigned *value){unsigned char raw[2];if(xarray_read(x,index*2,raw,2))return 1;*value=raw[0]|raw[1]<<8;return 0;}
static int put(XArray *x,unsigned long index,unsigned value){unsigned char raw[2];raw[0]=value&255;raw[1]=value>>8&255;return xarray_write(x,index*2,raw,2);}
static int scenario_sort(void){
    unsigned long seed=12345,sum=0,check=0;unsigned previous=0,value;
    XArray *src=&a,*dst=&b,*swap;
    if(xarray_open(src,&xarray_kapp_backend,SORT_VALUES*2UL,cache,LINES))return fail("open sort source");
    if(xarray_open(dst,&xarray_kapp_backend,SORT_VALUES*2UL,cache+LINES*XARRAY_PAGE,LINES)){xarray_close(src);return fail("open sort target");}
    p_output("XARRAY sorting ");number(SORT_VALUES);p_output(" 16-bit values\r\n");
    for(unsigned long i=0;i<SORT_VALUES;i++){
        seed=seed*1103515245UL+12345UL;value=(unsigned)(seed>>8)&0xffff;sum+=value;
        if(put(src,i,value))return fail("sort fill");
    }
    for(unsigned long width=1;width<SORT_VALUES;width*=2){
        for(unsigned long start=0;start<SORT_VALUES;start+=2*width){
            unsigned long left=start,right=start+width,out=start;
            unsigned long left_end=right<SORT_VALUES?right:SORT_VALUES,right_end=start+2*width<SORT_VALUES?start+2*width:SORT_VALUES;
            unsigned lv=0,rv=0;int have_l=0,have_r=0;
            if(left<left_end){if(get(src,left,&lv))return fail("sort read");have_l=1;}
            if(right<right_end){if(get(src,right,&rv))return fail("sort read");have_r=1;}
            while(have_l||have_r){
                unsigned chosen;
                if(have_l&&(!have_r||lv<=rv)){
                    chosen=lv;left++;have_l=0;
                    if(left<left_end){if(get(src,left,&lv))return fail("sort read");have_l=1;}
                }else{
                    chosen=rv;right++;have_r=0;
                    if(right<right_end){if(get(src,right,&rv))return fail("sort read");have_r=1;}
                }
                if(put(dst,out++,chosen))return fail("sort write");
            }
        }
        swap=src;src=dst;dst=swap;
    }
    for(unsigned long i=0;i<SORT_VALUES;i++){
        if(get(src,i,&value))return fail("sort verify read");
        if(i&&value<previous)return fail("sort order");
        previous=value;check+=value;
    }
    if(check!=sum)return fail("sort checksum");
    report("sort source",src);report("sort target",dst);
    if(xarray_close(src)|xarray_close(dst))return fail("close");
    return 0;
}
static int scenario_search(void){
    static const unsigned char needle[8]={'K','3','8','6','x','m','s','!'};
    enum { SIZE=64*1024, WINDOW=1024 };
    unsigned char window[WINDOW+sizeof needle-1];
    unsigned long at=3*XARRAY_PAGE-3,found=0xffffffffUL,matches=0;
    if(xarray_open(&a,&xarray_kapp_backend,SIZE,cache,LINES))return fail("open search");
    if(xarray_write(&a,at,needle,sizeof needle))return fail("plant needle");
    for(unsigned long start=0;start<SIZE;start+=WINDOW){
        unsigned n=WINDOW+sizeof needle-1;
        if(start+n>SIZE)n=(unsigned)(SIZE-start);
        if(xarray_read(&a,start,window,n))return fail("search read");
        for(unsigned i=0;i+sizeof needle<=n&&i<WINDOW;i++){
            unsigned j=0;
            while(j<sizeof needle&&window[i+j]==needle[j])j++;
            if(j==sizeof needle){matches++;found=start+i;}
        }
    }
    if(matches!=1||found!=at)return fail("needle across a page boundary");
    report("search",&a);
    return xarray_close(&a)?fail("close"):0;
}
static int scenario_range(void){
    unsigned char byte=0;XArrayStats before,after;unsigned char *page;unsigned n;
    if(xarray_open(&a,&xarray_kapp_backend,10000,cache,LINES))return fail("open range");
    xarray_stats(&a,&before);
    if(xarray_read(&a,10000,&byte,1)!=87||xarray_write(&a,9999,"ab",2)!=87||xarray_read(&a,0xffffffffUL,&byte,2)!=87||xarray_page(&a,10000,0,&page,&n)!=87)return fail("range accepted");
    xarray_stats(&a,&after);
    if(before.misses!=after.misses||before.hits!=after.hits||before.writebacks!=after.writebacks)return fail("range check transferred");
    if(xarray_read(&a,10000,&byte,0)||xarray_read(&a,9999,&byte,1))return fail("valid end range");
    report("range",&a);
    return xarray_close(&a)?fail("close"):0;
}
static int run_all(void){
    return scenario_fill()||scenario_sequential()||scenario_sort()||scenario_search()||scenario_range();
}
int main(void){
    if(kapp_xms_version()!=0x100)return fail("XMS API missing");
    p_output("XARRAY READY\r\n1 fill, 2 sequential, 3 sort, 4 search, 5 range, A all, Q quit\r\n");
    for(;;){
        unsigned ax=0x0800;__asm__ volatile("int $0x21":"+a"(ax)::"cc","memory");unsigned key=ax&255;
        if(key=='q'||key=='Q')return 0;
        if(key=='1'&&scenario_fill())return 1;
        if(key=='2'&&scenario_sequential())return 1;
        if(key=='3'&&scenario_sort())return 1;
        if(key=='4'&&scenario_search())return 1;
        if(key=='5'&&scenario_range())return 1;
        if(key=='a'||key=='A'){if(run_all())return 1;p_output("XARRAY ALL OK\r\n");}
        kapp_yield();
    }
}
