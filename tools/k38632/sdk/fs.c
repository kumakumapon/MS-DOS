#include "fs.h"

static void path_copy(char out[K32_FS_PATH],const char *in){
 if(!in)return;
 /* A nonterminated field is invalid at the broker, preserving overflow. */
 for(unsigned i=0;i<K32_FS_PATH;i++){out[i]=in[i];if(!in[i])break;}
}
K32Fs k32_fs_request(unsigned action,const char *path,const char *destination){
 K32Fs r={.version=1,.size=sizeof r,.action=action};
 path_copy(r.path,path);path_copy(r.destination,destination);return r;
}
int k32_fs(K32Fs *r){
 K32Io io=k32_io_name(K32_IO_FS,0);
 io.data=(unsigned)r;io.bytes=sizeof *r;
 int e=k32_io(&io);
 return e<0?e:e==(int)sizeof *r?0:-5;
}
/* Namespace 0 is Work. Namespace 1 onwards is a read-only DOS drive. */
int k32_fs_read(const char *path,unsigned volume,unsigned *bytes){
 K32Fs r=k32_fs_request(volume?K32_FS_DRIVE_OPEN:K32_FS_OPEN,path,0);
 r.flags=volume?volume-1:0;
 int e=k32_fs(&r);if(e)return e;
 if(bytes)*bytes=r.length;
 return (int)r.token;
}
int k32_fs_revision(const char *path,K32Revision *out){
 unsigned bytes=0;int h=k32_fs_read(path,0,&bytes);
 return k32_io_scan(h,bytes,out);
}
int k32_fs_begin_save(const char *path,const K32Revision *old,K32Io *writer){
 K32Fs r=k32_fs_request(K32_FS_SAVE,path,0);
 r.flags=old->exists;r.length=old->length;r.hash=old->hash;
 int e=k32_fs(&r);if(e)return e;
 *writer=(K32Io){.version=1,.size=sizeof *writer,.token=r.token};return 0;
}
int k32_fs_save(const char *path,const void *data,unsigned bytes,const K32Revision *old){
 if(bytes>K32_IO_LIMIT)return -27;
 K32Io writer;int e=k32_fs_begin_save(path,old,&writer);
 return e?e:k32_io_finish_save(&writer,data,bytes);
}
