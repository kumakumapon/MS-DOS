#include "../filer/filer.h"

/* Window positions are text cells (8 x 16 pixels). Backgrounds are drawn to
 * the three PC-98 graphics planes; labels use the native text layer. */
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

static void bios(unsigned a, unsigned c) {
    unsigned d = 0;
    __asm__ volatile("int $0x18" : "+a"(a), "+c"(c), "+d"(d)
                     :: "ebx", "esi", "edi", "cc", "memory");
}
static void fill(unsigned segment, unsigned offset, unsigned length, unsigned value) {
    __asm__ volatile("pushw %%es; movw %w3,%%es; rep stosb; popw %%es"
                     : "+D"(offset), "+c"(length) : "a"(value), "r"(segment) : "memory");
}
static void rect(int x, int y, int w, int h, int color) {
    /* Coordinates and sizes are always clipped by the window manager. */
    for (int p = 0; p < 3; p++) {
        unsigned segment = 0xa800 + p * 0x800;
        unsigned value = color & (1 << p) ? 255 : 0;
        if (w == 80) fill(segment, y * 16 * 80, h * 16 * 80, value);
        else for (int row = y * 16; row < (y + h) * 16; row++)
            fill(segment, row * 80 + x, w, value);
    }
}
static void text(int x, int y, const char *s, int width, int selected) {
    for (int i = 0; i < width && s[i]; i++) p_cell(x + i, y, s[i], selected);
}
static void line(Window *w, int row, const char *s, int selected) {
    if (row < w->h - 5) text(w->x + 2, w->y + 3 + row, s, w->w - 4, selected);
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
    if (fd_path(temp, home, "W98TMP.TMP") || fd_path(backup, home, "W98OLD.TMP")) return 0;
    int present = p_stat(note_path, &existing), old = p_stat(backup, &existing);
    if ((present < 0 && present != -2) || old != -2) {
        message("Cannot save: read error or W98OLD.TMP already exists"); return 0;
    }
    int handle = p_create(temp), length = 0;
    if (handle < 0) { message("Cannot create W98TMP.TMP; original file retained"); return 0; }
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
            message("Save failed; recover original from W98OLD.TMP"); return 0;
        }
        p_unlink(temp); message("Save failed; original file and text retained"); return 0;
    }
    dirty = 0;
    if (present == 0 && p_unlink(backup) < 0) message("Saved NOTE.TXT; backup remains in W98OLD.TMP");
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
static void enter_graphics(void) {
    p_screen();
    bios(0x4200, 0xc000); /* 640 x 400, 8 colors */
    bios(0x4000, 0);
}
static void launch(const char *program, const char *arguments) {
    bios(0x4100, 0);
    p_finish();
    int result = p_exec(program, arguments);
    if (result < 0) message("Cannot start program; check its path and memory");
    else {
        p_output("\r\nPress any key to return to WIN98...");
        p_key(); message("Program returned to desktop");
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
        if (row >= top) p_cell(w->x + 2 + col, w->y + 3 + row - top, memo[i], 0);
        if (++col == cols) { row++; col = 0; }
    }
    if (active == MEMO) p_cell(w->x + 2 + col, w->y + 3 + row - top, '_', 1);
}
static void draw_window(int id) {
    Window *w = &windows[id];
    rect(w->x + 1, w->y + 1, w->w, w->h, 0);
    rect(w->x, w->y, w->w, w->h, 7);
    rect(w->x + 1, w->y + 1, w->w - 2, w->h - 2, 0);
    rect(w->x + 1, w->y + 1, w->w - 2, 1, active == id ? 5 : 1);
    /* Remove covered text from lower windows before drawing this window. */
    for (int y = w->y; y < w->y + w->h + 1; y++)
        for (int x = w->x; x < w->x + w->w + 1; x++) p_cell(x, y, ' ', 0);
    text(w->x + 2, w->y + 1, titles[id], w->w - 9, 0);
    text(w->x + w->w - 6, w->y + 1, "[Esc]", 5, 0);
    if (id == LAUNCHER) {
        for (int i = 0; i < 5; i++) line(w, i, labels[i], i == w->selected);
        line(w, 6, "Enter: launch    F3: Files", 0);
    } else if (id == FILES) {
        line(w, 0, folder, 0);
        int visible = w->h - 7, top = w->selected >= visible ? w->selected - visible + 1 : 0;
        for (int i = 0; i < visible && i + top < count; i++) {
            Entry *e = &entries[i + top];
            char label[32]; fd_text(label, e->attr & FD_DIR ? "<DIR> " : "      ", sizeof label);
            fd_text(label + 6, e->name, sizeof label - 6);
            line(w, i + 2, label, i + top == w->selected);
        }
        if (list_error) line(w, 2, "Directory read failed", 0);
        else if (!count) line(w, 2, "Empty directory", 0);
        text(w->x + 2, w->y + w->h - 2, truncated ? "First 128 entries; use FD98 for more" : "Enter: open  Backspace: parent", w->w - 4, 0);
    } else if (id == MEMO) {
        if (memo_blocked) line(w, 0, "Open NOTE.TXT in the Text editor", 0);
        else draw_memo(w);
        text(w->x + 2, w->y + w->h - 2, dirty ? "NOTE.TXT *  Ctrl+S: save" : "NOTE.TXT    Ctrl+S: save", w->w - 4, 0);
    } else if (id == HELP) {
        static const char *help[] = {"F1 Help   F2 Launcher   F3 Files", "F4 Memo   F5 About      Tab: switch", "F6 then arrows: move window", "F7 then arrows: resize window", "Enter: finish moving/resizing", "F8: minimize  Tab: restore", "Esc: close    F10: exit to DOS", "Memo: ASCII text, Ctrl+S saves", "DOS tools run in the foreground"};
        for (int i = 0; i < 9; i++) line(w, i, help[i], 0);
    } else {
        line(w, 0, "WIN98 Desktop 1.0", 0);
        line(w, 2, "Original PC-98 window system", 0);
        line(w, 3, "MS-DOS 4.0 / 640 x 400 / 8 colors", 0);
        line(w, 5, "Copyright 2026 kumakumapon", 0);
        line(w, 6, "MIT license - see WINLIC.TXT", 0);
    }
}
static void draw(void) {
    rect(0, 0, 80, 25, 1);
    for (int y = 0; y < 25; y++) for (int x = 0; x < 80; x++) p_cell(x, y, ' ', 0);
    rect(0, 0, 80, 1, 0);
    text(1, 0, "WIN98 Desktop  F1 Help  F2 Launcher  F3 Files  F4 Memo  F5 About  F10 Exit", 78, 0);
    for (int i = 0; i < WINDOWS; i++) {
        int id = order[i];
        if (windows[id].open && !windows[id].hidden) draw_window(id);
    }
    rect(0, 24, 80, 1, 0);
    if (confirm) text(1, 24, "Unsaved memo: Enter saves / D discards / Esc cancels", 78, 1);
    else if (mode) text(1, 24, mode == 1 ? "MOVE: arrows change position; Enter finishes" : "RESIZE: arrows change size; Enter finishes", 78, 0);
    else {
        text(1, 24, status, 38, 0);
        for (int i = 0; i < WINDOWS; i++) if (windows[i].open)
            text(40 + i * 8, 24, titles[i], 7, active == i);
    }
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
int main(void) {
    p_init();
    if (p_cwd(home) < 0) { p_output("Cannot read working directory\r\n"); return 1; }
    fd_text(folder, home, sizeof folder);
    if (fd_path(note_path, home, "NOTE.TXT")) return 1;
    for (int i = 0; i < WINDOWS; i++) {
        windows[i] = (Window){3 + i * 4, 2 + i, 48, 16, 0, 0, 0}; order[i] = i;
    }
    load(); refresh(); focus(LAUNCHER); enter_graphics();
    for (;;) {
        draw(); int key = p_key();
        int decision = handle_confirm(key);
        if (decision == 2) break;
        if (decision) continue;
        if (key == KEY_F1 + 9) {
            if (dirty) { confirm = 1; exiting = 1; continue; }
            break;
        }
        if (key >= KEY_F1 && key <= KEY_F1 + 4) {
            int id = key == KEY_F1 ? HELP : key == KEY_F1 + 1 ? LAUNCHER : key == KEY_F1 + 2 ? FILES : key == KEY_F1 + 3 ? MEMO : ABOUT;
            if (id == FILES) refresh();
            focus(id); continue;
        }
        if (key == 9) { next(); continue; }
        if (active < 0) continue;
        Window *w = &windows[active];
        if (key == KEY_F1 + 5 || key == KEY_F1 + 6) { int wanted = key == KEY_F1 + 5 ? 1 : 2; mode = mode == wanted ? 0 : wanted; continue; }
        if (mode && (key == 13 || key == 27)) { mode = 0; continue; }
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
            continue;
        }
        if (key == KEY_F1 + 7) { w->hidden = 1; active = -1;
            for (int i = WINDOWS - 1; i >= 0; i--) if (windows[order[i]].open && !windows[order[i]].hidden) { active = order[i]; break; }
            message("Window minimized; Tab restores"); continue;
        }
        if (key == 27) {
            if (active == MEMO && dirty) { confirm = 1; exiting = 0; }
            else close_window();
            continue;
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
            if (memo_blocked) { message("Use Text editor for non-ASCII or larger NOTE.TXT"); continue; }
            if (key == 19) save();
            else if (key == 8 && used) { used--; dirty = 1; }
            else if ((key == 13 || (key >= 32 && key <= 126)) && used < (int)sizeof memo - 1) {
                memo[used++] = key == 13 ? '\n' : key; dirty = 1;
            } else if (used == (int)sizeof memo - 1) message("Memo full (1023 characters); use the Text editor for larger files");
        }
    }
    bios(0x4100, 0); p_finish(); p_output("WIN98 exited.\r\n"); return 0;
}
