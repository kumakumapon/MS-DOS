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
#endif
