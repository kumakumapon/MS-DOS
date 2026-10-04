#include "editor.h"

#define ROWS 21
static Editor editor;
static char message[81], original[FD_PATH], search[81];
static int top, left;

static void field(int x, int y, int width, const char *s, int highlight) {
    int i = 0;
    while (i < width && s[i]) { p_cell(x + i, y, s[i], highlight); i++; }
    while (i < width) p_cell(x + i++, y, ' ', highlight);
}
static void result(int error, const char *success) {
    const char *s = success;
    if (error == 8) s = "Text exceeds the 16 KiB limit";
    else if (error == ED_BAD_TEXT) s = "Not a Shift_JIS text file (binary or invalid byte pair)";
    else if (error == ED_RECOVERY || error == ED_BACKUP_KEPT) s = "Recovery file: ";
    else if (error == 5) s = "Access denied / read-only file";
    else if (error == 112) s = "Disk full; original file preserved";
    else if (error == FD_BADNAME) s = "Use an ASCII 8.3 file name (no device names)";
    else if (error) s = "DOS file operation failed";
    fd_text(message, s, sizeof message);
    if (error == ED_RECOVERY || error == ED_BACKUP_KEPT)
        fd_text(message + fd_len(message), editor.recovery, sizeof message - fd_len(message));
}
static int line_number(int at) {
    int n = 0;
    for (int i = 0; i < at; i++) if (editor.data[i] == '\n') n++;
    return n;
}
static void render(void) {
    char number[12];
    int current = line_number(editor.cursor), col = ed_column(&editor);
    if (current < top) top = current;
    if (current >= top + ROWS) top = current - ROWS + 1;
    if (col < left) left = col;
    if (col >= left + 78) left = col - 77;
    for (int y = 0; y < 25; y++) field(0, y, 80, "", 0);
    field(0, 0, 80, "EDIT98 - Shift_JIS text editor", 1);
    field(52, 0, 4, "Ln:", 1);
    fd_number(number, (u32)current + 1); field(56, 0, 6, number, 1);
    field(64, 0, 4, "Col:", 1);
    fd_number(number, (u32)col + 1); field(69, 0, 9, number, 1);
    field(0, 1, 80, editor.path[0] ? editor.path : "[New file]", 0);
    if (editor.dirty) field(76, 1, 4, " * ", 1);
    int at = 0, line = 0;
    while (line < top && at < editor.length) { at = ed_end(&editor, at); if (at < editor.length) at++; line++; }
    for (int row = 0; row < ROWS; row++) {
        int end = ed_end(&editor, at), column = 0;
        while (at < end) {
            int next = ed_next(&editor, at), width = ed_width(&editor, at, column);
            int x = column - left, highlight = at == editor.cursor;
            if (x >= 0 && x + width <= 80) {
                if (next - at == 2) p_pair(x, row + 2, editor.data[at], editor.data[at + 1], highlight);
                else if (editor.data[at] == '\t') {
                    for (int n = 0; n < width; n++) p_cell(x + n, row + 2, ' ', highlight && !n);
                } else p_cell(x, row + 2, (char)editor.data[at], highlight);
            }
            column += width; at = next;
        }
        if (at == editor.cursor && column >= left && column < left + 80)
            p_cell(column - left, row + 2, '_', 1);
        if (at == editor.length) break;
        at++;
    }
    field(0, 23, 80, message, 0);
    field(0, 24, 80, "F1 Help F2 Open F3 Save F4 SaveAs F5 Find F6 Next F10 Quit", 1);
}
/* Prompts append whole characters; Ctrl-U clears, Backspace removes one character. */
static int prompt(const char *label, char *value, int capacity, int japanese) {
    int length = fd_len(value), pending = 0;
    for (;;) {
        field(0, 23, 80, label, 1); field(0, 24, 80, "", 0);
        int x = 0;
        for (int i = 0; i < length && x < 78; i++, x++) {
            if (sjis_lead((u8)value[i]) && i + 1 < length) {
                p_pair(x, 24, (u8)value[i], (u8)value[i + 1], 0); i++; x++;
            } else p_cell(x, 24, value[i], 0);
        }
        p_cell(x, 24, '_', 1);
        int key = p_key();
        if (pending) {
            if (key < 256 && sjis_trail((u8)key) && length + 2 < capacity) {
                value[length++] = (char)pending; value[length++] = (char)key;
                value[length] = 0; pending = 0; continue;
            }
            pending = 0;
        }
        if (key == 27) return 0;
        if (key == 13) return 1;
        if (key == 21) length = 0;
        else if (key == 8 && length) {
            int previous = 0;
            for (int i = 0; i < length;) { previous = i; i += sjis_lead((u8)value[i]) ? 2 : 1; }
            length = previous;
        } else if (japanese && key < 256 && sjis_lead((u8)key)) pending = key;
        else if (((key >= 32 && key < 127) || (japanese && key >= 0xa1 && key <= 0xdf)) && length + 1 < capacity)
            value[length++] = (char)key;
        value[length] = 0;
    }
}
static int confirm(const char *label) {
    field(0, 23, 80, label, 1);
    int key = p_key();
    return key == 'y' || key == 'Y';
}
static int save(int as) {
    char old[FD_PATH], input[FD_PATH], path[FD_PATH];
    fd_text(old, editor.path, FD_PATH);
    if (as || !editor.path[0]) {
        fd_text(input, editor.path, FD_PATH);
        if (!prompt("Save as: Enter=accept Esc=cancel Ctrl-U=clear", input, FD_PATH, 0)) return 0;
        int error = fd_path(path, original, input);
        if (error) { result(error, ""); return 0; }
        Entry entry;
        if (!p_stat(path, &entry) && !confirm("Destination exists. Replace? Y=yes Other=cancel")) return 0;
        fd_text(editor.path, path, FD_PATH);
    }
    int error = ed_save(&editor);
    result(error, "Saved (Shift_JIS / CRLF)");
    if (error && error != ED_BACKUP_KEPT) { fd_text(editor.path, old, FD_PATH); return 0; }
    return 1;
}
static int may_discard(void) {
    if (!editor.dirty) return 1;
    field(0, 23, 80, "Unsaved changes. Y=save N=discard Esc=cancel", 1);
    int key = p_key();
    if (key == 'y' || key == 'Y') return save(0);
    return key == 'n' || key == 'N';
}
static void open_file(void) {
    char input[FD_PATH], path[FD_PATH];
    if (!may_discard()) return;
    fd_text(input, editor.path, FD_PATH);
    if (!prompt("Open / new file: Enter=accept Esc=cancel Ctrl-U=clear", input, FD_PATH, 0)) return;
    int error = fd_path(path, original, input);
    if (!error) error = ed_load(&editor, path);
    if (!error) top = left = 0;
    result(error, "Opened");
}
static void find(int ask) {
    if (ask && !prompt("Find (Shift_JIS): Enter=accept Esc=cancel", search, sizeof search, 1)) return;
    int length = fd_len(search);
    if (!length) return;
    int start = ed_next(&editor, editor.cursor), at = start;
    for (int pass = 0; pass < 2; pass++) {
        int stop = pass ? start : editor.length;
        while (at < stop) {
            int n = 0;
            while (n < length && at + n < editor.length && editor.data[at + n] == (u8)search[n]) n++;
            if (n == length) { editor.cursor = at; editor.desired = -1; result(0, "Found"); return; }
            at = ed_next(&editor, at);
        }
        at = 0;
    }
    result(0, "Text not found");
}
static void help(void) {
    p_screen();
    const char *lines[] = {
        "EDIT98 - native PC-98 text editor / 16 KiB per file",
        "F2 Open/new   F3 or Ctrl-S Save   F4 Save as   F10 or Esc Quit",
        "Arrows Move   HOME / Ctrl-A Line start   Ctrl-E Line end",
        "ROLL UP/DOWN Page   Backspace/Delete Remove whole character",
        "Ctrl-Y Delete line   Ctrl-Z Undo/redo last edit   Enter New line",
        "F5 or Ctrl-F Find   F6 Find next   Tab Insert tab",
        "Japanese: open WebNP2 text input, use your OS IME, then Send.",
        "Text: Shift_JIS, half-width kana, CRLF output; ASCII 8.3 names.",
        "Saving writes a temporary file before replacing the original.",
        "Invalid/binary text is rejected. Read-only files need Save as.",
        "Open and Save clear undo history. Long lines scroll horizontally.",
        "Press any key to return."
    };
    for (unsigned i = 0; i < sizeof lines / sizeof lines[0]; i++) field(0, (int)i + 1, 80, lines[i], !i);
    p_key(); p_screen();
}
int main(void) {
    p_init(); p_cwd(original); ed_init(&editor);
    u32 address = 0x80;
    __asm__ volatile ("" : "+r"(address));
    volatile u8 *tail = (volatile u8 *)address;
    char argument[FD_PATH], path[FD_PATH];
    int length = tail[0], first = 0, n = 0;
    while (first < length && tail[first + 1] == ' ') first++;
    while (first < length && n < FD_PATH - 1) argument[n++] = (char)tail[++first];
    while (n && argument[n - 1] == ' ') n--;
    argument[n] = 0;
    if (n) {
        int error = fd_path(path, original, argument);
        if (!error) error = ed_load(&editor, path);
        result(error, "Opened");
    } else result(0, "New file. F1: help. F3: save.");
    p_screen();
    int pending = 0;
    for (;;) {
        render();
        int key = p_key();
        if (pending) {
            if (key < 256 && sjis_trail((u8)key)) {
                u8 pair[2] = {(u8)pending, (u8)key};
                result(ed_insert(&editor, pair, 2), ""); pending = 0; continue;
            }
            pending = 0; result(ED_BAD_TEXT, "");
        }
        if (key == 27 || key == KEY_F1 + 9) { if (may_discard()) break; }
        else if (key == KEY_F1) help();
        else if (key == KEY_F1 + 1) open_file();
        else if (key == KEY_F1 + 2 || key == 19) save(0);
        else if (key == KEY_F1 + 3) save(1);
        else if (key == KEY_F1 + 4 || key == 6) find(1);
        else if (key == KEY_F1 + 5) find(0);
        else if (key == KEY_LEFT) { editor.cursor = ed_prev(&editor, editor.cursor); editor.desired = -1; }
        else if (key == KEY_RIGHT) { editor.cursor = ed_next(&editor, editor.cursor); editor.desired = -1; }
        else if (key == KEY_HOME || key == 1) { editor.cursor = ed_start(&editor, editor.cursor); editor.desired = -1; }
        else if (key == KEY_END || key == 5) { editor.cursor = ed_end(&editor, editor.cursor); editor.desired = -1; }
        else if (key == KEY_UP || key == KEY_DOWN) ed_vertical(&editor, key == KEY_UP ? -1 : 1);
        else if (key == KEY_PAGEUP || key == KEY_PAGEDOWN)
            for (int i = 0; i < ROWS; i++) ed_vertical(&editor, key == KEY_PAGEUP ? -1 : 1);
        else if (key == 8) ed_delete(&editor, 1);
        else if (key == KEY_DELETE || key == 127) ed_delete(&editor, 0);
        else if (key == 25) ed_delete_line(&editor);
        else if (key == 26) ed_undo(&editor);
        else if (key < 256 && sjis_lead((u8)key)) pending = key;
        else if ((key >= 32 && key < 127) || (key >= 0xa1 && key <= 0xdf) || key == 9 || key == 13) {
            u8 c = key == 13 ? '\n' : (u8)key;
            result(ed_insert(&editor, &c, 1), "");
        }
    }
    p_finish(); p_output("EDIT98 exited.\r\n");
    return 0;
}
