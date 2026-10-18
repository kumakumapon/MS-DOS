#include "loader.h"
#include <assert.h>
static void w(k8 *h,int at,k16 n){h[at]=n;h[at+1]=n>>8;}
int main(void) {
    k8 h[32]={0};K386Image p;
    assert(k386_image(h,0,&p)==11);
    assert(!k386_image(h,128,&p)&&p.cs==0x8000&&p.ip==0x100);
    assert(!k386_image(h,0xff00,&p)&&p.required==0x1000);
    assert(k386_image(h,0xff01,&p)==8);
    h[0]='M';h[1]='Z';w(h,2,96);w(h,4,1);w(h,6,1);w(h,8,4);
    w(h,12,0xffff);w(h,16,0xfef0);w(h,24,28);
    assert(!k386_image(h,96,&p)&&p.header==64&&p.bytes==32&&p.cs==0x8010);
    assert(k386_relocation(&p,30,0));assert(!k386_relocation(&p,31,0));
    assert(!k386_relocation(&p,0xffff,0xffff));
    w(h,2,97);assert(k386_image(h,96,&p)==11);w(h,2,96);
    w(h,24,62);assert(k386_image(h,96,&p)==11);w(h,24,28);
    w(h,10,0xffff);assert(k386_image(h,96,&p)==8);w(h,10,0);
    w(h,22,0x1000);assert(k386_image(h,96,&p)==11);w(h,22,0);
    w(h,14,0x9000);assert(k386_image(h,96,&p)==11);w(h,14,0);
    w(h,26,1);assert(k386_image(h,96,&p)==11);
    /* A multi-segment MZ must reserve its whole stack and minimum heap. */
    w(h,26,0);w(h,2,0);w(h,4,256);w(h,6,0);w(h,8,2);
    w(h,10,0x1000);w(h,12,0x2000);w(h,14,0x2000);w(h,16,0x1000);
    assert(!k386_image(h,0x20000,&p));
    assert(p.bytes==0x1ffe0&&p.required==0x300e&&p.maximum==0x400e);
    w(h,14,0x4000);assert(k386_image(h,0x20000,&p)==11);
    w(h,14,0x2000);w(h,10,0x8000);w(h,12,0xffff);
    assert(k386_image(h,0x20000,&p)==8);
    assert(k386_image(h,0x90001,&p)==11);
    return 0;
}
