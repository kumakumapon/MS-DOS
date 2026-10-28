#ifndef K386_SDK_H
#define K386_SDK_H
/* For 16-bit GCC freestanding text apps linked with app-start.S/app.ld.
 * ABI 1.0: AX=D388h; BX=0 query, BX=1 yield. AX=388Dh, BX=0100h,
 * CX=1 (read-only files), DX=one-based session ID. No guest pointers returned.
 */
static inline unsigned kapp_session(void) {
    unsigned a=0xd388,b=0,c,d;
    __asm__ volatile("int $0x2f":"+a"(a),"+b"(b),"=c"(c),"=d"(d)::"cc","memory");
    return a==0x388d&&b==0x0100?d:0;
}
static inline void kapp_yield(void) {
    unsigned a=0xd388,b=1,c,d;
    __asm__ volatile("int $0x2f":"+a"(a),"+b"(b),"=c"(c),"=d"(d)::"cc","memory");
}
/* Managed XMS data blocks: no driver entry point or host handle is exposed.
 * Wrappers preserve DS/ES and use ES:DI for at most 4096 data bytes.
 * Return 0 on success or a DOS-style error. An allocation may yield while zeroing.
 */
enum { KAPP_XMS_QUOTA_KIB=4096, KAPP_XMS_TRANSFER=4096 };
typedef struct {unsigned session_kib,session_blocks,process_kib,process_blocks;} KAppXmsUsage;
static inline unsigned kapp_xms_version(void) {
    unsigned a=0xd389,b=0,c,d;
    __asm__ volatile("int $0x2f":"+a"(a),"+b"(b),"=c"(c),"=d"(d)::"cc","memory");
    return a==0x389d?b:0;
}
static inline unsigned kapp_xms_call(unsigned op,unsigned *token,unsigned offset,void *buffer,unsigned count) {
    unsigned a=0xd389,b=op,c=count,d=*token,si=offset,di=(unsigned)buffer;
    __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x2f; popw %%es"
        :"+a"(a),"+b"(b),"+c"(c),"+d"(d),"+S"(si),"+D"(di)::"cc","memory");
    *token=d;return a;
}
static inline unsigned kapp_xms_alloc(unsigned kb,unsigned *token) {*token=0;return kapp_xms_call(1,token,0,0,kb);}
static inline unsigned kapp_xms_free(unsigned token) {return kapp_xms_call(2,&token,0,0,0);}
static inline unsigned kapp_xms_read(unsigned token,unsigned offset,void *out,unsigned count) {return kapp_xms_call(3,&token,offset,out,count);}
static inline unsigned kapp_xms_write(unsigned token,unsigned offset,const void *in,unsigned count) {return kapp_xms_call(4,&token,offset,(void *)in,count);}
static inline unsigned kapp_xms_usage(KAppXmsUsage *usage) {
    unsigned a=0xd389,b=5,c,d,si,di;
    __asm__ volatile("int $0x2f":"+a"(a),"+b"(b),"=c"(c),"=d"(d),"=S"(si),"=D"(di)::"cc","memory");
    usage->session_kib=c;usage->session_blocks=d;usage->process_kib=si;usage->process_blocks=di;return a;
}
#endif
