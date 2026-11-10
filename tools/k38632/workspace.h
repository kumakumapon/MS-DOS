/* SDK4: bounded workspace requests, no application DOS handles or pointers. */
#ifndef K32_WORKSPACE_H
#define K32_WORKSPACE_H
#define K32_FEATURE_SAVE 128u
#define K32_WORK_LIMIT 65536u
enum {K32_WORK_STAT=1,K32_WORK_OPEN,K32_WORK_FIRST,K32_WORK_SAVE,K32_WORK_INFO,
 K32_WORK_REC_INFO,K32_WORK_REC_OPEN_TMP,K32_WORK_REC_OPEN_BAK,
 K32_WORK_REC_RESTORE_TMP,K32_WORK_REC_RESTORE_BAK,
 K32_WORK_REC_FIRST_TMP,K32_WORK_REC_FIRST_BAK};
#define K32_WORK_CRLF 1u
enum {K32_WORK_HDD=1,K32_WORK_FD=2,K32_WORK_READY=4};
typedef struct {
 unsigned version,size,flags,drive; /* drive is zero-based, selected by broker */
 char root[16];
 unsigned sector_bytes,cluster_sectors,free_clusters,total_clusters;
 unsigned free_bytes,total_bytes,max_bytes;int error;
} K32WorkInfo;
_Static_assert(sizeof(K32WorkInfo)==64,"workspace information contract");
typedef struct {
 unsigned version,size,operation,flags;
 char name[16];
 unsigned data,bytes,exists,length,hash,reserved[3];
} K32Work;
_Static_assert(sizeof(K32Work)==64,"workspace wire contract");
typedef struct {unsigned exists,length,hash;int error;} K32WorkRevision;
typedef struct {unsigned version,size;K32WorkRevision file[3];unsigned reserved[2];} K32RecoveryInfo;
typedef struct {char destination[16];K32RecoveryInfo expected;} K32RecoveryRestore;
_Static_assert(sizeof(K32RecoveryInfo)==64,"recovery state contract");
_Static_assert(sizeof(K32RecoveryRestore)==80,"recovery request contract");
typedef struct {
 int (*stat)(const char *,unsigned *,unsigned *);
 int (*create)(const char *);
 int (*write)(int,const void *,unsigned);
 int (*close)(int);
 int (*rename)(const char *,const char *);
 int (*remove)(const char *);
 int (*copy)(unsigned,void *,unsigned);
 int (*space)(unsigned *,unsigned *); /* free bytes and allocation unit */
} K32WorkIO;
int k32_work_path(const char *,char [24]);
int k32_work_path_at(unsigned,const char *,char [24]);
int k32_work_space(K32WorkInfo *,unsigned,unsigned,unsigned,unsigned);
int k32_work_request(const K32Work *);
int k32_work_variant_at(unsigned,const char *,unsigned,char [24]);
int k32_recovery_info_at(unsigned,const char *,K32RecoveryInfo *,const K32WorkIO *);
int k32_recovery_restore_at(unsigned,const char *,unsigned,const K32RecoveryRestore *,const K32WorkIO *);
/* 0 committed; 1 committed with backup retained; negative = failure.
 * -117 means recovery files may remain. Never silently remove old backups. */
int k32_work_save(const K32Work *,const K32WorkIO *,unsigned char [4096]);
int k32_work_save_at(unsigned,const K32Work *,const K32WorkIO *,unsigned char [4096]);
#endif
