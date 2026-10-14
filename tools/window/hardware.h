#ifndef KDESK_HARDWARE_H
#define KDESK_HARDWARE_H
#include "../filer/filer.h"

enum { INK, DESKTOP, PAPER, SURFACE, ACCENT, SELECTED, GOLD, WHITE,
       SHADOW, INACTIVE, MUTED, BORDER, DESKTOP_LIGHT, HANDLE, BUTTON, PRESSED };
typedef struct { int x, y, buttons; u32 ticks; } Mouse;

void gfx_start(void);
void gfx_stop(void);
void gfx_rect(int x, int y, int w, int h, int color);
void gfx_cell(int x, int y, char ch, int foreground, int background, int selected);
void gfx_clear_text(int x, int y, int w, int h);
void gfx_pointer_show(int x, int y);
void gfx_pointer_hide(void);
void gfx_outline(int x, int y, int w, int h);
void mouse_start(void);
void mouse_stop(void);
Mouse mouse_read(void);
int mouse_queued(void);
int key_ready(void);
int key_read(void);
void input_idle(void);
u32 input_ticks(void);
#endif
