#ifndef K32_FS_SDK_H
#define K32_FS_SDK_H
#include "io.h"
K32Fs k32_fs_request(unsigned action,const char *path,const char *destination);
int k32_fs(K32Fs *r);
#endif
