#ifndef K386_DRIVES_H
#define K386_DRIVES_H
#include "abi.h"
/* Private runtime state, not part of the public mailbox/session wire. */
typedef struct {char cwd[2][68];k16 current,hdd;} K386Drives;
void k386_drives_init(K386Drives *d,unsigned hdd);
int k386_drive_slot(const K386Drives *d,unsigned drive);
int k386_drive_path(const K386Drives *d,const char *input,char out[128],int wild);
int k386_drive_cwd(K386Drives *d,const char *absolute);
#endif
