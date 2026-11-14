#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sdk/libc.h"
static unsigned allocations;
void *k32_alloc(unsigned n){allocations++;return calloc(1,n);}
int k32_free(void *p){allocations--;free(p);return 0;}
int main(void){
 char b[40],guard[6]={'!','!','!','!','!','!'};
 assert(k32_snprintf(b,sizeof b,"%d %u %x %X %% %c %s",(-2147483647-1),~0u,0xabcu,0xabcu,'Z',"ok")==37);
 assert(!strcmp(b,"-2147483648 4294967295 abc ABC % Z ok"));
 assert(k32_snprintf(guard+1,4,"abcdef")==6&&!memcmp(guard,"!abc\0!",6));
 assert(k32_snprintf(0,0,"%u",123u)==3);assert(k32_snprintf(b,sizeof b,"%q")==-22);
 strcpy(b,"abcdef");k32_memmove(b+1,b,6);assert(!memcmp(b,"aabcdef",7));k32_memmove(b,b+1,6);assert(!memcmp(b,"abcdef",6));
 unsigned char *p=k32_calloc(200,3);assert(p&&allocations==1);for(unsigned i=0;i<600;i++)assert(!p[i]);p[599]=23;
 assert(!k32_realloc(p,~0u)&&p[599]==23);p=k32_realloc(p,1200);assert(p&&p[599]==23&&allocations==1);assert(k32_realloc(p,100)==p);k32_free_memory(p);assert(!allocations);
 assert(!k32_calloc(~0u,2)&&!k32_malloc(0)&&!k32_malloc(~0u));return 0;
}
