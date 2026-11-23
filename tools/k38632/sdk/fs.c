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
