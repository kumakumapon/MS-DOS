#ifndef K32_EXTENDED_SDK_H
#define K32_EXTENDED_SDK_H
#include "k32.h"
#define K32_ABI_EXT 0x0101
#define K32_WAIT_KEY 1
#define K32_WAIT_TIME 2
#define K32_BOOT_ADDRESS 0xbfd000u
typedef struct {unsigned magic,abi,size,argc,argv,features,heap_base,heap_limit,page_size,tick_rate,reserved[6];} K32Info;
static inline const K32Info *k32_info(void){return (const K32Info *)K32_BOOT_ADDRESS;}
static inline int k32_call4(unsigned n,unsigned b,unsigned c,unsigned d){__asm__ volatile("int $0x30":"+a"(n):"b"(b),"c"(c),"d"(d):"cc","memory");return (int)n;}
static inline int k32_query(void){return k32_call(4,0,0);}
static inline void *k32_alloc(unsigned n){int p=k32_call(5,n,0);return p<0?0:(void *)(unsigned)p;}
static inline int k32_free(void *p){return k32_call(6,(unsigned)p,0);}
static inline int k32_poll_key(void){return k32_call(7,0,0);}
static inline int k32_wait(unsigned flags,unsigned ms){return k32_call(8,flags,ms);}
static inline int k32_open(const char *name){unsigned n=0;while(n<13&&name[n])n++;return n>12?-22:k32_call(9,(unsigned)name,n);}
static inline int k32_read(unsigned h,void *p,unsigned n){return k32_call4(10,h,(unsigned)p,n);}
static inline int k32_seek(unsigned h,unsigned at){return k32_call(11,h,at);}
static inline int k32_close(unsigned h){return k32_call(12,h,0);}
static inline unsigned k32_len(const char *s){unsigned n=0;while(s[n])n++;return n;}
static inline int k32_puts(const char *s){return k32_write(s,k32_len(s));}
#endif
