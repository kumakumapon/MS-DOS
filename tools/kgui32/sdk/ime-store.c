#include "ime-store.h"
#include "../../k38632/sdk/libc.h"
static unsigned char staged[KI_DICTIONARY];static KiUser checked;
static unsigned hash(const unsigned char *p,unsigned n){unsigned h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static int read_variant(unsigned variant,K32Revision *revision){
 unsigned size;int h=k32_io_open(KI_USER_FILE,variant,&size);if(h<0)return h;
 int error=size>KI_DICTIONARY?-27:0;unsigned n=0;
 while(!error&&n<size){unsigned count=size-n;if(count>4096)count=4096;int got=k32_io_read((unsigned)h,staged+n,count);if(got<=0){error=got?got:-5;break;}n+=(unsigned)got;}
 if(!error){unsigned char extra;int got=k32_io_read((unsigned)h,&extra,1);if(got)error=got<0?got:-116;}
 int closed=k32_io_close((unsigned)h);if(!error)error=closed;if(error)return error;
 error=ki_user_load(&checked,staged,n);if(error)return error;
 *revision=(K32Revision){1,n,hash(staged,n)};return 0;
}
void ki_store_init(KiStore *s){ki_user_init(&s->words);s->revision=(K32Revision){0};s->loaded=0;s->error=0;}
int ki_store_load(KiStore *s){
 K32Revision next;int e=read_variant(0,&next);
 if(e==-2){ki_user_init(&s->words);s->revision=(K32Revision){0};s->loaded=1;s->error=0;return 0;}
 if(!e){K32Revision current;e=k32_io_revision(KI_USER_FILE,0,&current);if(!e&&(current.exists!=next.exists||current.length!=next.length||current.hash!=next.hash))e=-116;}
 if(!e){s->words=checked;s->revision=next;s->loaded=1;}s->error=e;return e;
}
int ki_store_save(KiStore *s){
 if(!s->loaded)return -16;
 KiDictionary valid;int e=ki_user_view(&s->words,&valid);if(e)return e;
 e=k32_io_save(KI_USER_FILE,s->words.data,s->words.bytes,&s->revision);
 if(e>=0){s->revision=(K32Revision){1,s->words.bytes,hash(s->words.data,s->words.bytes)};s->words.dirty=0;}s->error=e<0?e:0;return e;
}
int ki_store_inspect(unsigned variant,KiDictionary *view,K32Revision *revision){if(variant<1||variant>2)return -22;int e=read_variant(variant,revision);if(e)return e;return ki_user_view(&checked,view);}
int ki_store_restore(unsigned variant,const char *destination,const K32Revision *expected){
 KiDictionary view;K32Revision current;int e=ki_store_inspect(variant,&view,&current);if(e)return e;
 if(current.exists!=expected->exists||current.length!=expected->length||current.hash!=expected->hash)return -116;
 return k32_io_recover(KI_USER_FILE,variant,destination,expected);
}
