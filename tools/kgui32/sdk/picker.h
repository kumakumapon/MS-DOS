#ifndef KG_PICKER_H
#define KG_PICKER_H
#include "kgui.h"
#include "widgets.h"
#define KP_ENTRIES 192
typedef struct {K32DirEntry entries[KP_ENTRIES];unsigned matches[KP_ENTRIES],count,cursor,loading,work,saving,document_bytes;K32WorkInfo location;KwInput filter,name;KwList list;KwFocus focus;KwScroll scroll;int error;} KpPicker;
void kp_workspace_info(K32WorkInfo *);
void kp_open(KpPicker *);
void kp_save(KpPicker *,const char *);
void kp_save_size(KpPicker *,const char *,unsigned);
void kp_close(KpPicker *);
int kp_tick(KpPicker *);
/* 1 selected regular file, -1 cancel, 0 continue. */
int kp_event(KpPicker *,const KgEvent *,char name[13]);
int kp_draw(KpPicker *);
#endif
