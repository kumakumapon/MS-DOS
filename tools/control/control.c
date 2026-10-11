#include "../filer/filer.h"

#define CAP 2048

typedef struct {
    int xms, ems, files, buffers, filer;
} Settings;

static char input[CAP + 1], output[CAP + 1], message[80];
static Settings settings;

static char upper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c; }
static int equal(const char *a, const char *b) {
    while (*a && *b && upper(*a) == upper(*b)) { a++; b++; }
    return !*a && !*b;
}
static int begins(const char *a, const char *b) {
    while (*b) { if (upper(*a++) != upper(*b++)) return 0; }
    return 1;
}
static int equal_line(const char *a, const char *b, int length) {
    int i = 0;
    while (i < length && b[i] && upper(a[i]) == upper(b[i])) i++;
    return i == length && !b[i];
}
static int number(const char *s) {
    int n = 0;
    while (*s == ' ' || *s == '\t') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return n;
}
static void line(int y, const char *s, int hi) {
    int x = 0;
    while (s[x] && x < 80) { p_cell(x, y, s[x], hi); x++; }
    while (x < 80) p_cell(x++, y, ' ', hi);
}
static void put_number(char *out, unsigned n) {
    fd_number(out, n);
}
static void render(void) {
    char value[12], row[80];
    line(0, "PC-98 SYSTEM SETTINGS", 1);
    line(2, "Changes are saved to A: and take effect after reboot.", 0);
    line(4, "MEMORY DRIVERS  (CONFIG.SYS)", 1);
    line(5, settings.xms ? " XMS manager FDXMS286.SYS : ENABLED" : " XMS manager FDXMS286.SYS : DISABLED", 0);
    line(6, settings.ems ? " EMS manager EMM386.EXE   : ENABLED (8192 KiB pool)" : " EMS manager EMM386.EXE   : DISABLED", 0);
    line(8, "DOS STARTUP  (CONFIG.SYS)", 1);
    row[0] = 0; fd_text(row, " FILES=", sizeof row);
    put_number(value, (unsigned)settings.files); fd_text(row + fd_len(row), value, sizeof row - fd_len(row));
    fd_text(row + fd_len(row), "   BUFFERS=", sizeof row - fd_len(row));
    put_number(value, (unsigned)settings.buffers); fd_text(row + fd_len(row), value, sizeof row - fd_len(row));
    line(9, row, 0);
    line(11, "AUTOEXEC.BAT", 1);
    line(12, settings.filer ? " Start FD98 file manager automatically : YES" : " Start FD98 file manager automatically : NO", 0);
    line(15, "X  Toggle XMS     E  Toggle EMS (requires XMS)", 0);
    line(16, "+/-  FILES 20-60  [/]  BUFFERS 8/16", 0);
    line(17, "F  Toggle FD98 startup", 0);
    line(19, "S  Save settings    R  Reload files    Q/Esc  Exit", 1);
    line(22, message, 0);
}
static int read_file(const char *path, char *data, int capacity) {
    int handle = p_open(path);
    if (handle == -2) return 0;
    if (handle < 0) return -handle;
    int size = 0, error = 0;
    while (size < capacity) {
        int count = p_read(handle, data + size, capacity - size);
        if (count < 0) { error = -count; break; }
        if (!count) break;
        size += count;
    }
    if (!error && size == capacity) {
        char extra;
        int count = p_read(handle, &extra, 1);
        if (count > 0) error = 8;
        else if (count < 0) error = -count;
    }
    int closed = p_close(handle);
    if (!error && closed < 0) error = -closed;
    if (error) return -error;
    data[size] = 0;
    return size;
}
static int managed_config(const char *text) {
    char linebuf[80];
    int n = fd_len(text);
    while (n && (text[n - 1] == ' ' || text[n - 1] == '\t')) n--;
    int start = 0;
    while (start < n && (text[start] == ' ' || text[start] == '\t')) start++;
    int length = n - start;
    if (length >= (int)sizeof linebuf) return 0;
    for (int i = 0; i < length; i++) linebuf[i] = text[start + i];
    linebuf[length] = 0;
    return begins(linebuf, "FILES=") || begins(linebuf, "BUFFERS=") ||
           equal(linebuf, "DEVICE=FDXMS286.SYS") ||
           begins(linebuf, "DEVICE=EMM386.EXE");
}
static int scan_config(void) {
    int size = read_file("CONFIG.SYS", input, CAP);
    if (size < 0) return -size;
    settings.xms = settings.ems = 0;
    settings.files = 20; settings.buffers = 8;
    for (int at = 0; at < size;) {
        int end = at;
        while (end < size && input[end] != '\r' && input[end] != '\n') end++;
        int length = end - at;
        char one[80];
        if (length < (int)sizeof one) {
            for (int i = 0; i < length; i++) one[i] = input[at + i];
            one[length] = 0;
            if (begins(one, "FILES=")) {
                int n = number(one + 6);
                if (n >= 20 && n <= 60) settings.files = n;
            } else if (begins(one, "BUFFERS=")) {
                int n = number(one + 8);
                if (n == 8 || n == 16) settings.buffers = n;
            } else if (equal(one, "DEVICE=FDXMS286.SYS")) settings.xms = 1;
            else if (begins(one, "DEVICE=EMM386.EXE")) settings.ems = 1;
        }
        while (end < size && (input[end] == '\r' || input[end] == '\n')) end++;
        at = end;
    }
    return 0;
}
static int scan_autoexec(void) {
    int size = read_file("AUTOEXEC.BAT", input, CAP);
    if (size < 0) return -size;
    settings.filer = 0;
    for (int at = 0; at < size;) {
        int end = at;
        while (end < size && input[end] != '\r' && input[end] != '\n') end++;
        int length = end - at;
        if (length == 20 && equal_line(input + at, "REM CPANEL AUTO FD98", length)) {
            int next = end;
            while (next < size && (input[next] == '\r' || input[next] == '\n')) next++;
            int next_end = next;
            while (next_end < size && input[next_end] != '\r' && input[next_end] != '\n') next_end++;
            char one[32];
            int n = next_end - next;
            if (n > 0 && n < (int)sizeof one) {
                for (int i = 0; i < n; i++) one[i] = input[next + i];
                one[n] = 0;
                if (equal(one, "FD98")) settings.filer = 1;
            }
        }
        while (end < size && (input[end] == '\r' || input[end] == '\n')) end++;
        at = end;
    }
    return 0;
}
static int append(char *dest, int *used, const char *src, int count) {
    if (*used + count > CAP) return 0;
    for (int i = 0; i < count; i++) dest[(*used)++] = src[i];
    return 1;
}
static int append_text(char *dest, int *used, const char *src) {
    return append(dest, used, src, fd_len(src));
}
static int append_line(char *dest, int *used, const char *src) {
    if (*used && dest[*used - 1] != '\n' && !append(dest, used, "\r\n", 2)) return 0;
    return append_text(dest, used, src) && append(dest, used, "\r\n", 2);
}
static int build_config(char *dest, int *length) {
    int size = read_file("CONFIG.SYS", input, CAP);
    if (size < 0) return -size;
    int used = 0;
    for (int at = 0; at < size;) {
        int end = at;
        while (end < size && input[end] != '\r' && input[end] != '\n') end++;
        char one[80]; int n = end - at;
        int skip = 0;
        if (n < (int)sizeof one) {
            for (int i = 0; i < n; i++) one[i] = input[at + i];
            one[n] = 0; skip = managed_config(one);
        }
        int next = end;
        while (next < size && (input[next] == '\r' || input[next] == '\n')) next++;
        if (!skip && !append(dest, &used, input + at, next - at)) return 8;
        at = next;
    }
    char value[12], directive[32];
    directive[0] = 0; fd_text(directive, "FILES=", sizeof directive);
    put_number(value, (unsigned)settings.files); fd_text(directive + fd_len(directive), value, sizeof directive - fd_len(directive));
    if (!append_line(dest, &used, directive)) return 8;
    directive[0] = 0; fd_text(directive, "BUFFERS=", sizeof directive);
    put_number(value, (unsigned)settings.buffers); fd_text(directive + fd_len(directive), value, sizeof directive - fd_len(directive));
    if (!append_line(dest, &used, directive)) return 8;
    if (settings.xms && !append_line(dest, &used, "DEVICE=FDXMS286.SYS")) return 8;
    if (settings.ems && !append_line(dest, &used, "DEVICE=EMM386.EXE EMM=8192")) return 8;
    dest[used] = 0; *length = used;
    return 0;
}
static int build_autoexec(char *dest, int *length) {
    int size = read_file("AUTOEXEC.BAT", input, CAP);
    if (size < 0) return -size;
    int used = 0;
    for (int at = 0; at < size;) {
        int end = at;
        while (end < size && input[end] != '\r' && input[end] != '\n') end++;
        int skip = 0, next = end;
        int n = end - at;
        if (n == 20 && equal_line(input + at, "REM CPANEL AUTO FD98", n)) {
            next = end;
            while (next < size && (input[next] == '\r' || input[next] == '\n')) next++;
            int next_end = next;
            while (next_end < size && input[next_end] != '\r' && input[next_end] != '\n') next_end++;
            char one[32]; int m = next_end - next;
            if (m > 0 && m < (int)sizeof one) {
                for (int i = 0; i < m; i++) one[i] = input[next + i];
                one[m] = 0;
                if (equal(one, "FD98")) { skip = 1; end = next_end; }
            }
        }
        next = end;
        while (next < size && (input[next] == '\r' || input[next] == '\n')) next++;
        if (!skip && !append(dest, &used, input + at, next - at)) return 8;
        at = next;
    }
    if (settings.filer && (!append_line(dest, &used, "REM CPANEL AUTO FD98") ||
                           !append_line(dest, &used, "FD98"))) return 8;
    dest[used] = 0; *length = used;
    return 0;
}
static int save_file(const char *path, const char *data, int length) {
    char temp[16], backup[16];
    int handle = -1;
    for (int i = 0; i < 10; i++) {
        temp[0] = 'C'; temp[1] = 'P'; temp[2] = '0'; temp[3] = '0';
        temp[4] = '0'; temp[5] = '0'; temp[6] = (char)('0' + i); temp[7] = '0';
        temp[8] = '.'; temp[9] = 'T'; temp[10] = 'M'; temp[11] = 'P'; temp[12] = 0;
        backup[0] = temp[0]; backup[1] = temp[1]; backup[2] = temp[2]; backup[3] = temp[3];
        backup[4] = temp[4]; backup[5] = temp[5]; backup[6] = temp[6]; backup[7] = temp[7];
        backup[8] = '.'; backup[9] = 'B'; backup[10] = 'A'; backup[11] = 'K'; backup[12] = 0;
        Entry entry;
        if (p_stat(temp, &entry) == -2 && p_stat(backup, &entry) == -2) {
            handle = p_create(temp);
            if (handle >= 0) break;
        }
    }
    if (handle < 0) return 80;
    int at = 0, error = 0;
    while (at < length) {
        int count = length - at;
        if (count > 512) count = 512;
        int written = p_write(handle, data + at, count);
        if (written != count) { error = written < 0 ? -written : 112; break; }
        at += count;
    }
    int closed = p_close(handle);
    if (!error && closed < 0) error = -closed;
    if (error) { p_unlink(temp); return error; }
    Entry entry;
    int exists = p_stat(path, &entry) == 0;
    if (exists) {
        error = p_rename(path, backup);
        if (error < 0) { p_unlink(temp); return -error; }
    }
    error = p_rename(temp, path);
    if (error < 0) {
        if (exists) p_rename(backup, path);
        p_unlink(temp);
        return -error;
    }
    if (exists) p_unlink(backup);
    return 0;
}
static int save_all(void) {
    int length, error = build_config(output, &length);
    if (error) return error;
    error = save_file("CONFIG.SYS", output, length);
    if (error) return error;
    error = build_autoexec(output, &length);
    if (error) return error;
    return save_file("AUTOEXEC.BAT", output, length);
}
static void load(void) {
    int error = scan_config();
    if (!error) error = scan_autoexec();
    if (error) fd_text(message, "Read error; settings not loaded.", sizeof message);
    else fd_text(message, "Ready. S saves changes; reboot to apply.", sizeof message);
}
int main(void) {
    p_init();
    p_screen();
    load();
    for (;;) {
        render();
        int key = p_key();
        if (key == 27 || key == 'q' || key == 'Q') break;
        if (key == 'x' || key == 'X') {
            settings.xms = !settings.xms;
            if (!settings.xms) settings.ems = 0;
        } else if (key == 'e' || key == 'E') {
            if (!settings.ems) { settings.xms = 1; settings.ems = 1; }
            else settings.ems = 0;
        } else if (key == 'f' || key == 'F') settings.filer = !settings.filer;
        else if (key == '+' || key == '=') { settings.files += 10; if (settings.files > 60) settings.files = 20; }
        else if (key == '-') { settings.files -= 10; if (settings.files < 20) settings.files = 60; }
        else if (key == ']') settings.buffers = settings.buffers == 8 ? 16 : 8;
        else if (key == '[') settings.buffers = settings.buffers == 8 ? 16 : 8;
        else if (key == 'r' || key == 'R') load();
        else if (key == 's' || key == 'S') {
            int error = save_all();
            fd_text(message, error ? "Save failed; check free disk space and file access." :
                    "Saved. Reboot for CONFIG.SYS changes to take effect.", sizeof message);
            if (!error) { scan_config(); scan_autoexec(); }
        }
    }
    p_finish();
    p_output("CPANEL exited.\r\n");
    return 0;
}
