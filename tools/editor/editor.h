#ifndef EDITOR_H
#define EDITOR_H
#include "../filer/filer.h"
#include "../filer/sjis.h"
#define ED_CAP 16384
#define ED_BAD_TEXT 1000
#define ED_RECOVERY 1001
#define ED_BACKUP_KEPT 1002
typedef struct {
    u8 data[ED_CAP];
    int length, cursor, dirty, desired, undo_valid, read_only;
    char path[FD_PATH], recovery[FD_PATH];
} Editor;
void ed_init(Editor *e);
int ed_next(const Editor *e, int at);
int ed_prev(const Editor *e, int at);
int ed_start(const Editor *e, int at);
int ed_end(const Editor *e, int at);
int ed_width(const Editor *e, int at, int col);
int ed_column(const Editor *e);
void ed_vertical(Editor *e, int direction);
int ed_insert(Editor *e, const u8 *text, int count);
void ed_delete(Editor *e, int backwards);
void ed_delete_line(Editor *e);
void ed_undo(Editor *e);
int ed_load(Editor *e, const char *path);
int ed_save(Editor *e);
#endif
