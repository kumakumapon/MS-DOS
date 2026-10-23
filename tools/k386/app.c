#include "app.h"
int kapp_header(const unsigned char *h,unsigned n) {
    static const char magic[]="K386APP1";
    if(n<36||h[0]!='M'||h[1]!='Z')return 0;
    for(unsigned i=0;i<8;i++)if(h[28+i]!=(unsigned char)magic[i])return 0;
    if(n<KAPP_HEADER_SIZE||(unsigned)(h[8]|h[9]<<8)*16<KAPP_HEADER_SIZE||
       (unsigned)(h[24]|h[25]<<8)<KAPP_HEADER_SIZE||
       (h[36]|h[37]<<8)!=KAPP_ABI||(h[38]|h[39]<<8)!=KAPP_TEXT)return -1;
    unsigned i=40;
    for(;i<72&&h[i];i++)if(h[i]<32||h[i]>126)return -1;
    return i>40&&i<72?1:-1;
}
