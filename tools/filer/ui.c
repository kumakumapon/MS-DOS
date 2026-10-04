#include "filer.h"
#include "sjis.h"

#define ROWS 18
static Panel panels[2];
static int active;
static char message[81], original[FD_PATH];

static void text(int x, int y, const char *value, int highlighted) {
    while (*value && x < 80) p_cell(x++, y, *value++, highlighted);
}
static void field(int x, int y, int width, const char *value, int highlighted) {
    int i = 0;
    while (i < width && value[i]) p_cell(x + i, y, value[i], highlighted), i++;
    while (i < width) p_cell(x + i++, y, ' ', highlighted);
}
static void blank(void) {
    for (int y = 0; y < 25; y++) for (int x = 0; x < 80; x++) p_cell(x, y, ' ', 0);
}
static const char *error_text(int error) {
    switch (error) {
        case 2: return "File not found";
        case 3: return "Path not found";
        case 5: return "Access denied (read-only file, directory, or nonempty directory)";
        case 15: return "Drive not available";
        case 17: return "Cannot move a directory between drives";
        case FD_EXISTS: return "Destination exists; no files overwritten";
        case 112: return "Disk full";
        case FD_BADNAME: return "Use an 8.3 name or an absolute drive path (no device names)";
        case FD_CANCELLED: return "Cancelled";
        case FD_ROLLBACK: return "Cleanup failed; inspect source and destination before retrying";
        default: return "DOS file operation failed";
    }
}
static void status(int error, const char *success) {
    if (!error) fd_text(message, success, sizeof message);
    else {
        char number[11];
        fd_text(message, error_text(error), sizeof message);
        fd_number(number, (u32)error);
        int n = fd_len(message);
        if (n + fd_len(number) + 4 < (int)sizeof message) {
            message[n++] = ' '; message[n++] = '(';
            fd_text(message + n, number, (int)sizeof message - n);
            n = fd_len(message); message[n++] = ')'; message[n] = 0;
        }
    }
}
static void refresh(void) {
    for (int i = 0; i < 2; i++) {
        int error = fd_refresh(&panels[i]);
        if (error) status(error, "");
    }
}
static Entry *selected(void) {
    Panel *panel = &panels[active];
    return panel->count ? &panel->entries[panel->selected] : 0;
}
static void render(void) {
    char number[11];
    blank();
    field(0, 0, 80, "FD Filer 1.0 - MS-DOS   [Tab: switch pane]   [F1: help]   [F10/Q: quit]", 1);
    for (int side = 0; side < 2; side++) {
        Panel *panel = &panels[side];
        int x = side * 40;
        field(x, 1, 39, panel->path, side == active);
        field(x, 2, 39, "Name          Bytes       Date    Attr", 0);
        if (panel->selected < panel->top) panel->top = panel->selected;
        if (panel->selected >= panel->top + ROWS) panel->top = panel->selected - ROWS + 1;
        if (panel->top < 0) panel->top = 0;
        for (int row = 0; row < ROWS; row++) {
            int index = panel->top + row;
            if (index >= panel->count) break;
            Entry *entry = &panel->entries[index];
            int highlight = index == panel->selected && side == active;
            field(x, 3 + row, 39, "", highlight);
            field(x, 3 + row, 13, entry->name, highlight);
            if (entry->attr & FD_DIR) field(x + 14, 3 + row, 10, "<DIR>", highlight);
            else { fd_number(number, entry->size); field(x + 14, 3 + row, 10, number, highlight); }
            u32 year = 1980 + (entry->date >> 9), month = (entry->date >> 5) & 15, day = entry->date & 31;
            char date[9];
            date[0] = (char)('0' + year / 1000 % 10); date[1] = (char)('0' + year / 100 % 10);
            date[2] = (char)('0' + year / 10 % 10); date[3] = (char)('0' + year % 10);
            date[4] = (char)('0' + month / 10); date[5] = (char)('0' + month % 10);
            date[6] = (char)('0' + day / 10); date[7] = (char)('0' + day % 10); date[8] = 0;
            if (entry->date) field(x + 25, 3 + row, 8, date, highlight);
            char attributes[5] = {
                entry->attr & 1 ? 'R' : '-', entry->attr & 2 ? 'H' : '-',
                entry->attr & 4 ? 'S' : '-', entry->attr & 0x20 ? 'A' : '-', 0
            };
            field(x + 34, 3 + row, 4, attributes, highlight);
        }
        fd_number(number, (u32)panel->count);
        text(x, 21, number, 0);
        text(x + fd_len(number), 21, panel->truncated ? " entries (list limit reached)" : " entries", 0);
    }
    for (int y = 1; y < 22; y++) p_cell(39, y, '|', 0);
    text(0, 22, active ? "Active: RIGHT  Selected: " : "Active: LEFT  Selected: ", 0);
    Entry *entry = selected();
    if (entry) text(active ? 25 : 24, 22, entry->name, 0);
    field(0, 23, 80, message, 0);
    field(0, 24, 80, "F2 Rename F3 View F4 Refresh F5 Copy F6 Move F7 Mkdir F8 Delete F9 Run E Edit", 1);
}
static int prompt(const char *label, char *value) {
    int length = fd_len(value), position = length;
    for (;;) {
        field(0, 23, 80, label, 1);
        field(0, 24, 80, "", 0);
        int start = position > 77 ? position - 77 : 0;
        field(0, 24, 79, value + start, 0);
        p_cell(position - start, 24, position == length ? '_' : value[position], 1);
        int key = p_key();
        if (key == 27) return 0;
        if (key == 13) return 1;
        if (key == 21) length = position = 0, value[0] = 0; /* Ctrl-U */
        else if (key == KEY_LEFT && position) position--;
        else if (key == KEY_RIGHT && position < length) position++;
        else if (key == KEY_HOME || key == 1) position = 0;
        else if (key == KEY_END || key == 5) position = length;
        else if ((key == 8 || key == 127) && position) {
            for (int i = position - 1; i < length; i++) value[i] = value[i + 1];
            length--; position--;
        } else if (key >= 32 && key < 127 && length < FD_PATH - 1) {
            for (int i = length; i >= position; i--) value[i + 1] = value[i];
            value[position++] = (char)key; length++;
        }
    }
}
static int entry_path(char *path) {
    Entry *entry = selected();
    if (!entry || !fd_cmp(entry->name, "..")) return 0;
    return !fd_path(path, panels[active].path, entry->name);
}
static void directory(const char *name) {
    Panel *panel = &panels[active];
    char previous[FD_PATH], path[FD_PATH];
    fd_text(previous, panel->path, FD_PATH);
    int error = fd_path(path, panel->path, name);
    if (!error) {
        fd_text(panel->path, path, FD_PATH);
        panel->selected = panel->top = panel->count = 0;
        error = fd_refresh(panel);
        if (error) { fd_text(panel->path, previous, FD_PATH); fd_refresh(panel); }
    }
    status(error, "Directory opened");
}
static void view(const char *path) {
    int handle = p_open(path);
    if (handle < 0) { status(-handle, ""); return; }
    u32 offsets[128] = {0}, position = 0;
    int page = 0, end = 0, error = 0;
    for (;;) {
        char buffer[1024];
        int used = 0, count = 0, x = 1, y = 2;
        p_screen();
        field(0, 0, 80, "FD Filer - Viewer", 1);
        text(0, 1, path, 0);
        error = p_seek(handle, offsets[page]);
        position = offsets[page];
        end = 0;
        while (!error && y < 23) {
            if (used == count) {
                count = p_read(handle, buffer, sizeof buffer); used = 0;
                if (count < 0) { error = count; break; }
                if (!count) { end = 1; break; }
            }
            unsigned char c = (u8)buffer[used++]; position++;
            if (c == '\r') continue;
            if (c == '\n') { x = 1; y++; continue; }
            if (sjis_lead(c)) {
                if (used == count) {
                    count = p_read(handle, buffer, sizeof buffer); used = 0;
                    if (count < 0) { error = count; break; }
                }
                if (count && sjis_trail((u8)buffer[used])) {
                    if (x > 77) { x = 1; y++; }
                    if (y == 23) { position--; break; }
                    p_pair(x, y, c, (u8)buffer[used++], 0); position++; x += 2;
                    if (x == 79) { x = 1; y++; }
                    continue;
                }
            }
            if (c == '\t') {
                do {
                    p_cell(x++, y, ' ', 0);
                    if (x == 79) { x = 1; y++; break; }
                } while ((x - 1) & 7);
            } else {
                p_cell(x++, y, (c >= 32 && c < 127) || (c >= 0xa1 && c <= 0xdf) ? (char)c : '.', 0);
                if (x == 79) x = 1, y++;
            }
        }
        text(0, 23, error ? "Read error" : end ? "[EOF]" : "[More]", 0);
        field(0, 24, 80, "Space/PgDn: next  PgUp: previous  Home: first  Esc/Q: return", 1);
        int key = p_key();
        if (key == 27 || key == 'q' || key == 'Q') break;
        if (key == KEY_PAGEUP || key == KEY_UP) { if (page) page--; }
        else if (key == KEY_HOME) page = 0, offsets[0] = 0;
        else if (!end && !error && (key == ' ' || key == 13 || key == KEY_PAGEDOWN || key == KEY_DOWN)) {
            if (page < 127) offsets[++page] = position;
            else { for (int i = 0; i < 127; i++) offsets[i] = offsets[i + 1]; offsets[127] = position; }
        }
    }
    int closed = p_close(handle);
    if (!error) error = closed;
    p_screen();
    status(error < 0 ? -error : error, "Viewer closed");
}
static void operation(int action) {
    char source[FD_PATH], destination[FD_PATH], input[FD_PATH];
    Entry *entry = selected();
    int error;
    if (action == 7) {
        input[0] = 0;
        if (!prompt("Create directory:  Enter=accept  Esc=cancel  Ctrl-U=clear", input)) return;
        error = fd_path(destination, panels[active].path, input);
        if (!error) error = fd_mkdir(destination);
        status(error, "Directory created");
    } else {
        if (!entry_path(source)) { status(5, ""); return; }
        if (action == 8) {
            field(0, 23, 80, "Delete selected file / EMPTY directory?  Y=yes  Any other key=cancel", 1);
            int key = p_key();
            if (key != 'y' && key != 'Y') { status(FD_CANCELLED, ""); return; }
            error = fd_remove(source);
            status(error, "Deleted");
        } else {
            const char *base = panels[action == 2 ? active : 1 - active].path;
            fd_path(input, base, entry->name);
            const char *label = action == 2 ? "Rename to:" : action == 5 ? "Copy to:" : "Move to:";
            if (!prompt(label, input)) return;
            error = fd_path(destination, base, input);
            if (!error) error = action == 2 ? fd_rename(source, destination) :
                                action == 5 ? fd_copy(source, destination) : fd_move(source, destination);
            status(error, action == 2 ? "Renamed" : action == 5 ? "Copied" : "Moved");
        }
    }
    refresh();
}
static void run(int editing) {
    char path[FD_PATH], program[FD_PATH], cwd[FD_PATH], arguments[FD_PATH];
    Entry *entry = selected();
    if (!entry_path(path) || (entry->attr & FD_DIR)) { status(5, ""); return; }
    int length = fd_len(entry->name);
    const char *ext = length > 4 ? entry->name + length - 4 : "";
    arguments[0] = 0;
    fd_text(program, path, FD_PATH);
    if (editing) {
        char root[4] = {original[0], ':', '\\', 0};
        fd_path(program, root, "EDIT98.COM");
        fd_text(arguments, path, FD_PATH);
    } else if (!fd_cmp(ext, ".BAS")) {
        char root[4] = {original[0], ':', '\\', 0};
        fd_path(program, root, "RBASIC.COM");
        fd_text(arguments, path, FD_PATH);
    } else if (fd_cmp(ext, ".COM") && fd_cmp(ext, ".EXE")) {
        fd_text(message, "F9 runs COM/EXE files or BAS files with the bundled RBASIC.COM", sizeof message);
        return;
    }
    int error = p_cwd(cwd);
    if (error) { status(-error, ""); return; }
    error = p_chdir(panels[active].path);
    if (error) { p_chdir(cwd); status(-error, ""); return; }
    p_finish();
    int code = p_exec(program, arguments);
    int restore = p_chdir(cwd);
    if (code >= 0) { p_output("\r\nProgram returned. Press any key to return to FD Filer.\r\n"); p_key(); }
    p_screen();
    if (code < 0 || restore < 0) status(code < 0 ? -code : -restore, "");
    else {
        char number[11]; fd_number(number, (u32)code);
        fd_text(message, "Program returned (exit ", sizeof message);
        fd_text(message + fd_len(message), number, (int)sizeof message - fd_len(message));
        int n = fd_len(message); message[n] = ')'; message[n + 1] = 0;
    }
    refresh();
}
static void help(void) {
    static const char *lines[] = {
        "FD Filer - Keyboard help",
        "A small, original two-pane filer for MS-DOS (not the FD-clone codebase).",
        "",
        "Up/Down or J/K: select     PgUp/PgDn: page     Home: first",
        "Tab or Left/Right: switch pane                Backspace: parent",
        "Enter: open directory / view file            F1 or ?: this help",
        "F2 or R: rename     F3 or V: text viewer      F4: refresh   E: EDIT98",
        "F5 or C: copy       F6 or M: move             F7 or N: mkdir",
        "F8 or D: delete selected file / empty directory (confirmation required)",
        "F9 or X: run COM/EXE; run BAS using RBASIC.COM on the startup drive",
        "F10 or Q/Esc: quit",
        "",
        "Prompts: Enter accept, Esc cancel, Ctrl-U clear, arrows edit.",
        "Destinations never overwrite existing files. Esc cancels a file copy.",
        "Directories are sorted first; file names use DOS 8.3 ASCII.",
        "One entry per operation. Directory copy is not recursive.",
        "Up to 256 entries per pane (255 plus parent in a subdirectory).",
        "",
        "Press any key to return."
    };
    p_screen();
    for (unsigned i = 0; i < sizeof lines / sizeof lines[0]; i++) text(0, (int)i, lines[i], i == 0);
    p_key(); p_screen();
}
int main(void) {
    p_init();
    int error = p_cwd(original);
    if (error) { p_output("FD Filer: cannot read current directory\r\n"); return 1; }
    for (int i = 0; i < 2; i++) fd_text(panels[i].path, original, FD_PATH);
    refresh(); p_screen();
    fd_text(message, "F1: help. Existing destinations are never overwritten.", sizeof message);
    for (;;) {
        render();
        int key = p_key();
        Panel *panel = &panels[active];
        if (key == 27 || key == 'q' || key == 'Q' || key == KEY_F1 + 9) break;
        if (key == KEY_UP || key == 'k' || key == 'K') { if (panel->selected) panel->selected--; }
        else if (key == KEY_DOWN || key == 'j' || key == 'J') { if (panel->selected + 1 < panel->count) panel->selected++; }
        else if (key == KEY_HOME) panel->selected = 0;
        else if (key == KEY_END) panel->selected = panel->count ? panel->count - 1 : 0;
        else if (key == KEY_PAGEUP) { panel->selected -= ROWS; if (panel->selected < 0) panel->selected = 0; }
        else if (key == KEY_PAGEDOWN) { panel->selected += ROWS; if (panel->selected >= panel->count) panel->selected = panel->count ? panel->count - 1 : 0; }
        else if (key == 9 || key == KEY_LEFT || key == KEY_RIGHT) active = 1 - active;
        else if (key == 8 || key == 127) directory("..");
        else if (key == KEY_F1 || key == '?') help();
        else if (key == KEY_F1 + 3) { refresh(); fd_text(message, "Refreshed", sizeof message); }
        else if (key == KEY_F1 + 1 || key == 'r' || key == 'R') operation(2);
        else if (key == KEY_F1 + 4 || key == 'c' || key == 'C') operation(5);
        else if (key == KEY_F1 + 5 || key == 'm' || key == 'M') operation(6);
        else if (key == KEY_F1 + 6 || key == 'n' || key == 'N') operation(7);
        else if (key == KEY_F1 + 7 || key == 'd' || key == 'D') operation(8);
        else if (key == KEY_F1 + 8 || key == 'x' || key == 'X') run(0);
        else if (key == 'e' || key == 'E') run(1);
        else if (key == 13 || key == KEY_F1 + 2 || key == 'v' || key == 'V') {
            Entry *entry = selected();
            if (entry) {
                if (key == 13 && (entry->attr & FD_DIR)) directory(entry->name);
                else if (!(entry->attr & FD_DIR)) { char path[FD_PATH]; if (entry_path(path)) view(path); }
            }
        }
    }
    p_finish();
    p_output("FD Filer exited.\r\n");
    return 0;
}
