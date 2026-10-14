#include "hardware.h"

/* PC-98 16-color planar VRAM and CG-ROM; no embedded external font or TSR. */
static const unsigned segments[] = {0xa800, 0xb000, 0xb800, 0xe000};
static const u16 theme[] = {
    0x234, 0x256, 0xeff, 0xcde, 0x27b, 0x259, 0xfc6, 0xfff,
    0x123, 0xbcd, 0x678, 0x467, 0x367, 0x7ab, 0xeef, 0xacd,
};
static u8 fonts[256][16], palette[16][3], saved_mode, saved_index;
static int fonts_ready, cursor_visible, cursor_x, cursor_y, cursor_rows, cursor_bytes;
static u8 underneath[4][16][3];
static u8 saved_mouse_c;
static int mouse_active;
static Mouse mouse = {320, 200, 0, 0};
static Mouse events[16];
static unsigned event_head, event_count;
static int graphics_active, keys[128];
static unsigned key_head, key_count;
static void mouse_pump(void);
static Mouse read_raw(void);
static int bios_key_ready(void);

static void out(unsigned port, u8 value) {
    __asm__ volatile("outb %b0,%w1" :: "a"(value), "d"(port));
}
static u8 in(unsigned port) {
    u8 value;
    __asm__ volatile("inb %w1,%b0" : "=a"(value) : "d"(port));
    return value;
}
static void bios(unsigned a, unsigned c) {
    unsigned d = 0;
    __asm__ volatile("int $0x18" : "+a"(a), "+c"(c), "+d"(d)
                     :: "ebx", "esi", "edi", "cc", "memory");
}
static u8 get(unsigned segment, unsigned offset) {
    u8 value;
    __asm__ volatile("pushw %%es; movw %w1,%%es; movb %%es:(%2),%0; popw %%es"
                     : "=q"(value) : "r"(segment), "r"(offset) : "memory");
    return value;
}
static void put(unsigned segment, unsigned offset, u8 value) {
    __asm__ volatile("pushw %%es; movw %w0,%%es; movb %b2,%%es:(%1); popw %%es"
                     :: "r"(segment), "r"(offset), "q"(value) : "memory");
}
static void fill(unsigned segment, unsigned offset, unsigned length, u8 value) {
    __asm__ volatile("pushw %%es; movw %w3,%%es; rep stosb; popw %%es"
                     : "+D"(offset), "+c"(length) : "a"(value), "r"(segment) : "memory");
}
static void fill_words(unsigned offset, unsigned length, u16 value) {
    unsigned segment = 0xa000;
    __asm__ volatile("pushw %%es; movw %w3,%%es; rep stosw; popw %%es"
                     : "+D"(offset), "+c"(length) : "a"(value), "r"(segment) : "memory");
}
static void masked(unsigned segment, unsigned offset, u8 mask, int set) {
    u8 value = get(segment, offset);
    put(segment, offset, set ? value | mask : value & (u8)~mask);
}
void gfx_rect(int x, int y, int w, int h, int color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 640) w = 640 - x;
    if (y + h > 400) h = 400 - y;
    if (w <= 0 || h <= 0) return;
    int left = x / 8, right = (x + w - 1) / 8;
    u8 first = 255 >> (x & 7), last = 255 << (7 - ((x + w - 1) & 7));
    for (int p = 0; p < 4; p++) {
        int set = color & (1 << p);
        if (!(x & 7) && !(w & 7)) {
            if (w == 640) for (int row = y; row < y + h; row += 8) {
                int rows = y + h - row < 8 ? y + h - row : 8;
                fill(segments[p], row * 80, rows * 80, set ? 255 : 0); mouse_pump();
            }
            else for (int row = y; row < y + h; row++) {
                fill(segments[p], row * 80 + left, w / 8, set ? 255 : 0);
                if (!(row & 7)) mouse_pump();
            }
        } else for (int row = y; row < y + h; row++) {
            unsigned offset = row * 80;
            if (left == right) masked(segments[p], offset + left, first & last, set);
            else {
                masked(segments[p], offset + left, first, set);
                if (right - left > 1) fill(segments[p], offset + left + 1, right - left - 1, set ? 255 : 0);
                masked(segments[p], offset + right, last, set);
            }
            if (!(row & 7)) mouse_pump();
        }
    }
}
void gfx_cell(int x, int y, char ch, int foreground, int background, int selected) {
    static unsigned cells;
    if (x < 0 || x >= 80 || y < 0 || y >= 25) return;
    /* Mirror labels in text VRAM for accessibility/debugging; the visible
     * glyphs are rendered into graphics so the pointer can cover them cleanly. */
    p_cell(x, y, ch, selected);
    for (int p = 0; p < 4; p++) for (int row = 0; row < 16; row++) {
        u8 bits = fonts[(u8)ch][row];
        u8 value = (foreground & (1 << p) ? bits : 0) |
                   (background & (1 << p) ? (u8)~bits : 0);
        put(segments[p], (y * 16 + row) * 80 + x, value);
    }
    if (!(++cells & 7)) mouse_pump();
}
void gfx_clear_text(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 80) w = 80 - x;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h && row < 25; row++) {
        unsigned offset = (row * 80 + x) * 2;
        fill_words(offset, w, ' '); fill_words(0x2000 + offset, w, 0xe1);
        mouse_pump();
    }
}
void gfx_outline(int x, int y, int w, int h) {
    /* XOR the preview frame twice to remove it without repainting the scene. */
    int top = y * 16, bottom = (y + h) * 16 - 1, left = x, right = x + w - 1;
    for (int p = 0; p < 4; p++) {
        for (int col = left; col <= right; col++) {
            unsigned a = top * 80 + col, b = bottom * 80 + col;
            put(segments[p], a, get(segments[p], a) ^ 255);
            put(segments[p], b, get(segments[p], b) ^ 255);
        }
        for (int row = top + 1; row < bottom; row++) {
            unsigned a = row * 80 + left, b = row * 80 + right;
            put(segments[p], a, get(segments[p], a) ^ 0x80);
            put(segments[p], b, get(segments[p], b) ^ 1);
            if (!(row & 15)) mouse_pump();
        }
    }
}
static void pixel(int x, int y, int color) {
    if (x < 0 || x >= 640 || y < 0 || y >= 400) return;
    unsigned offset = y * 80 + x / 8, bit = 0x80 >> (x & 7);
    for (int p = 0; p < 4; p++) masked(segments[p], offset, bit, color & (1 << p));
}
void gfx_pointer_hide(void) {
    if (!cursor_visible) return;
    for (int p = 0; p < 4; p++) for (int row = 0; row < cursor_rows; row++)
        for (int col = 0; col < cursor_bytes; col++)
            put(segments[p], (cursor_y + row) * 80 + cursor_x / 8 + col, underneath[p][row][col]);
    cursor_visible = 0;
}
void gfx_pointer_show(int x, int y) {
    static const u16 edge[16] = {0x8000,0xc000,0xe000,0xf000,0xf800,0xfc00,0xfe00,0xff00,
        0xff80,0xffc0,0xfc00,0xdc00,0x8e00,0x0600,0x0700,0x0300};
    static const u16 body[16] = {0,0,0x4000,0x6000,0x7000,0x7800,0x7c00,0x7e00,
        0x7f00,0x7c00,0x5800,0x0800,0x0400,0x0200,0x0200,0};
    gfx_pointer_hide();
    cursor_x = x; cursor_y = y; cursor_rows = y > 384 ? 400 - y : 16;
    int last = x + 15 < 640 ? x + 15 : 639;
    cursor_bytes = last / 8 - x / 8 + 1;
    for (int p = 0; p < 4; p++) for (int row = 0; row < cursor_rows; row++)
        for (int col = 0; col < cursor_bytes; col++)
            underneath[p][row][col] = get(segments[p], (y + row) * 80 + x / 8 + col);
    for (int row = 0; row < cursor_rows; row++) for (int col = 0; col < 16 && x + col < 640; col++) {
        if (edge[row] & (0x8000 >> col)) pixel(x + col, y + row, body[row] & (0x8000 >> col) ? WHITE : INK);
    }
    cursor_visible = 1;
}
void mouse_start(void) {
    if (mouse_active) return;
    saved_mouse_c = in(0x7fdd);
    unsigned a = 0x93, d = 0x7fdf;
    /* Keep the two configuration writes atomic without changing the C stack
     * between pushf/popf. Polling uses no installed interrupt handler. */
    __asm__ volatile("pushf; cli; outb %%al,%%dx; movw $0x7fdd,%%dx;"
                     "movb $0x10,%%al; outb %%al,%%dx; popf"
                     : "+a"(a), "+d"(d) :: "cc", "memory");
    mouse_active = 1;
    read_raw(); event_head = 0; event_count = 0;
}
void mouse_stop(void) {
    if (!mouse_active) return;
    out(0x7fdd, saved_mouse_c);
    mouse_active = 0;
    event_count = 0;
}
static Mouse read_raw(void) {
    /* Rising HOLD latches/resets X/Y; select low/high nibbles without
     * releasing HOLD. Button bits are active low (left=80h, right=20h). */
    out(0x7fdd, 0x90); u8 first = in(0x7fd9), x = first & 15;
    out(0x7fdd, 0xb0); x |= (in(0x7fd9) & 15) << 4;
    out(0x7fdd, 0xd0); u8 y = in(0x7fd9) & 15;
    out(0x7fdd, 0xf0); y |= (in(0x7fd9) & 15) << 4;
    out(0x7fdd, 0x10);
    mouse.x += (signed char)x; mouse.y += (signed char)y;
    if (mouse.x < 0) mouse.x = 0;
    if (mouse.x > 639) mouse.x = 639;
    if (mouse.y < 0) mouse.y = 0;
    if (mouse.y > 399) mouse.y = 399;
    mouse.buttons = (first & 0x80 ? 0 : 1) | (first & 0x20 ? 0 : 2);
    return mouse;
}
static void mouse_pump(void) {
    /* Paints take longer than a BIOS key-buffer slot. Collect input while
     * drawing, then let the desktop apply a batch before its next repaint. */
    if (graphics_active) while (key_count < 128 && bios_key_ready()) {
        keys[(key_head + key_count) & 127] = p_key(); key_count++;
    }
    if (!mouse_active) return;
    int previous = mouse.buttons;
    Mouse current = read_raw();
    if (previous == current.buttons) return;
    current.ticks = input_ticks();
    /* Bound the queue, retaining the newest state (including release) if a
     * pathological burst exceeds it. Normal UI painting drains every frame. */
    if (event_count == 16) { event_head = (event_head + 1) & 15; event_count--; }
    events[(event_head + event_count) & 15] = current; event_count++;
}
Mouse mouse_read(void) {
    mouse_pump();
    if (event_count) {
        Mouse result = events[event_head]; event_head = (event_head + 1) & 15; event_count--;
        return result;
    }
    mouse.ticks = input_ticks(); return mouse;
}
int mouse_queued(void) { return event_count != 0; }
static int bios_key_ready(void) {
    unsigned a = 0x0100, b = 0;
    __asm__ volatile("int $0x18" : "+a"(a), "+b"(b) :: "cc", "memory");
    return !!(b & 0xff00);
}
int key_ready(void) { return key_count || bios_key_ready(); }
int key_read(void) {
    if (!key_count) return p_key();
    int result = keys[key_head]; key_head = (key_head + 1) & 127; key_count--;
    return result;
}
u32 input_ticks(void) {
    /* The PC-98 ARTIC counter advances at 307200 Hz and wraps at 24 bits.
     * DOS CLOCK$ supplies whole seconds, which cannot time double clicks. */
    u8 high, middle, low;
    do { high = in(0x5f); middle = in(0x5d); low = in(0x5c); }
    while (middle != in(0x5d) || high != in(0x5f));
    return (u32)high << 16 | (u32)middle << 8 | low;
}
void input_idle(void) {
    /* A keyboard-only BIOS may leave timer IRQs masked. HLT would then stop
     * mouse polling until a key is pressed. Pace polling with ARTIC instead. */
    u32 start = input_ticks();
    while (((input_ticks() - start) & 0xffffff) < 3072)
        __asm__ volatile("nop");
}
void gfx_start(void) {
    saved_mode = in(0x6a) & 1;
    out(0x6a, 1); saved_index = in(0xa8);
    for (int i = 0; i < 16; i++) {
        out(0xa8, i); palette[i][0] = in(0xaa); palette[i][1] = in(0xac); palette[i][2] = in(0xae);
    }
    p_screen(); bios(0x4200, 0xc000); bios(0x4000, 0); out(0x6a, 1);
    for (int i = 0; i < 16; i++) {
        out(0xa8, i); out(0xaa, (theme[i] >> 4) & 15); out(0xac, (theme[i] >> 8) & 15); out(0xae, theme[i] & 15);
    }
    if (!fonts_ready) {
        out(0xa1, 0);
        for (int ch = 32; ch < 256; ch++) {
            out(0xa3, ch);
            for (int row = 0; row < 16; row++) { out(0xa5, row); fonts[ch][row] = in(0xa9); }
        }
        out(0xa1, 0); out(0xa3, 0); out(0xa5, 0); fonts_ready = 1;
    }
    bios(0x0d00, 0); /* Graphics glyphs replace the visible text layer. */
    graphics_active = 1;
    mouse_start();
}
void gfx_stop(void) {
    graphics_active = 0;
    gfx_pointer_hide(); mouse_stop();
    bios(0x4100, 0);
    for (int i = 0; i < 16; i++) {
        out(0xa8, i); out(0xaa, palette[i][0]); out(0xac, palette[i][1]); out(0xae, palette[i][2]);
    }
    out(0xa8, saved_index); out(0x6a, saved_mode ? 1 : 0); bios(0x0c00, 0);
}
