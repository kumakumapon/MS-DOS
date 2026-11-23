#ifndef K32_COMMON_IO_H
#define K32_COMMON_IO_H
#include "directory.h"
#define K32_FEATURE_IO 256u
#define K32_IO_LIMIT 1048576u
enum {K32_IO_STAT=1,K32_IO_OPEN,K32_IO_READ,K32_IO_SEEK,K32_IO_CLOSE,
 K32_IO_BEGIN,K32_IO_WRITE,K32_IO_COMMIT,K32_IO_ABORT,K32_IO_RECOVER,K32_IO_FS};
typedef struct {
 unsigned version,size,operation,flags;char name[16];
 unsigned token,data,bytes,offset,exists,length,hash,reserved;
} K32Io;
_Static_assert(sizeof(K32Io)==64,"common file request");
/* Relative, three-component DOS paths under the broker-selected Work root.
 * Boot operations use a separate, fixed set of A: filenames. */
#define K32_FS_PATH 40
#define K32_FS_BOOT_LIMIT 2048 /* same per-file capacity as CPANEL */
enum {K32_FS_LIST=1,K32_FS_NEXT,K32_FS_OPEN,K32_FS_CREATE,K32_FS_MKDIR,
 K32_FS_MOVE,K32_FS_TRASH,K32_FS_RESTORE,K32_FS_BOOT_OPEN,K32_FS_BOOT_WRITE};
#define K32_FS_LIST_TRASH 1u
#define K32_BOOT_XMS 1u
#define K32_BOOT_EMS 2u
#define K32_BOOT_FD98 4u
typedef struct {
 unsigned version,size,action,flags,token,length,hash;
 char path[K32_FS_PATH],destination[K32_FS_PATH];
 K32DirEntry entry;
} K32Fs;
_Static_assert(sizeof(K32Fs)==172,"Work management request");
/* Trusted real-mode envelope; request/buffer are caller-segment offsets. */
typedef struct {unsigned version,size,owner,epoch,request,buffer,operation;int result;unsigned handles;} K32IoPacket;
_Static_assert(sizeof(K32IoPacket)==36,"trusted broker envelope");
int k32_io_path(unsigned,const char *,unsigned,char [24]);
int k32_io_validate(const K32Io *);
typedef struct {int (*rename)(const char *,const char *);int (*remove)(const char *);} K32IoPublish;
int k32_io_publish(const char *,const char *,const char *,unsigned,const K32IoPublish *);
#endif
