#include "io.h"
#include "libc.h"
/* One thread per app. Scratch is private BSS, leaving legacy 4 KiB stacks free. */
static unsigned char scratch[4096];
int k32_io(K32Io *r){return k32_call(18,(unsigned)r,sizeof *r);}
K32Io k32_io_name(unsigned op,const char *name){K32Io r={.version=1,.size=sizeof r,.operation=op};if(name){unsigned i=0;for(;i<15&&name[i];i++)r.name[i]=name[i];if(name[i])r.reserved=1;}return r;}
int k32_io_open(const char *name,unsigned variant,unsigned *length){K32Io r=k32_io_name(K32_IO_OPEN,name);r.flags=variant;int e=k32_io(&r);if(e<0)return e;if(length)*length=r.length;return (int)r.token;}
int k32_io_read(unsigned token,void *data,unsigned n){K32Io r=k32_io_name(K32_IO_READ,0);r.token=token;r.data=(unsigned)data;r.bytes=n;return k32_io(&r);}
int k32_io_seek(unsigned token,unsigned at){K32Io r=k32_io_name(K32_IO_SEEK,0);r.token=token;r.offset=at;return k32_io(&r);}
int k32_io_close(unsigned token){K32Io r=k32_io_name(K32_IO_CLOSE,0);r.token=token;return k32_io(&r);}
int k32_io_revision(const char *name,unsigned variant,K32Revision *out){
 unsigned length;int h=k32_io_open(name,variant,&length);if(h==-2){*out=(K32Revision){0};return 0;}if(h<0)return h;
 unsigned bytes=0,hash=2166136261u;int error=0;
 for(;;){int n=k32_io_read((unsigned)h,scratch,sizeof scratch);if(n<0){error=n;break;}if(!n)break;bytes+=(unsigned)n;for(int i=0;i<n;i++)hash=(hash^scratch[i])*16777619u;}
 int closed=k32_io_close((unsigned)h);if(!error)error=closed;if(error)return error;if(bytes!=length)return -116;
 *out=(K32Revision){1,length,hash};return 0;
}
int k32_io_begin(const char *name,const K32Revision *old,K32Io *out){*out=k32_io_name(K32_IO_BEGIN,name);out->exists=old->exists;out->length=old->length;out->hash=old->hash;return k32_io(out);}
int k32_io_write(K32Io *r,const void *p,unsigned n){r->operation=K32_IO_WRITE;r->data=(unsigned)p;r->bytes=n;r->flags=0;return k32_io(r);}
int k32_io_commit(K32Io *r){r->operation=K32_IO_COMMIT;r->flags=0;int e;do{e=k32_io(r);}while(e==-11);return e;}
int k32_io_abort(K32Io *r,unsigned keep){r->operation=K32_IO_ABORT;r->flags=keep;return k32_io(r);}
int k32_io_save(const char *name,const void *data,unsigned bytes,const K32Revision *old){
 if(bytes>K32_IO_LIMIT)return -27;
 K32Io r;int e=k32_io_begin(name,old,&r);if(e<0)return e;
 const unsigned char *p=data;
 for(unsigned at=0;at<bytes;){unsigned n=bytes-at;if(n>4096)n=4096;e=k32_io_write(&r,p+at,n);if(e!=(int)n){int closed=k32_io_abort(&r,1);return closed<0?closed:e<0?e:-28;}at+=n;}
 e=k32_io_commit(&r);if(e<0){int closed=k32_io_abort(&r,1);if(closed<0)return closed;}return e;
}
int k32_io_recover(const char *name,unsigned variant,const char *destination,const K32Revision *old){
 unsigned n=k32_strlen(destination);if(n>12)return -22;
 K32Io r=k32_io_name(K32_IO_RECOVER,name);r.flags=variant;r.data=(unsigned)destination;r.bytes=n;r.exists=old->exists;r.length=old->length;r.hash=old->hash;
 int e;do{e=k32_io(&r);}while(e==-11);if(e<0&&r.token){int closed=k32_io_abort(&r,1);if(closed<0)return closed;}return e;
}
int k32_getchar(void){for(;;){int key=k32_poll_key();if(key>=0){if(key&255)return key&255;}else if(key!=-11)return key;int e=k32_wait(K32_WAIT_KEY,0);if(e<0)return e;}}
int k32_putchar(unsigned c){unsigned char byte=(unsigned char)c;int e=k32_write(&byte,1);return e==1?(int)byte:e<0?e:-5;}
int k32_printf(const char *format,...){char text[256];va_list args;va_start(args,format);int n=k32_vsnprintf(text,sizeof text,format,args);va_end(args);if(n<0)return n;if((unsigned)n>=sizeof text)return -27;return k32_write(text,(unsigned)n);}
