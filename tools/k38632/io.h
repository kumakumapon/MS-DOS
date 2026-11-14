#ifndef K32_COMMON_IO_H
#define K32_COMMON_IO_H
#define K32_FEATURE_IO 256u
#define K32_IO_LIMIT 1048576u
enum {K32_IO_STAT=1,K32_IO_OPEN,K32_IO_READ,K32_IO_SEEK,K32_IO_CLOSE,
 K32_IO_BEGIN,K32_IO_WRITE,K32_IO_COMMIT,K32_IO_ABORT,K32_IO_RECOVER};
typedef struct {
 unsigned version,size,operation,flags;char name[16];
 unsigned token,data,bytes,offset,exists,length,hash,reserved;
} K32Io;
_Static_assert(sizeof(K32Io)==64,"common file request");
/* Trusted real-mode envelope; request/buffer are caller-segment offsets. */
typedef struct {unsigned version,size,owner,epoch,request,buffer,operation;int result;unsigned handles;} K32IoPacket;
_Static_assert(sizeof(K32IoPacket)==36,"trusted broker envelope");
int k32_io_path(unsigned,const char *,unsigned,char [24]);
int k32_io_validate(const K32Io *);
typedef struct {int (*rename)(const char *,const char *);int (*remove)(const char *);} K32IoPublish;
int k32_io_publish(const char *,const char *,const char *,unsigned,const K32IoPublish *);
#endif
