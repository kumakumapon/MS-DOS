#include "editor.h"

/* One scratch buffer provides transactional loading and one-operation undo. */
static u8 scratch[ED_CAP];
static int undo_length, undo_cursor, undo_dirty;
void ed_init(Editor *e) {
    e->length = e->cursor = e->dirty = e->undo_valid = e->read_only = 0;
    e->desired = -1; e->path[0] = e->recovery[0] = 0;
}
int ed_next(const Editor *e, int at) {
    if (at >= e->length) return e->length;
    return at + (sjis_lead(e->data[at]) && at + 1 < e->length &&
                 sjis_trail(e->data[at + 1]) ? 2 : 1);
}
int ed_prev(const Editor *e, int at) {
    int previous = 0, pos = 0;
    while (pos < at) { previous = pos; pos = ed_next(e, pos); }
    return previous;
}
int ed_start(const Editor *e, int at) {
    while (at && e->data[at - 1] != '\n') at--;
    return at;
}
int ed_end(const Editor *e, int at) {
    while (at < e->length && e->data[at] != '\n') at = ed_next(e, at);
    return at;
}
int ed_width(const Editor *e, int at, int col) {
    if (at >= e->length) return 1;
    if (e->data[at] == '\t') return 8 - col % 8;
    return ed_next(e, at) - at;
}
int ed_column(const Editor *e) {
    int col = 0;
    for (int at = ed_start(e, e->cursor); at < e->cursor; at = ed_next(e, at))
        col += ed_width(e, at, col);
    return col;
}
void ed_vertical(Editor *e, int direction) {
    if (e->desired < 0) e->desired = ed_column(e);
    int start = ed_start(e, e->cursor);
    if (direction < 0) { if (!start) return; start = ed_start(e, start - 1); }
    else {
        start = ed_end(e, start);
        if (start == e->length) return;
        start++;
    }
    int end = ed_end(e, start), col = 0;
    while (start < end) {
        int width = ed_width(e, start, col);
        if (col + width > e->desired) break;
        col += width; start = ed_next(e, start);
    }
    e->cursor = start;
}
static int valid_text(const u8 *text, int count) {
    for (int i = 0; i < count; i++) {
        u8 c = text[i];
        if (sjis_lead(c)) {
            if (++i == count || !sjis_trail(text[i])) return 0;
        } else if (c != '\t' && c != '\n' &&
                   !(c >= 32 && c <= 126) && !(c >= 0xa1 && c <= 0xdf)) return 0;
    }
    return 1;
}
static void snapshot(Editor *e) {
    for (int i = 0; i < e->length; i++) scratch[i] = e->data[i];
    undo_length = e->length; undo_cursor = e->cursor; undo_dirty = e->dirty;
    e->undo_valid = 1;
}
int ed_insert(Editor *e, const u8 *text, int count) {
    if (count < 0 || count > ED_CAP - e->length) return 8;
    if (!valid_text(text, count)) return ED_BAD_TEXT;
    if (!count) return 0;
    snapshot(e);
    for (int i = e->length - 1; i >= e->cursor; i--) e->data[i + count] = e->data[i];
    for (int i = 0; i < count; i++) e->data[e->cursor + i] = text[i];
    e->length += count; e->cursor += count; e->dirty = 1; e->desired = -1;
    return 0;
}
static void erase(Editor *e, int start, int end) {
    if (end <= start) return;
    snapshot(e);
    for (int i = end; i < e->length; i++) e->data[start + i - end] = e->data[i];
    e->length -= end - start; e->cursor = start; e->dirty = 1; e->desired = -1;
}
void ed_delete(Editor *e, int backwards) {
    if (backwards) erase(e, ed_prev(e, e->cursor), e->cursor);
    else erase(e, e->cursor, ed_next(e, e->cursor));
}
void ed_delete_line(Editor *e) {
    int start = ed_start(e, e->cursor), end = ed_end(e, e->cursor);
    if (end < e->length) end++;
    erase(e, start, end);
}
void ed_undo(Editor *e) {
    if (!e->undo_valid) return;
    int length = e->length, cursor = e->cursor, dirty = e->dirty;
    int count = length > undo_length ? length : undo_length;
    for (int i = 0; i < count; i++) {
        u8 current = i < length ? e->data[i] : 0;
        e->data[i] = i < undo_length ? scratch[i] : 0;
        scratch[i] = current;
    }
    e->length = undo_length; e->cursor = undo_cursor; e->dirty = undo_dirty;
    undo_length = length; undo_cursor = cursor; undo_dirty = dirty;
    e->desired = -1;
}
int ed_load(Editor *e, const char *path) {
    Entry entry;
    int stat = p_stat(path, &entry);
    if (stat == -2) {
        ed_init(e); fd_text(e->path, path, FD_PATH);
        return 0;
    }
    if (stat < 0) return -stat;
    if (entry.attr & FD_DIR) return 5;
    int handle = p_open(path);
    if (handle < 0) return -handle;
    u8 chunk[512];
    int size = 0, error = 0, previous_cr = 0;
    e->undo_valid = 0;
    for (;;) {
        int count = p_read(handle, chunk, sizeof chunk);
        if (count < 0) { error = -count; break; }
        if (!count) break;
        for (int i = 0; i < count; i++) {
            u8 c = chunk[i];
            if (c == '\n' && previous_cr) { previous_cr = 0; continue; }
            previous_cr = c == '\r';
            if (size == ED_CAP) { error = 8; break; }
            scratch[size++] = c == '\r' ? '\n' : c;
        }
        if (error) break;
    }
    int closed = p_close(handle);
    if (!error && closed < 0) error = -closed;
    if (!error && !valid_text(scratch, size)) error = ED_BAD_TEXT;
    if (error) return error;
    ed_init(e);
    for (int i = 0; i < size; i++) e->data[i] = scratch[i];
    e->length = size; e->read_only = entry.attr & 1;
    fd_text(e->path, path, FD_PATH);
    return 0;
}
static int reserve(const char *path, const char *extension, char *out) {
    char directory[FD_PATH], name[13] = "ED000000.TMP";
    fd_text(directory, path, FD_PATH); fd_parent(directory);
    fd_text(name + 9, extension, 4);
    for (unsigned n = 0; n < 256; n++) {
        unsigned value = n;
        for (int i = 7; i >= 2; i--) { name[i] = (char)('0' + value % 10); value /= 10; }
        int error = fd_path(out, directory, name);
        if (error) return -error;
        int handle = p_create(out);
        if (handle != -FD_EXISTS) return handle;
    }
    return -FD_EXISTS;
}
static int cleanup(Editor *e, const char *temporary, int error) {
    if (p_unlink(temporary) < 0) {
        fd_text(e->recovery, temporary, FD_PATH);
        return ED_RECOVERY;
    }
    return error;
}
int ed_save(Editor *e) {
    char temporary[FD_PATH], backup[FD_PATH];
    Entry entry;
    e->recovery[0] = 0;
    if (!e->path[0]) return FD_BADNAME;
    int stat = p_stat(e->path, &entry);
    if (!stat && (entry.attr & (FD_DIR | 1))) return 5;
    if (stat && stat != -2) return -stat;
    int handle = reserve(e->path, "TMP", temporary);
    if (handle < 0) return -handle;
    u8 chunk[512];
    int error = 0, used = 0;
    for (int at = 0; at <= e->length; at++) {
        if (at == e->length || used >= 510) {
            int written = used ? p_write(handle, chunk, used) : 0;
            if (written != used) { error = written < 0 ? -written : 112; break; }
            used = 0;
        }
        if (at == e->length) break;
        if (e->data[at] == '\n') chunk[used++] = '\r';
        chunk[used++] = e->data[at];
    }
    int closed = p_close(handle);
    if (!error && closed < 0) error = -closed;
    if (error) return cleanup(e, temporary, error);
    if (!stat) {
        error = p_attrs(temporary, entry.attr & 0x26);
        if (error < 0) return cleanup(e, temporary, -error);
        handle = reserve(e->path, "BAK", backup);
        if (handle < 0) return cleanup(e, temporary, -handle);
        closed = p_close(handle);
        error = p_unlink(backup);
        if (closed < 0 || error < 0) return cleanup(e, temporary, 5);
        error = p_rename(e->path, backup);
        if (error < 0) return cleanup(e, temporary, -error);
    }
    error = p_rename(temporary, e->path);
    if (error < 0) {
        int restore = !stat ? p_rename(backup, e->path) : 0;
        int result = cleanup(e, temporary, -error);
        if (restore < 0) { fd_text(e->recovery, backup, FD_PATH); return ED_RECOVERY; }
        return result;
    }
    e->dirty = e->undo_valid = e->read_only = 0;
    if (!stat && p_unlink(backup) < 0) {
        fd_text(e->recovery, backup, FD_PATH);
        return ED_BACKUP_KEPT;
    }
    return 0;
}
