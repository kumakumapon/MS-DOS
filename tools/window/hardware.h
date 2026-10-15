#ifndef KDESK_HARDWARE_H
#define KDESK_HARDWARE_H
#include "../filer/filer.h"

enum { INK, DESKTOP, PAPER, SURFACE, ACCENT, SELECTED, GOLD, WHITE,
       SHADOW, INACTIVE, MUTED, BORDER, DESKTOP_LIGHT, HANDLE, BUTTON, PRESSED };
typedef struct { int x, y, buttons; u32 ticks; } Mouse;

unsigned mem_alloc(u32 bytes);
void mem_free(unsigned segment);
u8 mem_get(unsigned segment, u32 offset);
void mem_put(unsigned segment, u32 offset, u8 value);
void mem_copy(unsigned dst, u32 to, unsigned src, u32 from, u32 length);
void mem_fill(unsigned dst, u32 offset, u32 length, u8 color);
int gfx_colors(void);
u32 gfx_rgb(int index);
int gfx_nearest(int r, int g, int b);
void gfx_origin(int x, int y);
void gfx_pixel(int x, int y, int color);
void gfx_read_span(int x, int y, unsigned segment, u32 offset, unsigned count);
void gfx_write_span(int x, int y, unsigned segment, u32 offset, unsigned count);
void gfx_background(void);
int gfx_drag_begin(int x, int y, int w, int h);
void gfx_drag_move(int x, int y);
void gfx_drag_end(void);
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
void input_poll(void);
void input_idle(void);
u32 input_ticks(void);
#endif
