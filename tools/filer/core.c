#include "filer.h"

int fd_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}
int fd_cmp(const char *a, const char *b) {
    while (*a && *a == *b) a++, b++;
    return (u8)*a - (u8)*b;
}
void fd_text(char *out, const char *in, int capacity) {
    int i = 0;
    while (i + 1 < capacity && in[i]) out[i] = in[i], i++;
    out[i] = 0;
}
void fd_number(char *out, u32 value) {
    char reverse[11];
    int n = 0, i = 0;
    do { reverse[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) out[i++] = reverse[--n];
    out[i] = 0;
}
static int upper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static int letter(int c) { return c >= 'A' && c <= 'Z'; }
static int valid_name(const char *name) {
    const char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$-!#%&'()@^{}~";
    char stem[9];
    int base = 0, ext = -1;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') {
            if (ext >= 0 || !base) return 0;
            ext = 0;
        } else {
            int found = 0;
            for (int j = 0; allowed[j]; j++) if (name[i] == allowed[j]) found = 1;
            if (!found) return 0;
            if (ext < 0) {
                if (base == 8) return 0;
                stem[base++] = name[i];
            } else if (++ext > 3) return 0;
        }
    }
    if (!base || ext == 0) return 0;
    stem[base] = 0;
    if (!fd_cmp(stem, "CON") || !fd_cmp(stem, "PRN") ||
        !fd_cmp(stem, "AUX") || !fd_cmp(stem, "NUL") ||
        !fd_cmp(stem, "CLOCK$")) return 0;
    if (base == 4 && stem[3] >= '1' && stem[3] <= '9' &&
        ((stem[0] == 'C' && stem[1] == 'O' && stem[2] == 'M') ||
         (stem[0] == 'L' && stem[1] == 'P' && stem[2] == 'T'))) return 0;
    return 1;
}
void fd_parent(char *path) {
    int length = fd_len(path);
    if (length <= 3) return;
    while (length > 3 && path[length - 1] != '\\') length--;
    if (length > 3) length--;
    path[length] = 0;
}
int fd_path(char *out, const char *base, const char *input) {
    char path[FD_PATH], part[14];
    int i = 0, n;
    if (!input[0] || fd_len(input) >= FD_PATH) return FD_BADNAME;
    if (input[1] == ':') {
        int drive = upper(input[0]);
        if (!letter(drive) || (input[2] != '\\' && input[2] != '/')) return FD_BADNAME;
        path[0] = (char)drive; path[1] = ':'; path[2] = '\\'; path[3] = 0;
        i = 3;
    } else {
        fd_text(path, base, FD_PATH);
        if (input[0] == '\\' || input[0] == '/') path[3] = 0, i = 1;
    }
    while (input[i]) {
        while (input[i] == '\\' || input[i] == '/') i++;
        if (!input[i]) break;
        n = 0;
        while (input[i] && input[i] != '\\' && input[i] != '/') {
            if (n >= 13) return FD_BADNAME;
            part[n++] = (char)upper(input[i++]);
        }
        part[n] = 0;
        if (!fd_cmp(part, ".")) continue;
        if (!fd_cmp(part, "..")) { fd_parent(path); continue; }
        if (!valid_name(part)) return FD_BADNAME;
        int length = fd_len(path), separator = length > 3;
        if (length + separator + n >= FD_PATH) return FD_BADNAME;
        if (separator) path[length++] = '\\';
        for (int j = 0; j <= n; j++) path[length + j] = part[j];
    }
    fd_text(out, path, FD_PATH);
    return 0;
}
static int order(const Entry *a, const Entry *b) {
    if (!fd_cmp(a->name, "..")) return -1;
    if (!fd_cmp(b->name, "..")) return 1;
    if ((a->attr & FD_DIR) != (b->attr & FD_DIR)) return b->attr & FD_DIR ? 1 : -1;
    return fd_cmp(a->name, b->name);
}
int fd_refresh(Panel *panel) {
    char selected[13];
    selected[0] = 0;
    if (panel->selected >= 0 && panel->selected < panel->count)
        fd_text(selected, panel->entries[panel->selected].name, sizeof selected);
    int parent = fd_len(panel->path) > 3;
    int count = p_list(panel->path, panel->entries + parent,
                       FD_ENTRIES - parent, &panel->truncated);
    if (count < 0) return -count;
    if (parent) {
        Entry *entry = panel->entries;
        *entry = (Entry){0};
        fd_text(entry->name, "..", sizeof entry->name);
        entry->attr = FD_DIR;
    }
    panel->count = count + parent;
    for (int i = 1; i < panel->count; i++) {
        Entry entry = panel->entries[i];
        int j = i;
        while (j > 0 && order(&entry, &panel->entries[j - 1]) < 0) {
            panel->entries[j] = panel->entries[j - 1]; j--;
        }
        panel->entries[j] = entry;
    }
    if (panel->selected >= panel->count) panel->selected = panel->count - 1;
    if (panel->selected < 0) panel->selected = 0;
    for (int i = 0; i < panel->count; i++)
        if (!fd_cmp(selected, panel->entries[i].name)) panel->selected = i;
    return 0;
}
static int destination_ok(const char *source, const char *destination, Entry *entry) {
    Entry target;
    if (!fd_cmp(source, destination)) return FD_EXISTS;
    int result = p_stat(source, entry);
    if (result < 0) return -result;
    result = p_stat(destination, &target);
    if (!result) return FD_EXISTS;
    if (result != -2 && result != -3) return -result;
    return 0;
}
int fd_copy(const char *source, const char *destination) {
    static char buffer[2048];
    Entry entry;
    int error = destination_ok(source, destination, &entry);
    if (error) return error;
    if (entry.attr & FD_DIR) return 5;
    int input = p_open(source);
    if (input < 0) return -input;
    int output = p_create(destination);
    if (output < 0) { p_close(input); return -output; }
    for (;;) {
        if (p_cancel()) { error = FD_CANCELLED; break; }
        int count = p_read(input, buffer, sizeof buffer);
        if (count < 0) { error = -count; break; }
        if (!count) break;
        int written = p_write(output, buffer, count);
        if (written != count) { error = written < 0 ? -written : 112; break; }
    }
    if (!error) {
        int result = p_time(output, entry.time, entry.date);
        if (result < 0) error = -result;
    }
    int output_close = p_close(output), input_close = p_close(input);
    if (!error && output_close < 0) error = -output_close;
    if (!error && input_close < 0) error = -input_close;
    if (!error) {
        int result = p_attrs(destination, entry.attr & 0x27);
        if (result < 0) error = -result;
    }
    if (error) {
        p_attrs(destination, 0);
        if (p_unlink(destination) < 0) return FD_ROLLBACK;
    }
    return error;
}
int fd_rename(const char *source, const char *destination) {
    Entry entry;
    int error = destination_ok(source, destination, &entry);
    if (error) return error;
    int result = p_rename(source, destination);
    return result < 0 ? -result : 0;
}
int fd_move(const char *source, const char *destination) {
    int error = fd_rename(source, destination);
    if (error != 17) return error;
    error = fd_copy(source, destination);
    if (error) return error;
    int result = p_unlink(source);
    if (result < 0) {
        p_attrs(destination, 0);
        if (p_unlink(destination) < 0) return FD_ROLLBACK;
        return -result;
    }
    return 0;
}
int fd_remove(const char *path) {
    Entry entry;
    int result = p_stat(path, &entry);
    if (result < 0) return -result;
    result = entry.attr & FD_DIR ? p_rmdir(path) : p_unlink(path);
    return result < 0 ? -result : 0;
}
int fd_mkdir(const char *path) {
    Entry entry;
    if (!p_stat(path, &entry)) return FD_EXISTS;
    int result = p_mkdir(path);
    return result < 0 ? -result : 0;
}
