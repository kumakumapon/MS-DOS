#include "hardware.h"

unsigned mem_alloc(u32 bytes) {
    unsigned a = 0x4800, b = (bytes + 15) >> 4;
    if (!bytes || bytes > 0x90000) return 0;
    __asm__ volatile("int $0x21; jnc 1f; xor %%eax,%%eax; 1:"
                     : "+a"(a), "+b"(b) :: "cc", "memory");
    return a & 65535;
}
void mem_free(unsigned segment) {
    if (!segment) return;
    unsigned a = 0x4900;
    __asm__ volatile("pushw %%es; movw %w1,%%es; int $0x21; popw %%es"
                     : "+a"(a) : "r"(segment) : "cc", "memory");
}
u8 mem_get(unsigned segment, u32 offset) {
    u8 value; segment += offset >> 4; offset &= 15;
    __asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es"
                     : "=q"(value) : "r"(segment), "r"(offset) : "memory");
    return value;
}
void mem_put(unsigned segment, u32 offset, u8 value) {
    segment += offset >> 4; offset &= 15;
    __asm__ volatile("pushw %%es; movw %w0,%%es; movb %b2,%%es:(%1); popw %%es"
                     :: "r"(segment), "r"(offset), "q"(value) : "memory");
}
void mem_copy(unsigned dst, u32 to, unsigned src, u32 from, u32 length) {
    while (length) {
        unsigned n = length > 0xff00 ? 0xff00 : length;
        unsigned ds = src + (from >> 4), es = dst + (to >> 4);
        unsigned si = from & 15, di = to & 15, count = n;
        __asm__ volatile("pushw %%ds; pushw %%es; movw %w3,%%ds; movw %w4,%%es;"
                         "rep movsb; popw %%es; popw %%ds"
                         : "+S"(si), "+D"(di), "+c"(count) : "r"(ds), "r"(es) : "memory");
        from += n; to += n; length -= n;
    }
}
void mem_fill(unsigned dst, u32 offset, u32 length, u8 color) {
    while (length) {
        unsigned n = length > 0xff00 ? 0xff00 : length;
        unsigned es = dst + (offset >> 4), di = offset & 15, count = n;
        __asm__ volatile("pushw %%es; movw %w3,%%es; rep stosb; popw %%es"
                         : "+D"(di), "+c"(count) : "a"(color), "r"(es) : "memory");
        offset += n; length -= n;
    }
}
