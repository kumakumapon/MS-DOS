#include "loader.h"
static k16 word(const k8 *h,int at){return h[at]|(k16)h[at+1]<<8;}
int k386_image(const k8 *h,k32 length,K386Image *p) {
    *p=(K386Image){0,length,K386_SEGMENT,0x100,K386_SEGMENT,0xfffc,0,0,0x1000,0x9000};
    if(!length||length>0x90000)return 11;
    if(length<2||h[0]!='M'||h[1]!='Z')return length<=0xff00?0:8;
    if(length<28)return 11;
    k32 pages=word(h,4),last=word(h,2);
    if(!pages||last>511)return 11;
    k32 declared=(pages-1)*512+(last?last:512);
    p->header=(k32)word(h,8)*16;
    if(declared!=length||p->header<28||p->header>=length||word(h,26))return 11;
    p->bytes=length-p->header;
    k32 body=(p->bytes+15)/16+16,min=body+word(h,10),max=body+word(h,12);
    if(min>0x9000||word(h,12)<word(h,10))return 8;
    if(max>0x9000)max=0x9000;
    k32 pc=(k32)word(h,22)*16+word(h,20);
    k32 stack=(k32)word(h,14)*16+(word(h,16)?word(h,16):0x10000)+0x100;
    if(pc>=p->bytes||stack<0x102||stack>max*16)return 11;
    if(min<(stack+15)/16)min=(stack+15)/16;
    if(min<0x1000&&max>=0x1000)min=0x1000;
    p->required=min;p->maximum=max;
    p->relocations=word(h,6);p->table=word(h,24);
    if(p->relocations&&(p->table<28||(k32)p->table+(k32)p->relocations*4>p->header))return 11;
    p->cs=(k16)(0x8010+word(h,22));p->ip=word(h,20);
    p->ss=(k16)(0x8010+word(h,14));p->sp=word(h,16);
    return 0;
}
int k386_relocation(const K386Image *p,k16 offset,k16 segment) {
    k32 at=(k32)segment*16+offset;
    return offset!=0xffff&&at<p->bytes&&p->bytes-at>=2;
}
