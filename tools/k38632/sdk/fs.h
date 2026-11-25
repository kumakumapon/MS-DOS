#ifndef K32_FS_SDK_H
#define K32_FS_SDK_H
#include "io.h"
K32Fs k32_fs_request(unsigned action,const char *path,const char *destination);
int k32_fs(K32Fs *r);
int k32_fs_read(const char *,unsigned,unsigned *);
int k32_fs_revision(const char *,K32Revision *);
int k32_fs_save(const char *,const void *,unsigned,const K32Revision *);
#endif
