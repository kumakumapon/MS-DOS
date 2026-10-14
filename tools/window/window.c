#include "hardware.h"

/* Window positions use 8 x 16 cells. Four graphics planes hold both the
 * 16-color scene and CG-ROM glyphs; hidden text VRAM mirrors the labels. */
enum { LAUNCHER, FILES, MEMO, HELP, ABOUT, WINDOWS };
typedef struct { int x, y, w, h, open, hidden, selected; } Window;
static Window windows[WINDOWS];
static int order[WINDOWS], active = -1, mode, dirty, confirm, exiting;
static char home[FD_PATH], folder[FD_PATH], note_path[FD_PATH];
static char memo[1024], io[2048], status[80] = "Ready";
static int used, count, truncated, list_error, memo_blocked;
static Entry entries[128];
static const char *titles[] = {"Launcher", "Files", "Memo", "Help", "About"};
static const char *apps[] = {"FD98.COM", "EDIT98.COM", "CPANEL.COM", "RBASIC.COM", "COMMAND.COM"};
static const char *labels[] = {"File manager", "Text editor", "System settings", "BASIC", "DOS shell (EXIT returns)"};

static void label(int x, int y, const char *s, int width, int fg, int bg, int selected) {
    for (int i = 0; i < width && s[i]; i++) gfx_cell(x + i, y, s[i], fg, bg, selected);
}
static void text(int x, int y, const char *s, int width, int selected) {
    label(x, y, s, width, selected ? WHITE : INK, selected ? SELECTED : PAPER, selected);
}
static void line(Window *w, int row, const char *s, int selected) {
    if (row < w->h - 5) text(w->x + 2, w->y + 3 + row, s, w->w - 4, selected);
}
static void button(int x, int y, int width, const char *caption, int pressed) {
    int bg = pressed ? SELECTED : BUTTON;
    gfx_rect(x * 8, y * 16, width * 8, 16, bg);
    gfx_rect(x * 8, y * 16, width * 8, 1, WHITE);
    gfx_rect(x * 8, y * 16, 1, 16, WHITE);
    gfx_rect(x * 8, y * 16 + 15, width * 8, 1, BORDER);
    gfx_rect((x + width) * 8 - 1, y * 16, 1, 16, BORDER);
    label(x + 1, y, caption, width - 2, pressed ? WHITE : INK, bg, pressed);
}
static void message(const char *s) { fd_text(status, s, sizeof status); }
static void refresh(void) {
    count = p_list(folder, entries, 128, &truncated);
    list_error = count < 0;
    if (list_error) count = 0;
    if (windows[FILES].selected >= count) windows[FILES].selected = count ? count - 1 : 0;
}
static void focus(int id) {
    int at = 0;
    while (order[at] != id) at++;
    for (; at < WINDOWS - 1; at++) order[at] = order[at + 1];
    order[WINDOWS - 1] = id;
    windows[id].open = 1; windows[id].hidden = 0;
    active = id; mode = 0;
}
static void next(void) {
    int start = active;
    for (int i = 1; i <= WINDOWS; i++) {
        int id = (start + i + WINDOWS) % WINDOWS;
        if (windows[id].open) { focus(id); return; }
    }
    active = -1; mode = 0;
}
static int save(void) {
    char temp[FD_PATH], backup[FD_PATH]; Entry existing;
    if (memo_blocked) { message("NOTE.TXT needs Text editor; original file retained"); return 0; }
    if (fd_path(temp, home, "KDTMP.TMP") || fd_path(backup, home, "KDOLD.TMP")) return 0;
    int present = p_stat(note_path, &existing), old = p_stat(backup, &existing);
    if ((present < 0 && present != -2) || old != -2) {
        message("Cannot save: read error or KDOLD.TMP already exists"); return 0;
    }
    int handle = p_create(temp), length = 0;
    if (handle < 0) { message("Cannot create KDTMP.TMP; original file retained"); return 0; }
    for (int i = 0; i < used; i++) {
        if (memo[i] == '\n') io[length++] = '\r';
        io[length++] = memo[i];
    }
    int wrote = p_write(handle, io, length), closed = p_close(handle);
    if (wrote != length || closed < 0) {
        p_unlink(temp); message("Save failed; original file and text retained"); return 0;
    }
    if (present == 0 && p_rename(note_path, backup) < 0) {
        p_unlink(temp); message("Cannot replace NOTE.TXT; original file retained"); return 0;
    }
    if (p_rename(temp, note_path) < 0) {
        if (present == 0 && p_rename(backup, note_path) < 0) {
            message("Save failed; recover original from KDOLD.TMP"); return 0;
        }
        p_unlink(temp); message("Save failed; original file and text retained"); return 0;
    }
    dirty = 0;
    if (present == 0 && p_unlink(backup) < 0) message("Saved NOTE.TXT; backup remains in KDOLD.TMP");
    else message("Saved NOTE.TXT");
    return 1;
}
static void load(void) {
    used = 0; memo_blocked = 0;
    int handle = p_open(note_path);
    if (handle < 0) {
        if (handle != -2) memo_blocked = 1;
        return;
    }
    int n = p_read(handle, io, sizeof io);
    p_close(handle);
    if (n < 0 || n == (int)sizeof io) memo_blocked = 1;
    for (int i = 0; i < n; i++) {
        unsigned char ch = io[i];
        if (ch == '\r') continue;
        if (!(ch == '\n' || (ch >= 32 && ch <= 126)) || used == (int)sizeof memo - 1) {
            memo_blocked = 1; break;
        }
        memo[used++] = ch;
    }
    if (memo_blocked) { used = 0; message("NOTE.TXT needs Text editor; original file retained"); }
}
static void enter_graphics(void) { gfx_start(); }
static void launch(const char *program, const char *arguments) {
    gfx_stop(); p_finish();
    int result = p_exec(program, arguments);
    if (result < 0) message("Cannot start program; check its path and memory");
    else {
        p_output("\r\nPress any key or click to return to KDESK...");
        mouse_start(); int previous = mouse_read().buttons;
        while (!key_ready()) {
            Mouse current = mouse_read();
            if ((current.buttons & 1) && !(previous & 1)) break;
            previous = current.buttons; input_idle();
        }
        if (key_ready()) key_read();
        mouse_stop(); message("Program returned to desktop");
    }
    if (!dirty) load();
    enter_graphics(); refresh();
}
static void tool(int id, const char *arguments) {
    char path[FD_PATH];
    if (fd_path(path, home, apps[id])) { message("Application path is too long"); return; }
    launch(path, arguments);
}
static void file_open(void) {
    Window *w = &windows[FILES];
    if (!count) return;
    Entry *entry = &entries[w->selected];
    char path[FD_PATH];
    if (fd_path(path, folder, entry->name)) { message("File path is too long"); return; }
    if (entry->attr & FD_DIR) {
        fd_text(folder, path, sizeof folder); w->selected = 0; refresh(); return;
    }
    const char *extension = entry->name;
    for (int i = 0; entry->name[i]; i++) if (entry->name[i] == '.') extension = entry->name + i;
    if (!fd_cmp(extension, ".COM") || !fd_cmp(extension, ".EXE")) launch(path, "");
    else if (!fd_cmp(extension, ".TXT") || !fd_cmp(extension, ".BAS")) {
        /* The bundled DOS tools parse bare DOS paths, so refuse ambiguous spaces. */
        for (int i = 0; path[i]; i++) if (path[i] == ' ') { message("Open paths without spaces using the DOS shell"); return; }
        tool(!fd_cmp(extension, ".BAS") ? 3 : 1, path);
    } else message("Open COM/EXE, TXT or BAS; use File manager for other files");
}
static void draw_memo(Window *w) {
    int cols = w->w - 4, rows = w->h - 5, row = 0, col = 0;
    for (int i = 0; i < used; i++) {
        if (memo[i] == '\n') { row++; col = 0; }
        else if (++col == cols) { row++; col = 0; }
    }
    int top = row >= rows ? row - rows + 1 : 0;
    row = 0; col = 0;
    for (int i = 0; i < used; i++) {
        if (memo[i] == '\n') { row++; col = 0; continue; }
        if (row >= top) gfx_cell(w->x + 2 + col, w->y + 3 + row - top, memo[i], INK, PAPER, 0);
        if (++col == cols) { row++; col = 0; }
    }
    if (active == MEMO) gfx_cell(w->x + 2 + col, w->y + 3 + row - top, '_', WHITE, SELECTED, 1);
}
static int launcher_top(Window *w) {
    int visible = (w->h - 4) / 2;
    return w->selected >= visible ? w->selected - visible + 1 : 0;
}
static void frame(int x, int y, int w, int h) {
    gfx_rect(x * 8 + 5, y * 16 + 5, w * 8, h * 16, SHADOW);
    gfx_rect(x * 8, y * 16, w * 8, h * 16, PAPER);
    gfx_rect(x * 8, y * 16, w * 8, 1, WHITE);
    gfx_rect(x * 8, y * 16, 1, h * 16, WHITE);
    gfx_rect(x * 8, (y + h) * 16 - 2, w * 8, 2, BORDER);
    gfx_rect((x + w) * 8 - 2, y * 16, 2, h * 16, BORDER);
    gfx_clear_text(x, y, w + 1, h + 1);
}
static void draw_window(int id) {
    Window *w = &windows[id]; int title_color = active == id ? ACCENT : INACTIVE;
    frame(w->x, w->y, w->w, w->h);
    gfx_rect(w->x * 8 + 3, w->y * 16 + 8, w->w * 8 - 6, 28, title_color);
    label(w->x + 2, w->y + 1, titles[id], w->w - 13, active == id ? WHITE : INK, title_color, 0);
    button(w->x + w->w - 9, w->y + 1, 3, "_", 0);
    button(w->x + w->w - 5, w->y + 1, 3, "x", 0);
    if (id == LAUNCHER) {
        int top = launcher_top(w), visible = (w->h - 4) / 2;
        static const char *icons[] = {"F", "E", "S", "B", ">"};
        for (int row = 0; row < visible && row + top < 5; row++) {
            int i = row + top, y = w->y + 3 + row * 2, bg = i == w->selected ? SELECTED : BUTTON;
            gfx_rect((w->x + 2) * 8, y * 16, (w->w - 4) * 8, 16, bg);
            gfx_rect((w->x + 2) * 8, y * 16, 16, 16, ACCENT);
            label(w->x + 2, y, icons[i], 1, WHITE, ACCENT, 0);
            label(w->x + 5, y, labels[i], w->w - 7, i == w->selected ? WHITE : INK, bg, i == w->selected);
        }
        text(w->x + 2, w->y + w->h - 2, "Click an app / Enter to open", w->w - 4, 0);
    } else if (id == FILES) {
        line(w, 0, folder, 0);
        int visible = w->h - 7, top = w->selected >= visible ? w->selected - visible + 1 : 0;
        for (int i = 0; i < visible && i + top < count; i++) {
            Entry *e = &entries[i + top]; int y = w->y + 5 + i, selected = i + top == w->selected;
            char caption[32]; fd_text(caption, e->attr & FD_DIR ? "<DIR> " : "      ", sizeof caption);
            fd_text(caption + 6, e->name, sizeof caption - 6);
            gfx_rect((w->x + 2) * 8, y * 16, (w->w - 6) * 8, 16, selected ? SELECTED : PAPER);
            text(w->x + 2, y, caption, w->w - 6, selected);
        }
        if (list_error) line(w, 2, "Directory read failed", 0);
        else if (!count) line(w, 2, "Empty directory", 0);
        button(w->x + w->w - 3, w->y + 4, 2, "", 0);
        label(w->x + w->w - 3, w->y + 4, "^", 1, INK, BUTTON, 0);
        button(w->x + w->w - 3, w->y + w->h - 4, 2, "", 0);
        label(w->x + w->w - 3, w->y + w->h - 4, "v", 1, INK, BUTTON, 0);
        button(w->x + 2, w->y + w->h - 2, 9, "Parent", 0);
        button(w->x + 12, w->y + w->h - 2, 8, "Open", 0);
        if (truncated) text(w->x + w->w - 8, w->y + 3, "128+", 4, 0);
    } else if (id == MEMO) {
        if (memo_blocked) line(w, 0, "Open NOTE.TXT in the Text editor", 0);
        else draw_memo(w);
        button(w->x + 2, w->y + w->h - 2, 9, "Save", 0);
        text(w->x + 12, w->y + w->h - 2, dirty ? "NOTE.TXT *" : "NOTE.TXT", w->w - 14, 0);
    } else if (id == HELP) {
        static const char *help[] = {"F1 Help   F2 Launcher   F3 Files", "F4 Memo   F5 About      Tab: switch", "Drag title: move / corner: resize", "F6 then arrows: move window", "F7 then arrows: resize window", "F8: minimize  Tab/taskbar: restore", "Esc: close    F10: exit to DOS", "Memo: ASCII text, Ctrl+S saves", "Files: double-click or Open"};
        for (int i = 0; i < 9; i++) line(w, i, help[i], 0);
    } else {
        line(w, 0, "Kuma Desktop 1.1", 0);
        line(w, 2, "Original PC-98 window system", 0);
        line(w, 3, "MS-DOS 4.0 / 640 x 400 / 16 colors", 0);
        line(w, 5, "Copyright 2026 kumakumapon", 0);
        line(w, 6, "MIT license - see KDLIC.TXT", 0);
    }
    for (int i = 0; i < 3; i++)
        gfx_rect((w->x + w->w) * 8 - 7 + i * 2, (w->y + w->h) * 16 - 5 - i * 3, 1, 3 + i * 3, MUTED);
}
static void draw(void) {
    gfx_rect(0, 0, 640, 400, DESKTOP); gfx_clear_text(0, 0, 80, 25);
    for (int y = 64; y < 352; y += 32) gfx_rect(0, y, 640, 1, DESKTOP_LIGHT);
    label(60, 21, "PC-98 / MS-DOS", 18, HANDLE, DESKTOP, 0);
    gfx_rect(0, 0, 640, 16, INK);
    label(2, 0, "Kuma Desktop", 16, WHITE, INK, 0);
    static const char *menus[] = {"Apps", "Files", "Memo", "Help", "About", "Exit"};
    static const int menu_x[] = {22, 30, 39, 47, 55, 71};
    for (int i = 0; i < 6; i++) button(menu_x[i], 0, 7, menus[i], 0);
    for (int i = 0; i < WINDOWS; i++) {
        int id = order[i];
        if (windows[id].open && !windows[id].hidden) draw_window(id);
    }
    gfx_rect(0, 368, 640, 16, BORDER); gfx_rect(0, 384, 640, 16, SURFACE);
    label(1, 24, "F1 Help  Tab switch  F6/F7 move/resize", 38, INK, SURFACE, 0);
    static const char *tasks[] = {"Apps", "Files", "Memo", "Help", "About"};
    for (int i = 0; i < WINDOWS; i++) if (windows[i].open) button(40 + i * 8, 24, 8, tasks[i], active == i);
    if (confirm) {
        label(1, 23, "Unsaved memo: Enter saves / D discards / Esc cancels", 78, WHITE, BORDER, 0);
        frame(17, 9, 46, 8);
        gfx_rect(17 * 8 + 3, 9 * 16 + 8, 46 * 8 - 6, 28, ACCENT);
        label(19, 10, "Unsaved memo", 40, WHITE, ACCENT, 0);
        text(20, 12, "Save changes before closing?", 40, 0);
        button(20, 14, 10, "Save", 0); button(34, 14, 11, "Discard", 0); button(49, 14, 11, "Cancel", 0);
    } else if (mode) label(1, 23, mode == 1 ? "MOVE: arrows change position; Enter finishes" : "RESIZE: arrows change size; Enter finishes", 78, WHITE, BORDER, 0);
    else label(1, 23, status, 78, WHITE, BORDER, 0);
}
static void close_window(void) {
    windows[active].open = 0; next();
}
static int handle_confirm(int key) {
    if (!confirm) return 0;
    if (key == 27) { confirm = 0; exiting = 0; message("Close cancelled"); }
    else if (key == 13 || key == 'd' || key == 'D') {
        if (key == 13 && !save()) return 1;
        if (key != 13 && !exiting) load();
        dirty = 0; confirm = 0;
        if (exiting) return 2;
        close_window(); message("Memo closed");
    }
    return 1;
}
static int handle_key(int key) {
    int decision = handle_confirm(key);
    if (decision == 2) return 1;
    if (decision) return 0;
    if (key == KEY_F1 + 9) {
        if (dirty) { confirm = 1; exiting = 1; return 0; }
        return 1;
    }
    if (key >= KEY_F1 && key <= KEY_F1 + 4) {
        int id = key == KEY_F1 ? HELP : key == KEY_F1 + 1 ? LAUNCHER : key == KEY_F1 + 2 ? FILES : key == KEY_F1 + 3 ? MEMO : ABOUT;
        if (id == FILES) refresh();
        focus(id); return 0;
    }
    if (key == 9) { next(); return 0; }
    if (active < 0) return 0;
    Window *w = &windows[active];
    if (key == KEY_F1 + 5 || key == KEY_F1 + 6) { int wanted = key == KEY_F1 + 5 ? 1 : 2; mode = mode == wanted ? 0 : wanted; return 0; }
    if (mode && (key == 13 || key == 27)) { mode = 0; return 0; }
    if (mode) {
        int dx = key == KEY_RIGHT ? 1 : key == KEY_LEFT ? -1 : 0;
        int dy = key == KEY_DOWN ? 1 : key == KEY_UP ? -1 : 0;
        if (mode == 1) {
            if (w->x + dx >= 0 && w->x + dx + w->w < 80) w->x += dx;
            if (w->y + dy >= 1 && w->y + dy + w->h < 24) w->y += dy;
        } else {
            if (w->w + dx >= 30 && w->x + w->w + dx < 80) w->w += dx;
            if (w->h + dy >= 10 && w->y + w->h + dy < 24) w->h += dy;
        }
        return 0;
    }
    if (key == KEY_F1 + 7) { w->hidden = 1; active = -1;
        for (int i = WINDOWS - 1; i >= 0; i--) if (windows[order[i]].open && !windows[order[i]].hidden) { active = order[i]; break; }
        message("Window minimized; Tab restores"); return 0;
    }
    if (key == 27) {
        if (active == MEMO && dirty) { confirm = 1; exiting = 0; }
        else close_window();
        return 0;
    }
    if (active == LAUNCHER || active == FILES) {
        int limit = active == LAUNCHER ? 5 : count;
        if (key == KEY_UP && w->selected > 0) w->selected--;
        if (key == KEY_DOWN && w->selected + 1 < limit) w->selected++;
        if (key == KEY_HOME) w->selected = 0;
        if (key == KEY_END && limit) w->selected = limit - 1;
        if (key == 13) { if (active == LAUNCHER) tool(w->selected, ""); else file_open(); }
        if (active == FILES && key == 8) { fd_parent(folder); w->selected = 0; refresh(); }
    } else if (active == MEMO) {
        if (memo_blocked) { message("Use Text editor for non-ASCII or larger NOTE.TXT"); return 0; }
        if (key == 19) save();
        else if (key == 8 && used) { used--; dirty = 1; }
        else if ((key == 13 || (key >= 32 && key <= 126)) && used < (int)sizeof memo - 1) {
            memo[used++] = key == 13 ? '\n' : key; dirty = 1;
        } else if (used == (int)sizeof memo - 1) message("Memo full (1023 characters); use the Text editor for larger files");
    }
    return 0;
}

static int drag, drag_id, drag_mx, drag_my, drag_x, drag_y, drag_w, drag_h;
static int preview_x, preview_y, preview_w, preview_h, preview_visible;
static int last_file = -1;
static u32 last_click;
static int clamp(int n, int low, int high) { return n < low ? low : n > high ? high : n; }
static int mouse_event(Mouse now, int previous, int *redraw) {
    int cx = now.x / 8, cy = now.y / 16;
    if (drag && (previous & 1)) {
        Window *w = &windows[drag_id];
        if (!w->open || w->hidden || confirm) { drag = 0; return 0; }
        int x = w->x, y = w->y, width = w->w, height = w->h;
        if (drag == 1) {
            x = clamp(drag_x + (now.x - drag_mx) / 8, 0, 79 - w->w);
            y = clamp(drag_y + (now.y - drag_my) / 16, 1, 23 - w->h);
        } else {
            width = clamp(drag_w + (now.x - drag_mx) / 8, 30, 79 - w->x);
            height = clamp(drag_h + (now.y - drag_my) / 16, 10, 23 - w->y);
        }
        if (preview_visible) gfx_outline(preview_x, preview_y, preview_w, preview_h);
        preview_visible = 0;
        if (!(now.buttons & 1)) {
            w->x = x; w->y = y; w->w = width; w->h = height;
            drag = 0; *redraw = 1;
        } else {
            preview_x = x; preview_y = y; preview_w = width; preview_h = height;
            gfx_outline(x, y, width, height); preview_visible = 1;
        }
        return 0;
    }
    if (!(now.buttons & 1)) { drag = 0; return 0; }
    if (previous & 1) return 0;
    int previous_file = last_file;
    last_file = -1; /* Only consecutive clicks in the same file list open it. */
    *redraw = 1;
    if (confirm) {
        if (cy == 14) {
            if (cx >= 20 && cx < 30) return 13;
            if (cx >= 34 && cx < 45) return 'd';
            if (cx >= 49 && cx < 60) return 27;
        }
        return 0;
    }
    if (cy == 0) {
        static const int menu_x[] = {22, 30, 39, 47, 55, 71};
        static const int menu_key[] = {KEY_F1 + 1, KEY_F1 + 2, KEY_F1 + 3, KEY_F1, KEY_F1 + 4, KEY_F1 + 9};
        for (int i = 0; i < 6; i++) if (cx >= menu_x[i] && cx < menu_x[i] + 7) return menu_key[i];
        return 0;
    }
    if (cy == 24 && cx >= 40) {
        int id = (cx - 40) / 8;
        if (id < WINDOWS && windows[id].open) focus(id);
        return 0;
    }
    for (int i = WINDOWS - 1; i >= 0; i--) {
        int id = order[i]; Window *w = &windows[id];
        if (!w->open || w->hidden || cx < w->x || cx >= w->x + w->w || cy < w->y || cy >= w->y + w->h) continue;
        focus(id);
        if (cy == w->y + 1 && cx >= w->x + w->w - 5 && cx < w->x + w->w - 2) return 27;
        if (cy == w->y + 1 && cx >= w->x + w->w - 9 && cx < w->x + w->w - 6) return KEY_F1 + 7;
        if (cx >= w->x + w->w - 2 && cy >= w->y + w->h - 2) drag = 2;
        else if (cy <= w->y + 1) drag = 1;
        if (drag) {
            last_file = -1; drag_id = id; drag_mx = now.x; drag_my = now.y;
            drag_x = w->x; drag_y = w->y; drag_w = w->w; drag_h = w->h; return 0;
        }
        if (id == LAUNCHER) {
            int row = cy - w->y - 3;
            if (row >= 0 && row < w->h - 5 && !(row & 1) && cx >= w->x + 2 && cx < w->x + w->w - 2) {
                int selected = launcher_top(w) + row / 2;
                if (selected < 5) { w->selected = selected; tool(selected, ""); }
            }
        } else if (id == FILES) {
            if (cx >= w->x + w->w - 3) {
                if (cy == w->y + 4) return KEY_UP;
                if (cy == w->y + w->h - 4) return KEY_DOWN;
            }
            if (cy == w->y + w->h - 2) {
                if (cx >= w->x + 2 && cx < w->x + 11) return 8;
                if (cx >= w->x + 12 && cx < w->x + 20) return 13;
            }
            int row = cy - w->y - 5, visible = w->h - 7;
            int top = w->selected >= visible ? w->selected - visible + 1 : 0;
            if (row >= 0 && row < visible && row + top < count && cx >= w->x + 2 && cx < w->x + w->w - 4) {
                int selected = row + top; u32 tick = now.ticks;
                if (previous_file == selected && ((tick - last_click) & 0xffffff) < 153600) {
                    w->selected = selected; last_file = -1; file_open();
                } else { w->selected = selected; last_file = selected; last_click = tick; }
            }
        } else if (id == MEMO && cy == w->y + w->h - 2 && cx >= w->x + 2 && cx < w->x + 11) save();
        return 0;
    }
    last_file = -1; return 0;
}
int main(void) {
    p_init();
    if (p_cwd(home) < 0) { p_output("Cannot read working directory\r\n"); return 1; }
    fd_text(folder, home, sizeof folder);
    if (fd_path(note_path, home, "NOTE.TXT")) return 1;
    for (int i = 0; i < WINDOWS; i++) {
        windows[i] = (Window){3 + i * 4, 2 + i, 48, 16, 0, 0, 0}; order[i] = i;
    }
    load(); refresh(); focus(LAUNCHER); enter_graphics();
    draw(); Mouse previous = mouse_read(); gfx_pointer_show(previous.x, previous.y);
    for (;;) {
        Mouse now = mouse_read(); int ready = key_ready(), redraw = 0, stop = 0;
        if (now.x != previous.x || now.y != previous.y || now.buttons != previous.buttons || ready) {
            gfx_pointer_hide();
            for (int i = 0; i < 16; i++) {
                int key = mouse_event(now, previous.buttons, &redraw);
                if (key && handle_key(key)) { stop = 1; break; }
                previous = now;
                if (!mouse_queued()) break;
                now = mouse_read();
            }
            if (stop) break;
            if (ready) {
                drag = 0; last_file = -1; redraw = 1;
                for (int i = 0; i < 16 && key_ready(); i++)
                    if (handle_key(key_read())) { stop = 1; break; }
                if (stop) break;
            }
            if (redraw) { preview_visible = 0; draw(); }
            gfx_pointer_show(now.x, now.y);
        }
        previous = now; input_idle();
    }
    gfx_stop(); p_finish(); p_output("KDESK exited.\r\n"); return 0;
}
