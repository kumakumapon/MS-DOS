#include "libc.h"
#ifndef K32_LIBC_TEST
#include "k32x.h"
#else
extern void *k32_alloc(unsigned);
extern int k32_free(void *);
#endif
void *k32_memcpy(void *d,const void *s,unsigned n){unsigned char *a=d;const unsigned char *b=s;for(unsigned i=0;i<n;i++)a[i]=b[i];return d;}
void *k32_memmove(void *d,const void *s,unsigned n){unsigned char *a=d;const unsigned char *b=s;if(a>b){while(n){n--;a[n]=b[n];}}else k32_memcpy(d,s,n);return d;}
void *k32_memset(void *d,int c,unsigned n){unsigned char *a=d;for(unsigned i=0;i<n;i++)a[i]=(unsigned char)c;return d;}
int k32_memcmp(const void *a,const void *b,unsigned n){const unsigned char *s=a,*t=b;for(unsigned i=0;i<n;i++)if(s[i]!=t[i])return s[i]<t[i]?-1:1;return 0;}
unsigned k32_strlen(const char *s){unsigned n=0;while(s[n])n++;return n;}
int k32_strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
static void emit(char *out,unsigned cap,unsigned *n,char c){if(cap&&*n<cap-1)out[*n]=c;(*n)++;}
int k32_vsnprintf(char *out,unsigned cap,const char *format,va_list args){
 unsigned n=0;if(cap&&!out)return -22;
 for(;*format;format++){
  if(*format!='%'){emit(out,cap,&n,*format);continue;}
  char op=*++format;if(!op){if(cap)out[n<cap?n:cap-1]=0;return -22;}
  if(op=='%'){emit(out,cap,&n,'%');continue;}
  if(op=='s'){const char *s=va_arg(args,const char *);if(!s)s="(null)";while(*s)emit(out,cap,&n,*s++);continue;}
  if(op=='c'){emit(out,cap,&n,(char)va_arg(args,int));continue;}
  if(op!='u'&&op!='d'&&op!='x'&&op!='X'){if(cap)out[n<cap?n:cap-1]=0;return -22;}
  unsigned value;if(op=='d'){int v=va_arg(args,int);value=(unsigned)v;if(v<0){emit(out,cap,&n,'-');value=0-value;}}else value=va_arg(args,unsigned);
  unsigned radix=op=='x'||op=='X'?16:10,len=0;char digits[32];const char *alphabet=op=='X'?"0123456789ABCDEF":"0123456789abcdef";
  do{digits[len++]=alphabet[value%radix];value/=radix;}while(value);
  while(len)emit(out,cap,&n,digits[--len]);
 }
 if(cap)out[n<cap?n:cap-1]=0;
 return (int)n;
}
int k32_snprintf(char *out,unsigned cap,const char *format,...){va_list args;va_start(args,format);int n=k32_vsnprintf(out,cap,format,args);va_end(args);return n;}
typedef struct {unsigned magic,bytes,reserved[2];} Allocation;
void *k32_malloc(unsigned n){if(!n||n>0x400000-sizeof(Allocation))return 0;Allocation *p=k32_alloc(n+sizeof *p);if(!p)return 0;*p=(Allocation){.magic=0x4b334d31,.bytes=n};return p+1;}
void k32_free_memory(void *p){if(!p)return;Allocation *a=(Allocation *)p-1;if(a->magic==0x4b334d31){a->magic=0;(void)k32_free(a);}}
void *k32_calloc(unsigned n,unsigned size){if(!size||n>0x400000/size)return 0;void *p=k32_malloc(n*size);if(p)k32_memset(p,0,n*size);return p;}
void *k32_realloc(void *p,unsigned n){if(!p)return k32_malloc(n);if(!n){k32_free_memory(p);return 0;}Allocation *a=(Allocation *)p-1;if(a->magic!=0x4b334d31)return 0;if(n<=a->bytes){a->bytes=n;return p;}void *q=k32_malloc(n);if(q){k32_memcpy(q,p,a->bytes);k32_free_memory(p);}return q;}
