/* POSIX adapter for exercising the same file engine under ASan/UBSan.
   A: and B: are temporary A/ and B/ directories, not host disks. */
#define _POSIX_C_SOURCE 200809L
#include "filer.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int output_handle = -1, written;
static int error_code(void) {
    switch (errno) {
        case ENOENT: return -2;
        case ENOTDIR: return -3;
        case EEXIST: return -FD_EXISTS;
        case ENOSPC: return -112;
        case EXDEV: return -17;
        case EINVAL: return -FD_BADNAME;
        default: return -5;
    }
}
static void native(char *out, const char *path) {
    out[0] = path[0]; out[1] = '/';
    int i = 2;
    for (int j = 3; path[j]; j++) out[i++] = path[j] == '\\' ? '/' : path[j];
    out[i] = 0;
}
static void fill_entry(Entry *entry, const char *name, const struct stat *st) {
    *entry = (Entry){0};
    fd_text(entry->name, name, sizeof entry->name);
    entry->size = (u32)st->st_size;
    entry->attr = S_ISDIR(st->st_mode) ? FD_DIR : 0x20;
    if (!(st->st_mode & S_IWUSR)) entry->attr |= 1;
    struct tm *t = localtime(&st->st_mtime);
    if (t && t->tm_year >= 80 && t->tm_year <= 207) {
        entry->date = (u16)((t->tm_year - 80) << 9 | (t->tm_mon + 1) << 5 | t->tm_mday);
        entry->time = (u16)(t->tm_hour << 11 | t->tm_min << 5 | t->tm_sec / 2);
    }
}
int p_stat(const char *path, Entry *entry) {
    char name[FD_PATH + 2]; struct stat st;
    native(name, path);
    if (lstat(name, &st) < 0) return error_code();
    if (S_ISLNK(st.st_mode)) return -5;
    const char *base = strrchr(path, '\\');
    fill_entry(entry, base ? base + 1 : path, &st);
    return 0;
}
int p_list(const char *path, Entry *entries, int capacity, int *truncated) {
    char name[FD_PATH + 2];
    native(name, path);
    DIR *dir = opendir(name);
    if (!dir) return error_code();
    int count = 0; *truncated = 0;
    struct dirent *item;
    while ((item = readdir(dir))) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (strlen(item->d_name) > 12) continue;
        char full[FD_PATH + 2];
        if (snprintf(full, sizeof full, "%s%s%s", name,
                     name[strlen(name) - 1] == '/' ? "" : "/", item->d_name) >= (int)sizeof full) continue;
        struct stat st;
        if (lstat(full, &st) < 0 || S_ISLNK(st.st_mode)) continue;
        if (count == capacity) { *truncated = 1; break; }
        fill_entry(&entries[count++], item->d_name, &st);
    }
    closedir(dir);
    return count;
}
int p_open(const char *path) {
    char name[FD_PATH + 2]; native(name, path);
    int handle = open(name, O_RDONLY);
    return handle < 0 ? error_code() : handle;
}
int p_create(const char *path) {
    char name[FD_PATH + 2]; native(name, path);
    int handle = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (handle >= 0) output_handle = handle, written = 0;
    return handle < 0 ? error_code() : handle;
}
int p_read(int handle, void *buffer, int length) {
    if (getenv("FD_FAIL_READ")) return -5;
    int count = (int)read(handle, buffer, (size_t)length);
    return count < 0 ? error_code() : count;
}
int p_write(int handle, const void *buffer, int length) {
    const char *limit = getenv("FD_WRITE_LIMIT");
    if (limit && handle == output_handle) {
        int available = atoi(limit) - written;
        if (available <= 0) return -112;
        if (length > available) length = available;
    }
    int count = (int)write(handle, buffer, (size_t)length);
    if (count > 0) written += count;
    return count < 0 ? error_code() : count;
}
int p_close(int handle) {
    int result = close(handle);
    if (handle == output_handle && getenv("FD_FAIL_CLOSE")) return -5;
    return result < 0 ? error_code() : 0;
}
int p_time(int handle, u16 time, u16 date) {
    struct tm value = {0};
    value.tm_year = 80 + (date >> 9); value.tm_mon = ((date >> 5) & 15) - 1;
    value.tm_mday = date & 31; value.tm_hour = time >> 11;
    value.tm_min = (time >> 5) & 63; value.tm_sec = (time & 31) * 2; value.tm_isdst = -1;
    struct timespec times[2] = {{0, UTIME_OMIT}, {mktime(&value), 0}};
    return futimens(handle, times) < 0 ? error_code() : 0;
}
int p_attrs(const char *path, u8 attrs) {
    char name[FD_PATH + 2]; native(name, path);
    return chmod(name, attrs & 1 ? 0400 : 0600) < 0 ? error_code() : 0;
}
int p_rename(const char *source, const char *destination) {
    Entry entry;
    if (source[0] != destination[0] || getenv("FD_CROSS_DEVICE")) return -17;
    if (!p_stat(destination, &entry)) return -FD_EXISTS;
    char from[FD_PATH + 2], to[FD_PATH + 2];
    native(from, source); native(to, destination);
    return rename(from, to) < 0 ? error_code() : 0;
}
int p_unlink(const char *path) {
    const char *fail = getenv("FD_FAIL_UNLINK");
    if (fail && (!strcmp(fail, "ALL") || strstr(path, fail))) return -5;
    Entry entry;
    int error = p_stat(path, &entry);
    if (error) return error;
    if (entry.attr & 1) return -5;
    char name[FD_PATH + 2]; native(name, path);
    return unlink(name) < 0 ? error_code() : 0;
}
int p_mkdir(const char *path) {
    char name[FD_PATH + 2]; native(name, path);
    return mkdir(name, 0700) < 0 ? error_code() : 0;
}
int p_rmdir(const char *path) {
    char name[FD_PATH + 2]; native(name, path);
    return rmdir(name) < 0 ? error_code() : 0;
}
int p_cancel(void) { return getenv("FD_ABORT_COPY") != 0; }

int main(int argc, char **argv) {
    static Panel panel;
    char source[FD_PATH], destination[FD_PATH];
    if (argc < 3) return 2;
    int error = fd_path(source, "A:\\", argv[2]);
    if (!error && argc > 3) error = fd_path(destination, "A:\\", argv[3]);
    if (error) { printf("ERROR %d\n", error); return 1; }
    if (!strcmp(argv[1], "PATH")) puts(source);
    else if (!strcmp(argv[1], "LIST")) {
        fd_text(panel.path, source, FD_PATH);
        error = fd_refresh(&panel);
        if (!error) {
            for (int i = 0; i < panel.count; i++)
                printf("%s %c %u\n", panel.entries[i].name,
                       panel.entries[i].attr & FD_DIR ? 'D' : 'F', panel.entries[i].size);
            printf("TRUNCATED %d\n", panel.truncated);
        }
    } else if (!strcmp(argv[1], "COPY") && argc == 4) error = fd_copy(source, destination);
    else if (!strcmp(argv[1], "MOVE") && argc == 4) error = fd_move(source, destination);
    else if (!strcmp(argv[1], "RENAME") && argc == 4) error = fd_rename(source, destination);
    else if (!strcmp(argv[1], "DELETE")) error = fd_remove(source);
    else if (!strcmp(argv[1], "MKDIR")) error = fd_mkdir(source);
    else return 2;
    if (error) printf("ERROR %d\n", error);
    return error ? 1 : 0;
}
