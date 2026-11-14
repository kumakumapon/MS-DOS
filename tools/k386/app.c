#include "app.h"
int kapp_header(const unsigned char *h,unsigned n) {
    static const char magic[]="K386APP1";
    if(n<36||h[0]!='M'||h[1]!='Z')return 0;
    for(unsigned i=0;i<8;i++)if(h[28+i]!=(unsigned char)magic[i])return 0;
    if(n<KAPP_HEADER_SIZE||(unsigned)(h[8]|h[9]<<8)*16<KAPP_HEADER_SIZE||
       (unsigned)(h[24]|h[25]<<8)<KAPP_HEADER_SIZE||
       (h[36]|h[37]<<8)!=KAPP_ABI||
       ((h[38]|h[39]<<8)!=KAPP_TEXT&&(h[38]|h[39]<<8)!=KAPP_DESKTOP))return -1;
    unsigned i=40;
    for(;i<72&&h[i];i++)if(h[i]<32||h[i]>126)return -1;
    return i>40&&i<72?(h[38]|h[39]<<8):-1;
}

/* GUI classification only; V86's kapp_header deliberately still rejects kind 3. */
int kapp_pm32(const unsigned char *h,unsigned n){
    const char *magic="K386APP1",*elf="K32EXE01";
    if(n<96||h[0]!='M'||h[1]!='Z'||h[8]!=6||h[9]||h[24]!=96||h[25]||h[36]!=0||h[37]!=1||h[38]!=3||h[39])return 0;
    for(unsigned i=0;i<8;i++)if(h[28+i]!=(unsigned char)magic[i]||h[72+i]!=(unsigned char)elf[i])return 0;
    unsigned end=40;for(;end<72&&h[end];end++)if(h[end]<32||h[end]>126)return 0;
    if(end==40||end==72||h[93]||h[94]||h[95]||(h[92]&~15)||(h[80]==0&&h[92]))return 0;
    return (h[80]<=2)&&h[81]==1&&h[82]==24&&!h[83];
}
