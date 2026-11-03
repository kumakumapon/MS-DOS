#ifndef K32_SDK_H
#define K32_SDK_H
#define K32_ABI 0x0100
static inline int k32_call(unsigned n,unsigned b,unsigned c){
 __asm__ volatile("int $0x30":"+a"(n):"b"(b),"c"(c):"cc","memory");return (int)n;
}
static inline int k32_write(const void *p,unsigned n){return k32_call(1,(unsigned)p,n);}
static inline void k32_yield(void){(void)k32_call(2,0,0);}
static inline unsigned k32_ticks(void){return (unsigned)k32_call(3,0,0);}
__attribute__((noreturn)) static inline void k32_exit(unsigned code){(void)k32_call(0,code,0);for(;;){} }
#endif
