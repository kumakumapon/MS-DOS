#include "../filer/sjis.h"
#include "hardware.h"
#include "../display/cirrus.h"

/* PC-98 planar / PC-9821 PEGC VRAM and CG-ROM; no external font or TSR. */
static const unsigned segments[] = {0xa800, 0xb000, 0xb800, 0xe000};
static const u16 theme[] = {
    0x223, 0x123, 0xfff, 0xdef, 0x36a, 0x246, 0xfb6, 0xfff,
    0x112, 0xabc, 0x678, 0x456, 0x234, 0x7bc, 0xeee, 0xbcd,
};
static unsigned font_segment,palette_segment;
static int high_requested,high,wallpaper_ready;
int gfx_width(void){return high?800:640;}
int gfx_height(void){return high?600:400;}
void gfx_highcolor(int enabled){high_requested=enabled;}


static u8 palette[16][3], saved_mode, saved_index;
static int packed, bank = -1, origin_x, origin_y, offscreen;
static u32 high_base(void){return offscreen?0x100000ul:0;}
static u16 pixel16(int color){
    if(color&0x10000)return (u16)color;
    u32 rgb=gfx_rgb(color);return ((rgb>>19)<<11)|(((rgb>>10)&63)<<5)|((rgb>>3)&31);
}
static u8 saved_gate, saved_256, saved_banks[4], saved_pegc_mode, saved_pegc_enable;
/* The cursor uses only one video format at a time. */
static union { u16 packed[16][16];u8 planar[4][16][3]; } cursor_under;
#define packed_under cursor_under.packed
#define underneath cursor_under.planar
static unsigned drag_sprite;
static int sprite_x, sprite_y, sprite_w, sprite_h;
static int fonts_ready, cursor_visible, cursor_x, cursor_y, cursor_rows, cursor_bytes;
static u8 saved_mouse_c;
static int mouse_active;
static Mouse mouse = {320, 200, 0, 0};
static Mouse events[16];
static unsigned event_head, event_count;
static int graphics_active;static u16 keys[128];
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
static int query(u8 selector) { out(0x9a0, selector); return in(0x9a0) & 1; }
static void select_bank(int n) {
    n += offscreen ? 8 : 0;
    if (bank != n) { put(0xe000, 4, n); put(0xe000, 5, 0); bank = n; }
}
int gfx_colors(void) { return high?65536:packed?256:16; }
void gfx_origin(int x, int y) { origin_x = x; origin_y = y; }
u32 gfx_rgb(int index) {
    if (index < 16) {
        unsigned c = theme[index];
        return ((c >> 8) * 17u << 16) | (((c >> 4) & 15) * 17u << 8) | ((c & 15) * 17u);
    }
    if (index < 232) {
        index -= 16;
        return (index / 36 * 51u << 16) | (index / 6 % 6 * 51u << 8) | (index % 6 * 51u);
    }
    unsigned t = index - 232;
    return ((22 + t) << 16) | ((32 + t * 2) << 8) | (52 + t * 3);
}
int gfx_nearest(int r, int g, int b) {
    int found = packed ? 16 + (r + 25) / 51 * 36 + (g + 25) / 51 * 6 + (b + 25) / 51 : 0;
    u32 initial = gfx_rgb(found);
    int dr = r - (int)(initial >> 16), dg = g - (int)((initial >> 8) & 255), db = b - (int)(initial & 255);
    unsigned best = dr * dr + dg * dg + db * db;
    for (int j = 0; j < (packed ? 40 : 16); j++) {
        int i = j < 16 ? j : j + 216;
        u32 rgb = gfx_rgb(i); int dr = r - (int)(rgb >> 16), dg = g - (int)((rgb >> 8) & 255), db = b - (int)(rgb & 255);
        unsigned distance = dr * dr + dg * dg + db * db;
        if (distance < best) { best = distance; found = i; }
    }
    return found;
}
static void packed_fill(u32 at, unsigned count, u8 color) {
    while (count) {
        unsigned n = 32768 - (at & 32767); if (n > count) n = count;
        select_bank(at >> 15); fill(0xa800, at & 32767, n, color);
        at += n; count -= n;
    }
}
void gfx_wallpaper(void){
    if(!high)return;
    if(!wallpaper_ready){
        u16 pixels[800];unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
        int work=(gfx_height()/16-2)*16;
        for(int y=0;y<gfx_height();y++){
            for(int x=0;x<gfx_width();x+=8){int r=12+x*50/gfx_width(),g=24+y*80/work,b=48+(x+y)*110/(gfx_width()+work);
                u16 color=((r>>3)<<11)|((g>>2)<<5)|(b>>3);for(int n=0;n<8;n++)pixels[x+n]=color;}
            cirrus_write(0x200000ul+(u32)y*gfx_width()*2,ds,(unsigned)pixels,gfx_width()*2);
            if(!(y&15))mouse_pump();
        }wallpaper_ready=1;
    }
    if(!cirrus_blit(0x200000ul,high_base(),gfx_width()*2,gfx_height(),gfx_width()*2)){
        u16 pixels[800];unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
        for(int y=0;y<gfx_height();y++){u32 at=(u32)y*gfx_width()*2;
            cirrus_read(0x200000ul+at,ds,(unsigned)pixels,gfx_width()*2);cirrus_write(high_base()+at,ds,(unsigned)pixels,gfx_width()*2);}
    }
}
void gfx_read_span(int x, int y, unsigned segment, u32 offset, unsigned count) {
    if(high){cirrus_read(((u32)y*gfx_width()+x)*2+high_base(),segment+offset/16,offset&15,count*2);return;}
    u32 at = y * 640u + x;
    while (count) {
        unsigned n = 32768 - (at & 32767); if (n > count) n = count;
        select_bank(at >> 15); mem_copy(segment, offset, 0xa800, at & 32767, n);
        at += n; offset += n; count -= n;
    }
}
void gfx_write_span(int x, int y, unsigned segment, u32 offset, unsigned count) {
    x += origin_x; y += origin_y;
    if(high){cirrus_write(((u32)y*gfx_width()+x)*2+high_base(),segment+offset/16,offset&15,count*2);return;}
    u32 at = y * 640u + x;
    while (count) {
        unsigned n = 32768 - (at & 32767); if (n > count) n = count;
        select_bank(at >> 15); mem_copy(0xa800, at & 32767, segment, offset, n);
        at += n; offset += n; count -= n;
    }
}
/* Page 1 is the composition target; page 0 remains visible throughout paint.
 * With offscreen disabled, the same transfer restores the live-drag backdrop. */
static void restore_background(int x, int y, int w, int h);
void gfx_target(int buffered) { offscreen = packed && buffered; }
void gfx_present(int x, int y, int w, int h) {
    offscreen = 0;
    if (packed) restore_background(x, y, w, h);
}
static void restore_background(int x, int y, int w, int h) {
    if(high){
        u32 first=((u32)y*gfx_width()+x)*2;
        if(cirrus_blit(first+0x100000ul,first,w*2,h,gfx_width()*2))return;
        u16 rowbuf[800];unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
        for(int row=0;row<h;row++){u32 at=((u32)(y+row)*gfx_width()+x)*2;
            cirrus_read(at+0x100000ul,ds,(unsigned)rowbuf,w*2);cirrus_write(at,ds,(unsigned)rowbuf,w*2);
            if(!(row&15))mouse_pump();}return;
    }
    for (int row = 0; row < h; row++) {
        u32 at = (y + row) * 640u + x; unsigned count = w;
        while (count) {
            unsigned n = 32768 - (at & 32767); if (n > count) n = count;
            select_bank((at >> 15) + 8); put(0xe000, 6, at >> 15); put(0xe000, 7, 0);
            mem_copy(0xb000, at & 32767, 0xa800, at & 32767, n);
            at += n; count -= n;
        }
        if (!(row & 15)) mouse_pump();
    }
}
void gfx_drag_end(void) { mem_free(drag_sprite); drag_sprite = 0; }
int gfx_drag_begin(int x, int y, int w, int h) {
    gfx_drag_end();
    if (!packed) return 0;
    sprite_x = x; sprite_y = y; sprite_w = w - 5; sprite_h = h - 5;
    w = sprite_w; h = sprite_h;
    drag_sprite = mem_alloc(w * (u32)h * (high?2:1));
    if (!drag_sprite) return 0;
    for (int row = 0; row < h; row++) {
        gfx_read_span(x, y + row, drag_sprite, row * (u32)w*(high?2:1), w);
        if (!(row & 15)) mouse_pump();
    }
    return 1;
}
void gfx_drag_move(int x, int y) {
    if (!drag_sprite || (x == sprite_x && y == sprite_y)) return;
    /* The 600-line mode has eight pixels below its 37 complete text rows.
     * Keep drag shadows above the status/taskbar rows, just like the layout. */
    int bottom = (gfx_height()/16-2)*16;
    int old_h = sprite_h + 5;
    if (sprite_y + old_h > bottom) old_h = bottom - sprite_y;
    restore_background(sprite_x, sprite_y, sprite_w + 5, old_h);
    int side_h = sprite_h;
    if (y + 5 + side_h > bottom) side_h = bottom - y - 5;
    int bottom_h = bottom - y - sprite_h; if (bottom_h > 5) bottom_h = 5;
    gfx_rect(x + sprite_w, y + 5, 5, side_h, SHADOW);
    gfx_rect(x + 5, y + sprite_h, sprite_w, bottom_h, SHADOW);
    for (int row = 0; row < sprite_h; row++) {
        gfx_write_span(x, y + row, drag_sprite, row * (u32)sprite_w*(high?2:1), sprite_w);
        if (!(row & 15)) mouse_pump();
    }
    sprite_x = x; sprite_y = y;
}
void gfx_rect(int x, int y, int w, int h, int color) {
    x += origin_x; y += origin_y;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > gfx_width()) w = gfx_width() - x;
    if (y + h > gfx_height()) h = gfx_height() - y;
    if (w <= 0 || h <= 0) return;
    if(high){for(int row=y;row<y+h;row++){cirrus_fill(((u32)row*gfx_width()+x)*2+high_base(),w,pixel16(color));if(!(row&7))mouse_pump();}return;}
    if (packed) {
        for (int row = y; row < y + h; row++) {
            packed_fill(row * 640u + x, w, color);
            if (!(row & 7)) mouse_pump();
        }
        return;
    }
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
    if ((u8)ch < 32) ch = '?';
    u8 extra[16]; const u8 *glyph;
    if ((u8)ch > 126 || !font_segment) {
        /* Keep half-width kana/extended file names without reserving a full
         * 4 KiB font table in the COM's code/data/stack segment. */
        out(0xa1,0);out(0xa3,(u8)ch);
        for(int row=0;row<16;row++){out(0xa5,row);extra[row]=in(0xa9);}
        out(0xa3,0);out(0xa5,0);glyph=extra;
    } else {u16 ds;__asm__ volatile("movw %%ds,%0":"=r"(ds));mem_copy(ds,(unsigned)extra,font_segment,((u8)ch-32)*16,16);glyph=extra;}
    /* A heavier title/active-button face from the original CG-ROM bitmap. */
    if(background==ACCENT)for(int row=0;row<16;row++)extra[row]|=extra[row]>>1;
    if (x < 0 || x >= gfx_width()/8 || y < 0 || y >= gfx_height()/16) return;
    /* Mirror labels in text VRAM for accessibility/debugging; the visible
     * glyphs are rendered into graphics so the pointer can cover them cleanly. */
    int mirror=y;
    if(high&&y>=gfx_height()/16-2)mirror=y-(gfx_height()/16-25);
    if(x<80&&mirror<25)p_cell(x,mirror,ch,selected);
    if(high){
        u16 pixels[8];unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
        u16 fg=pixel16(foreground),bg=pixel16(background);
        for(int row=0;row<16;row++){for(int col=0;col<8;col++)pixels[col]=glyph[row]&(0x80>>col)?fg:bg;
            cirrus_write(((u32)(y*16+origin_y+row)*gfx_width()+x*8+origin_x)*2+high_base(),ds,(unsigned)pixels,16);}
        if(!(++cells&7))mouse_pump();
        return;
    }
    if (packed) {
        for (int row = 0; row < 16; row++) {
            u32 at = (y * 16 + origin_y + row) * 640u + x * 8 + origin_x;
            for (int col = 0; col < 8; col++) {
                select_bank(at >> 15);
                put(0xa800, at & 32767, glyph[row] & (0x80 >> col) ? foreground : background); at++;
            }
        }
        if (!(++cells & 7)) mouse_pump();
        return;
    }
    for (int p = 0; p < 4; p++) for (int row = 0; row < 16; row++) {
        u8 bits = glyph[row];
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
    if(high){
        top+=origin_y;bottom+=origin_y;left=left*8+origin_x;right=(right+1)*8-1+origin_x;
        for(int row=top;row<=bottom;row++){int step=row==top||row==bottom?1:right-left;
            for(int col=left;col<=right;col+=step){u32 at=((u32)row*gfx_width()+col)*2+high_base();cirrus_fill(at,1,cirrus_pixel(at)^65535);}}
        return;
    }
    if (packed) {
        top += origin_y; bottom += origin_y;
        left = left * 8 + origin_x; right = (right + 1) * 8 - 1 + origin_x;
        for (int row = top; row <= bottom; row++) {
            int step = row == top || row == bottom ? 1 : right - left;
            for (int col = left; col <= right; col += step) {
                u32 at = row * 640u + col;
                select_bank(at >> 15);
                put(0xa800, at & 32767, get(0xa800, at & 32767) ^ 255);
            }
            if (!(row & 15)) mouse_pump();
        }
        return;
    }
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
void gfx_pixel(int x, int y, int color) {
    if (x < 0 || x >= gfx_width() || y < 0 || y >= gfx_height()) return;
    if(high){cirrus_fill(((u32)y*gfx_width()+x)*2+high_base(),1,pixel16(color));return;}
    if (packed) { u32 at = y * 640u + x; select_bank(at >> 15); put(0xa800, at & 32767, color); return; }
    unsigned offset = y * 80 + x / 8, bit = 0x80 >> (x & 7);
    for (int p = 0; p < 4; p++) masked(segments[p], offset, bit, color & (1 << p));
}
void gfx_pointer_hide(void) {
    if (!cursor_visible) return;
    if (packed) {
        for (int row = 0; row < cursor_rows; row++) for (int col = 0; col < cursor_bytes; col++)
            gfx_pixel(cursor_x + col, cursor_y + row, high?0x10000|packed_under[row][col]:packed_under[row][col]);
        cursor_visible = 0; return;
    }
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
    cursor_x = x; cursor_y = y; cursor_rows = y > gfx_height()-16 ? gfx_height() - y : 16;
    int last = x + 15 < gfx_width() ? x + 15 : gfx_width()-1;
    cursor_bytes = packed ? last - x + 1 : last / 8 - x / 8 + 1;
    if (packed) for (int row = 0; row < cursor_rows; row++) for (int col = 0; col < cursor_bytes; col++) {
        u32 at = (y + row) * 640u + x + col; if(!high)select_bank(at >> 15);
        packed_under[row][col] = high?cirrus_pixel(((u32)(y+row)*gfx_width()+x+col)*2):get(0xa800, at & 32767);
    }
    if (!packed)
    for (int p = 0; p < 4; p++) for (int row = 0; row < cursor_rows; row++)
        for (int col = 0; col < cursor_bytes; col++)
            underneath[p][row][col] = get(segments[p], (y + row) * 80 + x / 8 + col);
    for (int row = 0; row < cursor_rows; row++) for (int col = 0; col < 16 && x + col < gfx_width(); col++) {
        if (edge[row] & (0x8000 >> col)) gfx_pixel(x + col, y + row, body[row] & (0x8000 >> col) ? WHITE : INK);
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
    if (mouse.x >= gfx_width()) mouse.x = gfx_width()-1;
    if (mouse.y < 0) mouse.y = 0;
    if (mouse.y >= gfx_height()) mouse.y = gfx_height()-1;
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
void input_poll(void) { mouse_pump(); }
Mouse mouse_read(void) {
    mouse_pump();
    if (event_count) {
        Mouse result = events[event_head]; event_head = (event_head + 1) & 15; event_count--;
        return result;
    }
    mouse.ticks = input_ticks(); return mouse;
}
int mouse_queued(void) { return event_count != 0; }
Mouse mouse_position(void) { return mouse; }
void mouse_position_set(int x, int y) {
    if (x >= 0 && x < gfx_width() && y >= 0 && y < gfx_height()) { mouse.x = x; mouse.y = y; }
}
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
    origin_x = 0; origin_y = 0; packed = 0; offscreen = 0;
    out(0x9a0, 11); int has_pegc = (in(0x9a0) & 0xfd) == 1;
    saved_mode = has_pegc ? query(4) : in(0x6a) & 1;
    saved_gate = has_pegc ? query(8) : 0; saved_256 = has_pegc ? query(10) : 0;
    saved_index = in(0xa8);
    int available = 0;
    if (has_pegc) {
        out(0x6a, 7); out(0x6a, 0x21); available = query(10);
        if(available&&!palette_segment)palette_segment=mem_alloc(256*3);
        if(!palette_segment)available=0;
        if (available) {
            for (int i = 0; i < 256; i++) {
                out(0xa8,i);mem_put(palette_segment,i*3,in(0xaa));mem_put(palette_segment,i*3+1,in(0xac));mem_put(palette_segment,i*3+2,in(0xae));
            }
            for (int i = 0; i < 4; i++) saved_banks[i] = get(0xe000, 4 + i);
            saved_pegc_mode = get(0xe000, 0x100); saved_pegc_enable = get(0xe000, 0x102);
        }
        out(0x6a, 0x20);
    }
    out(0x6a, 1);
    for (int i = 0; i < 16; i++) {
        out(0xa8, i); palette[i][0] = in(0xaa); palette[i][1] = in(0xac); palette[i][2] = in(0xae);
    }
    p_screen(); bios(0x4200, 0xc000); bios(0x4000, 0); out(0x6a, 1);
    for (int i = 0; i < 16; i++) {
        out(0xa8, i); out(0xaa, (theme[i] >> 4) & 15); out(0xac, (theme[i] >> 8) & 15); out(0xae, theme[i] & 15);
    }
    if (available) {
        out(0x6a, 7); out(0x6a, 0x21); packed = 1;
        put(0xe000, 0x100, 0); put(0xe000, 0x102, 1); bank = -1;
        for (int i = 0; i < 256; i++) {
            u32 c = gfx_rgb(i); out(0xa8, i); out(0xaa, (c >> 8) & 255); out(0xac, c >> 16); out(0xae, c & 255);
        }
    }
    if(!font_segment)font_segment=mem_alloc(95*16);
    if (!fonts_ready && font_segment) {
        out(0xa1, 0);
        for (int ch = 32; ch < 127; ch++) {
            out(0xa3, ch);
            for (int row = 0; row < 16; row++) { out(0xa5, row); mem_put(font_segment,(ch-32)*16+row,in(0xa9)); }
        }
        out(0xa1, 0); out(0xa3, 0); out(0xa5, 0); fonts_ready = 1;
    }
    bios(0x0d00, 0); /* Graphics glyphs replace the visible text layer. */
    if(high_requested&&cirrus_open(800,600)){high=1;packed=1;wallpaper_ready=0;}
    graphics_active = 1;
    mouse_start();
}
void gfx_stop(void) {
    graphics_active = 0; offscreen = 0;
    gfx_pointer_hide(); mouse_stop(); gfx_drag_end();
    if(high){cirrus_close();high=0;}
    if (packed) {
        for (int i = 0; i < 256; i++) {
            out(0xa8, i); out(0xaa,mem_get(palette_segment,i*3));out(0xac,mem_get(palette_segment,i*3+1));out(0xae,mem_get(palette_segment,i*3+2));
        }
        for (int i = 0; i < 4; i++) put(0xe000, 4 + i, saved_banks[i]);
        put(0xe000, 0x100, saved_pegc_mode); put(0xe000, 0x102, saved_pegc_enable);
        out(0x6a, 7); out(0x6a, 0x20); packed = 0;
    }
    bios(0x4100, 0); out(0x6a, 1);
    for (int i = 0; i < 16; i++) {
        out(0xa8, i); out(0xaa, palette[i][0]); out(0xac, palette[i][1]); out(0xae, palette[i][2]);
    }
    if (saved_256) { out(0x6a, 7); out(0x6a, 0x21); }
    out(0x6a, saved_gate ? 7 : 6);
    out(0xa8, saved_index); out(0x6a, saved_mode ? 1 : 0); bios(0x0c00, 0);
}

void gfx_pair(int x,int y,u8 lead,u8 trail){
    unsigned code=sjis_vram(lead,trail)&0x7fff;
    out(0xa1,code>>8);out(0xa3,code);
    u8 glyph[32];
    for(int half=0;half<2;half++)for(int row=0;row<16;row++){out(0xa5,row+(half?0:32));glyph[half*16+row]=in(0xa9);}
    out(0xa1,0);out(0xa3,0);out(0xa5,0);
    for(int row=0;row<16;row++)for(int col=0;col<16;col++)gfx_pixel(x*8+col,y*16+row,glyph[col/8*16+row]&(128>>(col%8))?INK:PAPER);
    p_pair(x,y,lead,trail,0);
}
