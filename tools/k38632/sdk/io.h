#ifndef K32_IO_SDK_H
#define K32_IO_SDK_H
#include "k32x.h"
#include "../io.h"
typedef struct {unsigned exists,length,hash;} K32Revision;
int k32_io(K32Io *);
K32Io k32_io_name(unsigned,const char *);
int k32_io_open(const char *,unsigned,unsigned *);
int k32_io_read(unsigned,void *,unsigned);
int k32_io_seek(unsigned,unsigned);
int k32_io_close(unsigned);
int k32_io_revision(const char *,unsigned,K32Revision *);
int k32_io_begin(const char *,const K32Revision *,K32Io *);
int k32_io_write(K32Io *,const void *,unsigned);
int k32_io_commit(K32Io *);
int k32_io_abort(K32Io *,unsigned);
int k32_io_save(const char *,const void *,unsigned,const K32Revision *);
int k32_io_recover(const char *,unsigned,const char *,const K32Revision *);
int k32_getchar(void);
int k32_putchar(unsigned);
int k32_printf(const char *,...);
#endif
