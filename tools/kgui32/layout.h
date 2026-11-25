#ifndef KGUI32_LAYOUT_H
#define KGUI32_LAYOUT_H
#include "scene.h"

enum { LAYOUT_APPS=16, LAYOUT_MAGIC=0x31594c4b };
typedef struct { char program[16]; Rect rect; unsigned mode; } AppLayout;
typedef struct {
 unsigned magic,version,panes,next;
 Rect pane[2];
 AppLayout apps[LAYOUT_APPS];
} WindowLayout;

int layout_validate(WindowLayout *);
int layout_restore(const WindowLayout *,Scene *,int,const char *);
int layout_remember(WindowLayout *,const Scene *,int,const char *);
#endif
