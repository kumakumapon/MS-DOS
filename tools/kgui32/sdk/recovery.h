#ifndef KG_RECOVERY_H
#define KG_RECOVERY_H
#include "picker.h"
#define KR_ENTRIES 96
/* Private app state; no new native wire size or execution slot. */
typedef struct {
 char names[KR_ENTRIES][13],base[13],restored[13];
 unsigned count,cursor,stage,loading,mode,kind,bytes,top,lines,rows,cols,changed,preview_valid;
 unsigned offsets[65537];char *preview;
 KwList list;KwInput name;KwFocus focus;
 K32WorkInfo location;K32RecoveryInfo info;K32RecoveryRestore request;
 int error;const char *note;
} KrRecovery;
void kr_open(KrRecovery *,char *);
void kr_close(KrRecovery *);
int kr_tick(KrRecovery *);
/* 1 closes, zero continues. No live Editor document is passed or changed. */
int kr_event(KrRecovery *,const KgEvent *);
int kr_draw(KrRecovery *);
#endif
