#include "bitmap.h"
#include "clipboard.h"
#ifndef VIEW_ONLY
#define VIEW_ONLY 0
#endif

enum { PENCIL, LINE, RECT, FILL, PICK, ERASE, SELECT };
static Bitmap image, undo, copied;
static char path[FD_PATH], owned[FD_PATH], status[80] = "Ready";
static int dirty, tool, color = 196, zoom, painting, sx, sy, px, py, has_selection, x1, y1, x2, y2;
static int last_x, last_y;
static u8 scaled[640];
static const char *tool_names[] = {"Pencil", "Line", "Rect", "Fill", "Pick", "Erase", "Select"};
static void message(const char *s) { fd_text(status, s, sizeof status); }
static void label(int x, int y, const char *s, int fg, int bg) {
    for (int i = 0; s[i] && x + i < 80; i++) gfx_cell(x + i, y, s[i], fg, bg, 0);
}
static void button(int x, int y, int width, const char *s, int pressed) {
    gfx_rect(x * 8, y * 16, width * 8 - 2, 16, pressed ? SELECTED : BUTTON);
    label(x + 1, y, s, pressed ? WHITE : INK, pressed ? SELECTED : BUTTON);
}
static unsigned local_segment(void) { u16 segment; __asm__ volatile("movw %%ds,%0" : "=r"(segment)); return segment; }
static void publish(void) { clip_image(image.segment, image.width, image.height); }
static void chrome(void) {
    gfx_rect(0, 0, 640, 64, SURFACE); gfx_clear_text(0, 0, 80, 4);
    label(1, 0, VIEW_ONLY ? "KVIEW - Image viewer 1.0" : "KPAINT - Image editor 1.0", WHITE, ACCENT);
    label(35, 0, path, INK, SURFACE); if (dirty) label(78, 0, "*", INK, SURFACE);
    label(1, 1, VIEW_ONLY ? "F1 Help  F2 Open  Ctrl+C Copy image  F10 Quit" : "F1 Help  F2 Open  F3 Save  F4 SaveAs  F5 New  F10 Quit", INK, SURFACE);
    if (!VIEW_ONLY) {
        for (int i = 0; i < 7; i++) button(i * 10, 2, 10, tool_names[i], tool == i);
        button(1, 3, 8, "Copy", 0); button(10, 3, 7, "Cut", 0); button(18, 3, 9, "Paste", 0); button(28, 3, 8, "Undo", 0);
    }
    char info[80] = "Color: ", number[16]; fd_number(number, color); fd_text(info + 7, number, sizeof info - 7);
    label(50, 3, info, INK, SURFACE); gfx_rect(480, 48, 24, 16, color);
    gfx_rect(0, 384, 640, 16, BORDER); gfx_clear_text(0, 24, 80, 1); label(1, 24, status, WHITE, BORDER);
}
static void draw_image(void) {
    zoom = image.width <= 236 && image.height <= 144 ? 2 : 1;
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            int c = mem_get(image.segment, y * (u32)image.width + x);
            for (int dx = 0; dx < zoom; dx++) scaled[x * zoom + dx] = c;
        }
        for (int dy = 0; dy < zoom; dy++) {
            if (gfx_colors() == 256) gfx_write_span(16, 80 + y * zoom + dy, local_segment(), (unsigned)scaled, image.width * zoom);
            else for (int x = 0; x < image.width * zoom; x++) gfx_pixel(16 + x, 80 + y * zoom + dy, scaled[x]);
        }
        input_poll();
    }
    if (has_selection) {
        int left = 16 + x1 * zoom, top = 80 + y1 * zoom, w = (x2 - x1 + 1) * zoom, h = (y2 - y1 + 1) * zoom;
        gfx_rect(left, top, w, 1, INK); gfx_rect(left, top + h - 1, w, 1, WHITE);
        gfx_rect(left, top, 1, h, INK); gfx_rect(left + w - 1, top, 1, h, WHITE);
    }
}
static void draw(void) {
    gfx_rect(0, 0, 640, 400, DESKTOP); gfx_clear_text(0, 0, 80, 25);
    gfx_rect(14, 78, image.width * (image.width <= 236 && image.height <= 144 ? 2 : 1) + 4,
             image.height * (image.width <= 236 && image.height <= 144 ? 2 : 1) + 4, BORDER);
    draw_image();
    for (int i = 0; i < gfx_colors(); i++) {
        int x = 512 + i % 16 * 7, y = 80 + i / 16 * 16;
        gfx_rect(x, y, 7, 16, i);
        if (i == color) { gfx_rect(x, y, 7, 1, WHITE); gfx_rect(x, y + 15, 7, 1, INK); }
    }
    label(64, 22, gfx_colors() == 256 ? "256 colors" : "16 colors", WHITE, DESKTOP);
    chrome();
}
static int inside(int x, int y) { return x >= 0 && y >= 0 && x < image.width && y < image.height; }
static int checkpoint(void) {
    if (!bitmap_clone(&undo, &image)) { message("Not enough memory for undo; image retained"); return 0; }
    return 1;
}
static void paint_pixel(int x, int y, int c) {
    if (!inside(x, y)) return;
    mem_put(image.segment, y * (u32)image.width + x, c);
    gfx_rect(16 + x * zoom, 80 + y * zoom, zoom, zoom, c);
}
static void line(int ax, int ay, int bx, int by, int c) {
    int dx = bx > ax ? bx - ax : ax - bx, dy = by > ay ? ay - by : by - ay;
    int step_x = ax < bx ? 1 : -1, step_y = ay < by ? 1 : -1, error = dx + dy;
    for (;;) {
        paint_pixel(ax, ay, c); if (ax == bx && ay == by) break;
        int e = error * 2;
        if (e >= dy) { error += dy; ax += step_x; }
        if (e <= dx) { error += dx; ay += step_y; }
    }
}
static int flood(int x, int y) {
    int original = mem_get(image.segment, y * (u32)image.width + x);
    if (original == color) return 0;
    unsigned queue = mem_alloc(image.width * (u32)image.height * 2);
    if (!queue) { message("Not enough memory for fill; image retained"); return 0; }
    u32 head = 0, tail = 1; unsigned first = y * image.width + x;
    mem_put(queue, 0, first); mem_put(queue, 1, first >> 8); mem_put(image.segment, first, color);
    while (head < tail) {
        unsigned n = mem_get(queue, head * 2) | (unsigned)mem_get(queue, head * 2 + 1) << 8;
        head++; int nx = n % image.width, ny = n / image.width;
        for (int i = 0; i < 4; i++) {
            int xx = nx + (i == 0 ? -1 : i == 1 ? 1 : 0), yy = ny + (i == 2 ? -1 : i == 3 ? 1 : 0);
            if (inside(xx, yy)) {
                unsigned at = yy * image.width + xx;
                if (mem_get(image.segment, at) == original) {
                    mem_put(image.segment, at, color); mem_put(queue, tail * 2, at); mem_put(queue, tail * 2 + 1, at >> 8); tail++;
                }
            }
        }
        if (!(head & 255)) input_poll();
    }
    mem_free(queue); return 1;
}
static int copy_image(void) {
    int ax = has_selection ? x1 : 0, ay = has_selection ? y1 : 0;
    int w = has_selection ? x2 - x1 + 1 : image.width, h = has_selection ? y2 - y1 + 1 : image.height;
    Bitmap next = {0};
    if (!bitmap_new(&next, w, h, WHITE)) { message("Not enough memory for clipboard"); return 0; }
    for (int y = 0; y < h; y++) mem_copy(next.segment, y * (u32)w, image.segment, (ay + y) * (u32)image.width + ax, w);
    bitmap_free(&copied); copied = next; clip_image(copied.segment, w, h); message("Copied image to clipboard"); return 1;
}
static int confirm(const char *prompt) {
    gfx_rect(96, 144, 448, 112, PAPER); label(14, 10, prompt, INK, PAPER);
    label(14, 13, "Y: Yes   N/Esc: Cancel", INK, PAPER);
    for (;;) {
        if (key_ready()) { int k = key_read(); if (k == 'y' || k == 'Y') return 1; if (k == 'n' || k == 'N' || k == 27) return 0; }
        mouse_read(); input_idle();
    }
}
static int ask_path(const char *caption, char *value) {
    char input[FD_PATH]; fd_text(input, value, sizeof input); int used = fd_len(input);
    for (;;) {
        gfx_rect(32, 144, 576, 112, PAPER); gfx_clear_text(4, 9, 72, 7);
        label(6, 10, caption, INK, PAPER); label(6, 12, input, WHITE, SELECTED);
        label(6, 14, "Enter: accept  Ctrl+U: clear  Esc: cancel", INK, PAPER);
        while (!key_ready()) { mouse_read(); input_idle(); }
        int key = key_read();
        if (key == 27) return 0;
        if (key == 13 && used) { fd_text(value, input, FD_PATH); return 1; }
        if (key == 21) used = 0;
        else if (key == 8 && used) used--;
        else if (key >= 32 && key <= 126 && used < FD_PATH - 1) input[used++] = key;
        input[used] = 0;
    }
}
static int normalize(char *out, const char *input) {
    char cwd[FD_PATH]; return p_cwd(cwd) >= 0 && !fd_path(out, cwd, input);
}
static int save_as(int ask) {
    char target[FD_PATH], normalized[FD_PATH]; fd_text(target, path, sizeof target);
    if (ask && !ask_path("Save BMP as:", target)) return 0;
    if (!normalize(normalized, target)) { message("Invalid image path"); return 0; }
    Entry entry;
    if (fd_cmp(normalized, owned) && p_stat(normalized, &entry) == 0 && !confirm("Replace existing image?")) return 0;
    int error = bitmap_save(&image, normalized);
    if (error < 0) { message(error == -FD_ROLLBACK ? "Save failed: recover original from KDIMG.BAK" : "Save failed; image retained (check KDIMG.TMP / KDIMG.BAK)"); return 0; }
    fd_text(path, normalized, sizeof path); fd_text(owned, normalized, sizeof owned); dirty = 0;
    message(error ? "Saved BMP; backup remains in KDIMG.BAK" : "Saved BMP"); return 1;
}
static int discard(void) { return !dirty || confirm("Discard unsaved image changes?"); }
static void open_image(void) {
    char next[FD_PATH], normalized[FD_PATH]; fd_text(next, path, sizeof next);
    if (!discard() || !ask_path("Open BMP:", next)) return;
    if (!normalize(normalized, next)) { message("Invalid image path"); return; }
    int error = bitmap_load(&image, normalized);
    if (error < 0) { message("Cannot open BMP: need uncompressed 1/4/8/24-bit, max 320x200"); return; }
    fd_text(path, normalized, sizeof path); fd_text(owned, normalized, sizeof owned);
    bitmap_free(&undo); dirty = 0; has_selection = 0; px = py = 0; publish(); message("Opened BMP");
}
static int handle_key(int key) {
    if (key == KEY_F1 + 9 || key == 27) return discard();
    if (key == KEY_F1) {
        gfx_rect(32, 96, 576, 240, PAPER);
        label(6, 7, VIEW_ONLY ? "KVIEW: native BMP image viewer" : "KPAINT: native bitmap editor", INK, PAPER);
        label(6, 9, "BMP: uncompressed 1/4/8/24-bit, up to 320x200", INK, PAPER);
        label(6, 11, VIEW_ONLY ? "F2 opens another image; Ctrl+C copies the image" : "P/L/R/F/I/E/S: pencil/line/rect/fill/pick/erase/select", INK, PAPER);
        label(6, 13, VIEW_ONLY ? "Use the WebNP2 media panel to copy/save PNG" : "Ctrl+C/X/V: copy/cut/paste selection or entire image", INK, PAPER);
        label(6, 15, "Ctrl+Z / U: undo; arrows move cursor; Space draws", INK, PAPER);
        label(6, 17, "Save writes 8-bit BMP; failed saves retain original", INK, PAPER);
        label(6, 19, "Press any key", INK, PAPER);
        while (!key_ready()) { mouse_read(); input_idle(); } key_read(); return 0;
    }
    if (key == KEY_F1 + 1) { open_image(); return 0; }
    if (key == 3 || key == 'c' || key == 'C') { copy_image(); return 0; }
    if (VIEW_ONLY) return 0;
    if (key == KEY_F1 + 2 || key == 19) save_as(0);
    else if (key == KEY_F1 + 3) save_as(1);
    else if (key == KEY_F1 + 4) {
        if (discard() && bitmap_new(&image, 224, 144, WHITE)) {
            bitmap_free(&undo); has_selection = 0; px = py = 0; dirty = 0; owned[0] = 0;
            fd_text(path, "DRAW.BMP", sizeof path); publish(); message("New image");
        }
    } else if (key == 26 || key == 'u' || key == 'U') {
        if (undo.segment) { Bitmap swapped = image; image = undo; undo = swapped; dirty = 1; has_selection = 0; publish(); message("Undo / redo"); }
        else message("Nothing to undo");
    } else if (key == 24) {
        if (copy_image() && checkpoint()) {
            int ax = has_selection ? x1 : 0, ay = has_selection ? y1 : 0;
            int w = has_selection ? x2 - x1 + 1 : image.width, h = has_selection ? y2 - y1 + 1 : image.height;
            for (int y = 0; y < h; y++) mem_fill(image.segment, (ay + y) * (u32)image.width + ax, w, WHITE);
            dirty = 1; message("Cut image to clipboard");
        }
    } else if (key == 22) {
        if (!copied.segment) message("Image clipboard is empty");
        else if (checkpoint()) {
            if (!inside(px, py)) px = py = 0;
            for (int y = 0; y < copied.height && py + y < image.height; y++) {
                int w = copied.width; if (px + w > image.width) w = image.width - px;
                mem_copy(image.segment, (py + y) * (u32)image.width + px, copied.segment, y * (u32)copied.width, w);
            }
            dirty = 1; has_selection = 0; publish(); message("Pasted image");
        }
    } else if (key == KEY_LEFT && px > 0) px--;
    else if (key == KEY_RIGHT && px + 1 < image.width) px++;
    else if (key == KEY_UP && py > 0) py--;
    else if (key == KEY_DOWN && py + 1 < image.height) py++;
    else if (key == ' ' && checkpoint()) { paint_pixel(px, py, color); dirty = 1; publish(); }
    else {
        if (key >= 'a' && key <= 'z') key -= 32;
        const char shortcuts[] = "PLRFIES";
        for (int i = 0; i < 7; i++) if (key == shortcuts[i]) { tool = i; has_selection = 0; }
    }
    return 0;
}
static int mouse_event(Mouse now, int previous) {
    int mx = (now.x - 16) / zoom, my = (now.y - 80) / zoom;
    if (now.x < 16 || now.y < 80) { mx = -1; my = -1; }
    if (inside(mx, my)) { px = mx; py = my; }
    if ((now.buttons & 2) && !(previous & 2) && inside(mx, my)) {
        color = mem_get(image.segment, my * (u32)image.width + mx); chrome(); return 0;
    }
    if (painting && (previous & 1)) {
        if (mx < 0) mx = 0;
        if (mx >= image.width) mx = image.width - 1;
        if (my < 0) my = 0;
        if (my >= image.height) my = image.height - 1;
        if (tool == PENCIL || tool == ERASE) {
            line(last_x, last_y, mx, my, tool == ERASE ? WHITE : color); last_x = mx; last_y = my;
        }
        if (!(now.buttons & 1)) {
            if (tool == LINE) line(sx, sy, mx, my, color);
            if (tool == RECT) { line(sx, sy, mx, sy, color); line(mx, sy, mx, my, color); line(mx, my, sx, my, color); line(sx, my, sx, sy, color); }
            if (tool == SELECT) {
                x1 = sx < mx ? sx : mx; x2 = sx > mx ? sx : mx; y1 = sy < my ? sy : my; y2 = sy > my ? sy : my;
                has_selection = 1; message("Selection ready for Copy / Cut");
            } else { dirty = 1; publish(); message("Image changed"); }
            painting = 0; return 1;
        }
        return 0;
    }
    if (!(now.buttons & 1) || (previous & 1)) return 0;
    if (now.x >= 512 && now.x < 624 && now.y >= 80 && now.y < 336) {
        int next = (now.y - 80) / 16 * 16 + (now.x - 512) / 7;
        if (next < gfx_colors()) color = next;
        return 1;
    }
    if (!VIEW_ONLY && now.y / 16 == 2 && now.x / 80 < 7) { tool = now.x / 80; has_selection = 0; return 1; }
    if (!VIEW_ONLY && now.y / 16 == 3) {
        int cx = now.x / 8;
        if (cx >= 1 && cx < 9) return handle_key(3), 1;
        if (cx >= 10 && cx < 17) return handle_key(24), 1;
        if (cx >= 18 && cx < 27) return handle_key(22), 1;
        if (cx >= 28 && cx < 36) return handle_key(26), 1;
    }
    if (VIEW_ONLY || !inside(mx, my)) return 0;
    if (tool == PICK) { color = mem_get(image.segment, my * (u32)image.width + mx); return 1; }
    if (tool != SELECT && !checkpoint()) return 1;
    has_selection = 0; sx = last_x = mx; sy = last_y = my;
    if (tool == FILL) { if (flood(mx, my)) { dirty = 1; publish(); message("Filled area"); } return 1; }
    painting = 1;
    if (tool == PENCIL || tool == ERASE) paint_pixel(mx, my, tool == ERASE ? WHITE : color);
    return 0;
}
int main(void) {
    p_init(); unsigned length = mem_get(local_segment(), 0x80); char argument[FD_PATH];
    if (length >= FD_PATH) length = FD_PATH - 1;
    unsigned start = 0; while (start < length && mem_get(local_segment(), 0x81 + start) == ' ') start++;
    unsigned used = 0; while (start < length) argument[used++] = mem_get(local_segment(), 0x81 + start++);
    while (used && argument[used - 1] == ' ') used--;
    argument[used] = 0;
    gfx_start(); clip_state(VIEW_ONLY ? 3 : 4);
    if (!bitmap_new(&image, 224, 144, WHITE)) { clip_state(0); gfx_stop(); p_output("Image allocation failed\r\n"); return 8; }
    fd_text(path, VIEW_ONLY ? "COLORS.BMP" : "DRAW.BMP", sizeof path);
    if (used || VIEW_ONLY) {
        char normalized[FD_PATH];
        if (normalize(normalized, used ? argument : path) && !bitmap_load(&image, normalized)) {
            fd_text(path, normalized, sizeof path); fd_text(owned, normalized, sizeof owned); message("Opened BMP");
        } else message("Cannot open BMP; use F2 to choose another image");
    }
    publish(); draw(); Mouse previous = mouse_read(); gfx_pointer_show(previous.x, previous.y);
    for (;;) {
        Mouse now = mouse_read(); int ready = key_ready(), redraw = 0, stop = 0;
        if (now.x != previous.x || now.y != previous.y || now.buttons != previous.buttons || ready) {
            gfx_pointer_hide();
            for (int i = 0; i < 16; i++) {
                if (mouse_event(now, previous.buttons)) redraw = 1;
                previous = now;
                if (!mouse_queued()) break;
                now = mouse_read();
            }
            if (ready) { painting = 0; stop = handle_key(key_read()); redraw = 1; }
            if (stop) break;
            if (redraw) draw();
            gfx_pointer_show(now.x, now.y);
        }
        previous = now; input_idle();
    }
    clip_state(0); gfx_stop(); bitmap_free(&image); bitmap_free(&undo); bitmap_free(&copied);
    p_finish(); p_output(VIEW_ONLY ? "KVIEW exited.\r\n" : "KPAINT exited.\r\n"); return 0;
}
