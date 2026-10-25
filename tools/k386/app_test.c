#include "app.h"
#include <assert.h>
#include <string.h>
int main(void) {
    unsigned char h[96]={0};
    assert(!kapp_header(h,sizeof h));
    h[0]='M';h[1]='Z';h[8]=5;h[24]=72;
    memcpy(h+28,"K386APP1",8);h[37]=1;h[38]=1;memcpy(h+40,"Hello",6);
    assert(kapp_header(h,sizeof h)==1);
    assert(kapp_header(h,71)==-1);
    h[24]=28;assert(kapp_header(h,sizeof h)==-1);h[24]=72;
    h[8]=4;assert(kapp_header(h,sizeof h)==-1);h[8]=5;
    h[37]=2;assert(kapp_header(h,sizeof h)==-1);h[37]=1;
    h[38]=2;assert(kapp_header(h,sizeof h)==KAPP_DESKTOP);
    h[38]=3;assert(kapp_header(h,sizeof h)==-1);h[38]=1;
    memset(h+40,'A',32);assert(kapp_header(h,sizeof h)==-1);
    h[71]=0;assert(kapp_header(h,sizeof h)==1);
    h[40]=0;assert(kapp_header(h,sizeof h)==-1);
    return 0;
}
