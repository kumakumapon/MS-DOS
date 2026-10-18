#include "loader.h"
static k16 word(const k8 *h,int at){return h[at]|(k16)h[at+1]<<8;}
int k386_image(const k8 *h,k32 length,K386Image *p) {
    *p=(K386Image){0, length, K386_SEGMENT,0x100,K386_SEGMENT,0xfffc,0,0};
    if(!length||length>0x10000)return 11;
    if(length<2||h[0]!='M'||h[1]!='Z')return length<=0xf000?0:8;
    if(length<28)return 11;
    k32 pages=word(h,4),last=word(h,2);
    if(!pages||last>511)return 11;
    k32 declared=(pages-1)*512+(last?last:512);
    p->header=(k32)word(h,8)*16;
    if(declared!=length||p->header<28||p->header>=length||word(h,26))return 11;
    p->bytes=length-p->header;
    if(p->bytes>0xff00||(k32)word(h,10)*16>0xff00-p->bytes)return 8;
    p->relocations=word(h,6);p->table=word(h,24);
    if(p->relocations&&(p->table<28||(k32)p->table+(k32)p->relocations*4>p->header))return 11;
    p->cs=(k16)(0x8010+word(h,22));p->ip=word(h,20);
    p->ss=(k16)(0x8010+word(h,14));p->sp=word(h,16);
    k32 pc=(k32)p->cs*16+p->ip;
    k32 stack=(k32)p->ss*16+(p->sp?p->sp:0x10000);
    if(pc<0x80100||pc>=0x80100+p->bytes||stack<0x80002||stack>0x90000)return 11;
    return 0;
}
int k386_relocation(const K386Image *p,k16 offset,k16 segment) {
    k32 at=(k32)segment*16+offset;
    return offset!=0xffff&&at<p->bytes&&p->bytes-at>=2;
}
