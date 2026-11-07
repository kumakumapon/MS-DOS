/* SDK4: bounded workspace requests, no application DOS handles or pointers. */
#ifndef K32_WORKSPACE_H
#define K32_WORKSPACE_H
#define K32_FEATURE_SAVE 128u
#define K32_WORK_LIMIT 65536u
enum {K32_WORK_STAT=1,K32_WORK_OPEN,K32_WORK_FIRST,K32_WORK_SAVE};
typedef struct {
 unsigned version,size,operation,flags;
 char name[16];
 unsigned data,bytes,exists,length,hash,reserved[3];
} K32Work;
_Static_assert(sizeof(K32Work)==64,"workspace wire contract");
typedef struct {
 int (*stat)(const char *,unsigned *,unsigned *);
 int (*create)(const char *);
 int (*write)(int,const void *,unsigned);
 int (*close)(int);
 int (*rename)(const char *,const char *);
 int (*remove)(const char *);
 int (*copy)(unsigned,void *,unsigned);
} K32WorkIO;
int k32_work_path(const char *,char [24]);
int k32_work_request(const K32Work *);
/* 0 committed; 1 committed with backup retained; negative = failure.
 * -117 means recovery files may remain. Never silently remove old backups. */
int k32_work_save(const K32Work *,const K32WorkIO *,unsigned char [4096]);
#endif
