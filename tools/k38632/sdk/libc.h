/* Small freestanding C subset. Names are prefixed; no hosted libc ABI claimed. */
#ifndef K32_LIBC_H
#define K32_LIBC_H
#include <stdarg.h>
void *k32_memcpy(void *,const void *,unsigned);
void *k32_memmove(void *,const void *,unsigned);
void *k32_memset(void *,int,unsigned);
int k32_memcmp(const void *,const void *,unsigned);
unsigned k32_strlen(const char *);
int k32_strcmp(const char *,const char *);
int k32_vsnprintf(char *,unsigned,const char *,va_list);
int k32_snprintf(char *,unsigned,const char *,...);
void *k32_malloc(unsigned);
void *k32_calloc(unsigned,unsigned);
void *k32_realloc(void *,unsigned);
void k32_free_memory(void *);
#endif
