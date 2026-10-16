#include "clipboard.h"
#include "utilities.h"

/* Window positions use 8 x 16 cells. Four graphics planes hold both the
 * 16-color scene and CG-ROM glyphs; hidden text VRAM mirrors the labels. */
enum { LAUNCHER, FILES, MEMO, HELP, ABOUT, CALCULATOR, CLOCK, WINDOWS };
enum { LAUNCH_APPS = 9 };
typedef struct { int x, y, w, h, open, hidden, selected, ox, oy; } Window;
static Window windows[WINDOWS];
static Calculator calculator;
static DeskClock desk_clock;
static int icon_view = 1, twelve_hour, scene_dirty;
static u32 clock_tick;
static int order[WINDOWS], active = -1, mode, dirty, confirm, exiting;
static char home[FD_PATH], folder[FD_PATH], note_path[FD_PATH];
static char memo[1024], io[2048], status[80] = "Ready";
static int used, count, truncated, list_error, memo_blocked, select_all;
static Entry entries[128];
static const char *titles[] = {"Launcher", "Files", "Memo", "Help", "About", "Calculator", "Clock"};
static const char *apps[] = {"FD98.COM", "EDIT98.COM", "CPANEL.COM", "RBASIC.COM", "COMMAND.COM", "KVIEW.COM", "KPAINT.COM"};
static const char *labels[] = {"File manager", "Text editor", "System settings", "BASIC", "DOS shell (EXIT returns)", "Image viewer", "Image editor", "Calculator", "Clock"};
static const char *icon_labels[] = {"File manager", "Text editor", "Settings", "BASIC", "DOS shell", "Image viewer", "Image editor", "Calculator", "Clock"};

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
    if(active!=id || !windows[id].open || windows[id].hidden)scene_dirty=1;
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
    used = 0; memo_blocked = 0; select_all = 0;
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
static void enter_graphics(void) { gfx_start(); clip_state(1); }
static void launch(const char *program, const char *arguments) {
    scene_dirty=1;
    clip_mouse_save(); clip_state(2); gfx_stop(); p_finish();
    int result = p_exec(program, arguments);
    clip_mouse_resume();
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
    if (id == 7) { focus(CALCULATOR); return; }
    if (id == 8) { clock_read(&desk_clock); focus(CLOCK); return; }
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
    } else if (!fd_cmp(extension, ".BMP")) tool(5, path);
    else message("Open COM/EXE, TXT, BAS or BMP; use File manager for other files");
}
static void draw_memo(Window *w) {
    int cols = w->w - 4, rows = w->h - 6, row = 0, col = 0;
    for (int i = 0; i < used; i++) {
        if (memo[i] == '\n') { row++; col = 0; }
        else if (++col == cols) { row++; col = 0; }
    }
    int top = row >= rows ? row - rows + 1 : 0;
    row = 0; col = 0;
    for (int i = 0; i < used; i++) {
        if (memo[i] == '\n') { row++; col = 0; continue; }
        if (row >= top) gfx_cell(w->x + 2 + col, w->y + 3 + row - top, memo[i], select_all ? WHITE : INK, select_all ? SELECTED : PAPER, select_all);
        if (++col == cols) { row++; col = 0; }
    }
    if (active == MEMO) gfx_cell(w->x + 2 + col, w->y + 3 + row - top, '_', WHITE, SELECTED, 1);
}
static int launcher_columns(Window *w) { return (w->w - 4) / 14; }
static int launcher_rows(Window *w) { return (w->h - 6) / (icon_view ? 4 : 2); }
static int launcher_top(Window *w) {
    int columns = icon_view ? launcher_columns(w) : 1, visible = launcher_rows(w);
    int row = w->selected / columns;
    return row >= visible ? (row - visible + 1) * columns : 0;
}
static const char *calc_buttons[6][4] = {
    {"MC","MR","M-","M+"},{"CE","C","BS","/"},{"7","8","9","*"},
    {"4","5","6","-"},{"1","2","3","+"},{"+/-","0",".","="}
};
static const int calc_keys[6][4] = {
    {CALC_MC,CALC_MR,CALC_MINUS,CALC_PLUS},{CALC_CE,'c',8,'/'},
    {'7','8','9','*'},{'4','5','6','-'},{'1','2','3','+'},{CALC_SIGN,'0','.','='}
};
static void calculator_key(int key) {
    if(key == 3) {
        if(!calculator.error) {clip_copy(calculator.input,fd_len(calculator.input));message("Copied calculator result");}
    } else if(key == 22) {
        if(calc_paste(&calculator,(const char *)clipboard.text,clipboard.length)) message("Pasted calculator number");
        else message("Clipboard needs a number: up to 8 integer / 4 decimal digits");
    } else {
        int changed=calc_key(&calculator,key);
        if(changed) message(changed==2?"Calculator input limit: 8 integer / 4 decimal digits":"Calculator ready");
    }
}
static void calculator_value(Window *w) {
    const char *value=calc_text(&calculator);
    gfx_clear_text(w->x+2,w->y+3,w->w-4,3);
    gfx_rect((w->x+2)*8,(w->y+5)*16,(w->w-4)*8,16,PAPER);
    gfx_rect((w->x+2)*8,(w->y+3)*16,(w->w-4)*8,32,SURFACE);
    if(calculator.error) label(w->x+3,w->y+3,value,w->w-6,INK,SURFACE,0);
    else utility_digits((w->x+w->w-3)*8-fd_len(value)*16,(w->y+3)*16+3,value,INK);
    const char *prefix="Value: ";int at=w->x+2;
    for(int i=0;prefix[i];i++)p_cell(at++,w->y+3,prefix[i],0);
    for(int i=0;value[i];i++)p_cell(at++,w->y+3,value[i],0);
    if(calculator.memory) text(w->x+2,w->y+5,"M",1,0);
    if(calculator.pending) {char op[]={calculator.pending,0};text(w->x+w->w-4,w->y+5,op,1,0);}
 }
static void draw_calculator(Window *w) {
    calculator_value(w);
    int left=w->x+(w->w-32)/2;
    for(int row=0;row<6;row++)for(int col=0;col<4;col++)button(left+col*8,w->y+6+row*2,7,calc_buttons[row][col],0);
    button(w->x+2,w->y+w->h-2,8,"Copy",0);button(w->x+11,w->y+w->h-2,9,"Paste",0);button(w->x+21,w->y+w->h-2,5,"%",0);
}
static void clock_controls(Window *w) {
    button(w->x+2,w->y+w->h-2,10,twelve_hour?"24-hour":"12-hour",0);
    button(w->x+13,w->y+w->h-2,8,"Copy",0);
    clock_draw(&desk_clock,twelve_hour,w->x*8,w->y*16,w->w*8);
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
    Window *w = &windows[id]; gfx_origin(w->ox, w->oy); int title_color = active == id ? ACCENT : INACTIVE;
    frame(w->x, w->y, w->w, w->h);
    gfx_rect(w->x * 8 + 3, w->y * 16 + 8, w->w * 8 - 6, 28, title_color);
    label(w->x + 2, w->y + 1, titles[id], w->w - 13, active == id ? WHITE : INK, title_color, 0);
    button(w->x + w->w - 9, w->y + 1, 3, "_", 0);
    button(w->x + w->w - 5, w->y + 1, 3, "x", 0);
    if (id == LAUNCHER) {
        int top=launcher_top(w), rows=launcher_rows(w), columns=icon_view?launcher_columns(w):1;
        for(int row=0;row<rows;row++)for(int col=0;col<columns;col++) {
            int i=top+row*columns+col;if(i>=LAUNCH_APPS)continue;
            int x=w->x+2+col*14,y=w->y+3+row*(icon_view?4:2),selected=i==w->selected,bg=selected?SELECTED:BUTTON;
            if(icon_view) {
                gfx_rect(x*8,y*16,13*8,3*16,bg);utility_icon(i,x*8+36,y*16);
                label(x,y+2,icon_labels[i],13,selected?WHITE:INK,bg,selected);
            } else {
                gfx_rect(x*8,y*16,(w->w-4)*8,16,bg);
                label(x+1,y,labels[i],w->w-6,selected?WHITE:INK,bg,selected);
            }
        }
        char selected[48]="Selected: ";fd_text(selected+10,icon_labels[w->selected],sizeof selected-10);
        text(w->x+2,w->y+w->h-3,selected,w->w-4,0);
        button(w->x+2,w->y+w->h-2,9,icon_view?"List":"Icons",0);
        button(w->x+12,w->y+w->h-2,8,"Open",0);
        text(w->x+22,w->y+w->h-2,"Enter opens",w->w-24,0);
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
        text(w->x + 2, w->y + w->h - 3, dirty ? "NOTE.TXT *" : "NOTE.TXT", w->w - 4, 0);
        button(w->x + 12, w->y + w->h - 2, 7, "Copy", 0);
        button(w->x + 20, w->y + w->h - 2, 6, "Cut", 0);
        if (w->w >= 40) button(w->x + 27, w->y + w->h - 2, 8, "Paste", 0);
        if (w->w >= 46) button(w->x + 36, w->y + w->h - 2, 7, "All", 0);
    } else if (id == CALCULATOR) {
        draw_calculator(w);
    } else if (id == CLOCK) {
        clock_controls(w);
    } else if (id == HELP) {
        static const char *help[] = {"F1 Help   F2 Launcher   F3 Files", "F4 Memo   F5 About      F9: Paint", "Drag title: move / corner: resize", "F6 then arrows: move window", "F7 then arrows: resize window", "F8: minimize  Tab/taskbar: restore", "Esc: close    F10: exit to DOS", "Memo: ASCII text, Ctrl+S saves", "Apps V: icons/list; Calc + Clock"};
        for (int i = 0; i < 9; i++) line(w, i, help[i], 0);
    } else {
        line(w, 0, "Kuma Desktop 1.3", 0);
        line(w, 2, "Original PC-98 window system", 0);
        line(w, 3, gfx_colors() == 256 ? "MS-DOS 4.0 / 640 x 400 / 256 colors" : "MS-DOS 4.0 / 640 x 400 / 16 colors", 0);
        line(w, 5, "Copyright 2026 kumakumapon", 0);
        line(w, 6, "MIT license - see KDLIC.TXT", 0);
    }
    for (int i = 0; i < 3; i++)
        gfx_rect((w->x + w->w) * 8 - 7 + i * 2, (w->y + w->h) * 16 - 5 - i * 3, 1, 3 + i * 3, MUTED);
    gfx_origin(0, 0);
}
static void draw(void) {
    scene_dirty=0;
    gfx_rect(0, 0, 640, 400, DESKTOP); gfx_clear_text(0, 0, 80, 25);
    if (gfx_colors() == 256) for (int y = 16; y < 368; y++)
        gfx_rect(0, y, 640, 1, 232 + (y - 16) * 23 / 351);
    else for (int y = 64; y < 352; y += 32) gfx_rect(0, y, 640, 1, DESKTOP_LIGHT);
    label(60, 21, "PC-98 / MS-DOS", 18, HANDLE, DESKTOP, 0);
    gfx_rect(0, 0, 640, 16, INK);
    label(2, 0, "Kuma Desktop", 16, WHITE, INK, 0);
    static const char *menus[] = {"Apps", "Files", "Memo", "Help", "About", "Paint", "Exit"};
    static const int menu_x[] = {22, 30, 39, 47, 55, 63, 71};
    for (int i = 0; i < 7; i++) button(menu_x[i], 0, 7, menus[i], 0);
    for (int i = 0; i < WINDOWS; i++) {
        int id = order[i];
        if (id != active && windows[id].open && !windows[id].hidden) draw_window(id);
    }
    gfx_rect(0, 368, 640, 16, BORDER); gfx_rect(0, 384, 640, 16, SURFACE);
    label(1, 24, "F1 Help / Tab switch", 22, INK, SURFACE, 0);
    static const char *tasks[] = {"Apps", "Files", "Memo", "Help", "About", "Calc", "Clock"};
    for (int i = 0; i < WINDOWS; i++) if (windows[i].open) button(24 + i * 8, 24, 8, tasks[i], active == i);
    if (!confirm) {
        gfx_background();
        if (active >= 0 && !windows[active].hidden) draw_window(active);
        gfx_rect(0, 368, 640, 16, BORDER); gfx_rect(0, 384, 640, 16, SURFACE);
        label(1, 24, "F1 Help / Tab switch", 22, INK, SURFACE, 0);
        for (int i = 0; i < WINDOWS; i++) if (windows[i].open) button(24 + i * 8, 24, 8, tasks[i], active == i);
    } else if (active >= 0 && !windows[active].hidden) draw_window(active);
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
static int paste_memo(void) {
    char incoming[1024]; int n = clipboard.length, length = 0;
    if (memo_blocked || n >= CLIP_CAPACITY) { message("Cannot paste into this memo"); return 0; }
    for (int i = 0; i < n; i++) {
        unsigned char ch = clipboard.text[i];
        if (ch == '\r') { if (i + 1 < n && clipboard.text[i + 1] == '\n') continue; ch = '\n'; }
        if (!(ch == '\n' || (ch >= 32 && ch <= 126))) { message("Clipboard needs ASCII text; use Text editor for Japanese"); return 0; }
        incoming[length++] = ch;
    }
    int start = select_all ? 0 : used;
    if (start + length > 1023) { message("Clipboard exceeds memo capacity; text retained"); return 0; }
    for (int i = 0; i < length; i++) memo[start + i] = incoming[i];
    if (length || select_all) dirty = 1;
    used = start + length; select_all = 0; message("Pasted clipboard"); return 1;
}
static int handle_key(int key) {
    int decision = handle_confirm(key);
    if (decision == 2) return 1;
    if (decision) return 0;
    if (key == KEY_F1 + 8) { tool(6, ""); return 0; }
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
    if (key == KEY_F1 + 5 || key == KEY_F1 + 6) {
        int wanted = key == KEY_F1 + 5 ? 1 : 2;
        mode = mode == wanted ? 0 : wanted;
        if (mode) w->ox = w->oy = 0; /* Keyboard geometry uses whole cells. */
        return 0;
    }
    if (mode && (key == 13 || key == 27)) { mode = 0; return 0; }
    if (mode) {
        int dx = key == KEY_RIGHT ? 1 : key == KEY_LEFT ? -1 : 0;
        int dy = key == KEY_DOWN ? 1 : key == KEY_UP ? -1 : 0;
        if (mode == 1) {
            if (w->x + dx >= 0 && w->x + dx + w->w < 80) w->x += dx;
            if (w->y + dy >= 1 && w->y + dy + w->h < 24) w->y += dy;
        } else {
            if (w->w + dx >= (active == CALCULATOR ? 36 : active == CLOCK ? 32 : 30) && w->x + w->w + dx < 80) w->w += dx;
            if (w->h + dy >= (active == CALCULATOR ? 20 : active == CLOCK ? 15 : 10) && w->y + w->h + dy < 24) w->h += dy;
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
        if(active==LAUNCHER&&(key=='v'||key=='V')) {icon_view=!icon_view;message(icon_view?"Launcher icon view":"Launcher list view");return 0;}
        int limit = active == LAUNCHER ? LAUNCH_APPS : count;
        int step=active==LAUNCHER&&icon_view?launcher_columns(w):1;
        if (key == KEY_UP && w->selected > 0) w->selected = w->selected > step ? w->selected - step : 0;
        if (key == KEY_DOWN && w->selected + 1 < limit) w->selected = w->selected + step < limit ? w->selected + step : limit-1;
        if(active==LAUNCHER&&icon_view&&key==KEY_LEFT&&w->selected>0) w->selected--;
        if(active==LAUNCHER&&icon_view&&key==KEY_RIGHT&&w->selected+1<limit) w->selected++;
        if (key == KEY_HOME) w->selected = 0;
        if (key == KEY_END && limit) w->selected = limit - 1;
        if (key == 13) { if (active == LAUNCHER) tool(w->selected, ""); else file_open(); }
        if (active == FILES && key == 8) { fd_parent(folder); w->selected = 0; refresh(); }
    } else if (active == CALCULATOR) {
        calculator_key(key);
    } else if (active == CLOCK) {
        if(key=='t'||key=='T') {twelve_hour=!twelve_hour;message(twelve_hour?"Clock 12-hour display":"Clock 24-hour display");}
        if(key==3) {char value[32];clock_text(&desk_clock,twelve_hour,1,value);clip_copy(value,fd_len(value));message("Copied clock date/time");}
    } else if (active == MEMO) {
        if (memo_blocked) { message("Use Text editor for non-ASCII or larger NOTE.TXT"); return 0; }
        if (key == 1) { select_all = !select_all; message(select_all ? "Selected all memo text" : "Selection cleared"); }
        else if (key == 3 || key == 24) {
            clip_copy(memo, used); message("Copied memo to clipboard");
            if (key == 24 && used) { used = 0; dirty = 1; select_all = 0; message("Cut memo to clipboard"); }
        } else if (key == 22) paste_memo();
        else if (key == 19) save();
        else if (key == 8 && select_all) { used = 0; dirty = 1; select_all = 0; }
        else if (key == 8 && used) { used--; dirty = 1; }
        else if ((key == 13 || (key >= 32 && key <= 126)) && (select_all || used < (int)sizeof memo - 1)) {
            if (select_all) { used = 0; select_all = 0; }
            memo[used++] = key == 13 ? '\n' : key; dirty = 1;
        } else if (used == (int)sizeof memo - 1) message("Memo full (1023 characters); use the Text editor for larger files");
    }
    return 0;
}

static int live_drag, drag, drag_id, drag_mx, drag_my, drag_x, drag_y, drag_w, drag_h;
static int preview_x, preview_y, preview_w, preview_h, preview_visible;
static int last_file = -1, last_app = -1;
static u32 app_click;
static u32 last_click;
static int clamp(int n, int low, int high) { return n < low ? low : n > high ? high : n; }
static int mouse_event(Mouse now, int previous, int *redraw) {
    int cx = now.x / 8, cy = now.y / 16;
    if (drag && (previous & 1)) {
        Window *w = &windows[drag_id];
        if (!w->open || w->hidden || confirm) { drag = 0; return 0; }
        int x = w->x, y = w->y, width = w->w, height = w->h;
        if (drag == 1) {
            int px = clamp(drag_x + now.x - drag_mx, 0, 640 - w->w * 8 - 5);
            int py = clamp(drag_y + now.y - drag_my, 16, 368 - w->h * 16);
            if (live_drag) gfx_drag_move(px, py);
            x = px / 8; y = py / 16;
            if (!(now.buttons & 1)) { w->ox = gfx_colors() == 256 ? px & 7 : 0; w->oy = gfx_colors() == 256 ? py & 15 : 0; gfx_drag_end(); live_drag = 0; }
        } else {
            width = clamp(drag_w + (now.x - drag_mx) / 8, drag_id == CALCULATOR ? 36 : drag_id == CLOCK ? 32 : 30, 79 - w->x);
            height = clamp(drag_h + (now.y - drag_my) / 16, drag_id == CALCULATOR ? 20 : drag_id == CLOCK ? 15 : 10, 23 - w->y - (w->oy != 0));
        }
        gfx_origin(w->ox, w->oy);
        if (preview_visible) gfx_outline(preview_x, preview_y, preview_w, preview_h);
        preview_visible = 0;
        if (!(now.buttons & 1)) {
            w->x = x; w->y = y; w->w = width; w->h = height;
            drag = 0; *redraw = 1;
        } else if (!live_drag) {
            preview_x = x; preview_y = y; preview_w = width; preview_h = height;
            gfx_outline(x, y, width, height); preview_visible = 1;
        }
        gfx_origin(0, 0);
        return 0;
    }
    if (!(now.buttons & 1)) { drag = 0; return 0; }
    if (previous & 1) return 0;
    int previous_file = last_file, previous_app = last_app;
    last_app = -1;
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
        static const int menu_x[] = {22, 30, 39, 47, 55, 63, 71};
        static const int menu_key[] = {KEY_F1 + 1, KEY_F1 + 2, KEY_F1 + 3, KEY_F1, KEY_F1 + 4, KEY_F1 + 8, KEY_F1 + 9};
        for (int i = 0; i < 7; i++) if (cx >= menu_x[i] && cx < menu_x[i] + 7) return menu_key[i];
        return 0;
    }
    if (cy == 24 && cx >= 24) {
        int id = (cx - 24) / 8;
        if (id < WINDOWS && windows[id].open) focus(id);
        return 0;
    }
    for (int i = WINDOWS - 1; i >= 0; i--) {
        int id = order[i]; Window *w = &windows[id];
        cx = (now.x - w->ox) / 8; cy = (now.y - w->oy) / 16;
        if (!w->open || w->hidden || now.x < w->x * 8 + w->ox || now.x >= (w->x + w->w) * 8 + w->ox ||
            now.y < w->y * 16 + w->oy || now.y >= (w->y + w->h) * 16 + w->oy) continue;
        focus(id);
        if (cy == w->y + 1 && cx >= w->x + w->w - 5 && cx < w->x + w->w - 2) return 27;
        if (cy == w->y + 1 && cx >= w->x + w->w - 9 && cx < w->x + w->w - 6) return KEY_F1 + 7;
        if (cx >= w->x + w->w - 2 && cy >= w->y + w->h - 2) drag = 2;
        else if (cy <= w->y + 1) drag = 1;
        if (drag) {
            last_file = -1; drag_id = id; drag_mx = now.x; drag_my = now.y;
            drag_x = w->x * 8 + w->ox; drag_y = w->y * 16 + w->oy; drag_w = w->w; drag_h = w->h; return 0;
        }
        if (id == LAUNCHER) {
            if(cy==w->y+w->h-2) {
                if(cx>=w->x+2&&cx<w->x+11)return 'v';
                if(cx>=w->x+12&&cx<w->x+20)return 13;
            }
            int row=cy-w->y-3,col=cx-w->x-2,columns=icon_view?launcher_columns(w):1;
            if(row>=0&&row<launcher_rows(w)*(icon_view?4:2)&&col>=0&&col<w->w-4) {
                if(icon_view&&(row%4==3||col%14==13||col/14>=columns))return 0;
                if(!icon_view&&(row&1))return 0;
                int selected=launcher_top(w)+row/(icon_view?4:2)*columns+(icon_view?col/14:0);
                if(selected<LAUNCH_APPS) {
                    w->selected=selected;
                    if(!icon_view||(previous_app==selected&&((now.ticks-app_click)&0xffffff)<153600)) tool(selected,"");
                    else {last_app=selected;app_click=now.ticks;}
                }
            }
        } else if(id == CALCULATOR) {
            if(cy==w->y+w->h-2) {
                int col=cx-w->x;
                if(col>=2&&col<10)return 3;
                if(col>=11&&col<20)return 22;
                if(col>=21&&col<26)return '%';
            }
            int row=cy-w->y-6,col=cx-w->x-(w->w-32)/2;
            if(row>=0&&row<12&&!(row&1)&&col>=0&&col<32&&col%8<7)return calc_keys[row/2][col/8];
        } else if(id == CLOCK && cy==w->y+w->h-2) {
            int col=cx-w->x;
            if(col>=2&&col<12)return 't';
            if(col>=13&&col<21)return 3;
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
        } else if (id == MEMO && cy == w->y + w->h - 2) {
            int col = cx - w->x;
            if (col >= 2 && col < 11) save();
            else if (col >= 12 && col < 19) return 3;
            else if (col >= 20 && col < 26) return 24;
            else if (w->w >= 40 && col >= 27 && col < 35) return 22;
            else if (w->w >= 46 && col >= 36 && col < 43) return 1;
        }
        return 0;
    }
    last_file = -1; return 0;
}
static int clock_update(int moving) {
    if(!windows[CLOCK].open || ((input_ticks()-clock_tick)&0xffffff)<153600) return 0;
    clock_tick=input_ticks();DeskClock next;clock_read(&next);
    int changed=next.second!=desk_clock.second||next.minute!=desk_clock.minute||next.hour!=desk_clock.hour||next.day!=desk_clock.day||next.month!=desk_clock.month||next.year!=desk_clock.year||next.valid!=desk_clock.valid;
    desk_clock=next;
    if(!changed||windows[CLOCK].hidden||confirm||moving||mode)return 0;
    Window *w=&windows[CLOCK];int x=w->x*8+w->ox+w->w*4-100,y=w->y*16+w->oy+48,above=0;
    for(int i=0;i<WINDOWS;i++) {
        int id=order[i];if(id==CLOCK){above=1;continue;}Window *other=&windows[id];
        if(above&&other->open&&!other->hidden&&x<other->x*8+other->ox+other->w*8+5&&x+200>other->x*8+other->ox&&y<other->y*16+other->oy+other->h*16+5&&y+136>other->y*16+other->oy)return 1;
    }
    gfx_pointer_hide();gfx_origin(w->ox,w->oy);clock_draw(&desk_clock,twelve_hour,w->x*8,w->y*16,w->w*8);gfx_origin(0,0);
    Mouse now=mouse_position();gfx_pointer_show(now.x,now.y);return 0;
}
int main(void) {
    p_init();
    if (p_cwd(home) < 0) { p_output("Cannot read working directory\r\n"); return 1; }
    fd_text(folder, home, sizeof folder);
    if (fd_path(note_path, home, "NOTE.TXT")) return 1;
    for (int i = 0; i < WINDOWS; i++) {
        windows[i] = (Window){3 + i * 4, 2 + i, 48, 16, 0, 0, 0, 0, 0}; order[i] = i;
    }
    windows[CALCULATOR]=(Window){5,2,38,20,0,0,0,0,0};
    windows[CLOCK]=(Window){39,3,38,17,0,0,0,0,0};
    calc_init(&calculator);clock_read(&desk_clock);clock_tick=input_ticks();
    load(); refresh(); focus(LAUNCHER); enter_graphics();
    draw(); Mouse previous = mouse_read(); gfx_pointer_show(previous.x, previous.y);
    for (;;) {
        Mouse now = mouse_read(); int ready = key_ready(), redraw = clock_update(drag), stop = 0;
        int partial=active==CALCULATOR&&!drag&&!mode&&!confirm&&!redraw&&!scene_dirty;
        if (clipboard.command) {
            int request = clipboard.command; clipboard.result = 0;
            if (request != 1 || confirm || mode || memo_blocked) clipboard.result = 1;
            else { focus(MEMO); if (!paste_memo()) clipboard.result = 2; }
            clipboard.command = 0; clipboard.sequence++; redraw = 1;
        }
        if (now.x != previous.x || now.y != previous.y || now.buttons != previous.buttons || ready || redraw) {
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
                drag = 0; gfx_drag_end(); live_drag = 0; last_file = last_app = -1; redraw = 1;
                for (int i = 0; i < 16 && key_ready(); i++)
                    if (handle_key(key_read())) { stop = 1; break; }
                if (stop) break;
            }
            if (redraw) {
                preview_visible=0;
                if(partial&&active==CALCULATOR&&!drag&&!mode&&!confirm&&!scene_dirty) {
                    Window *w=&windows[CALCULATOR];gfx_origin(w->ox,w->oy);calculator_value(w);gfx_origin(0,0);
                    gfx_rect(0,368,640,16,BORDER);gfx_clear_text(0,23,80,1);label(1,23,status,78,WHITE,BORDER,0);
                } else draw();
            }
            if (drag == 1 && !live_drag) {
                Window *w = &windows[drag_id];
                live_drag = gfx_drag_begin(w->x * 8 + w->ox, w->y * 16 + w->oy, w->w * 8 + 5, w->h * 16 + 5);
            }
            gfx_pointer_show(now.x, now.y);
        }
        previous = now; input_idle();
    }
    clip_state(0); gfx_stop(); p_finish(); p_output("KDESK exited.\r\n"); return 0;
}
