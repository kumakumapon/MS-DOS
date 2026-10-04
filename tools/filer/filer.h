#ifndef FILER_H
#define FILER_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define FD_PATH 128
#define FD_ENTRIES 256
#define FD_DIR 0x10
#define FD_EXISTS 80
#define FD_BADNAME 123
#define FD_CANCELLED 995
#define FD_ROLLBACK 996

typedef struct {
    char name[13];
    u8 attr;
    u16 time, date;
    u32 size;
} Entry;

typedef struct {
    char path[FD_PATH];
    Entry entries[FD_ENTRIES];
    int count, selected, top, truncated;
} Panel;

enum {
    KEY_UP = 256, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END,
    KEY_PAGEUP, KEY_PAGEDOWN, KEY_F1 = 280
};

int fd_len(const char *s);
int fd_cmp(const char *a, const char *b);
void fd_text(char *out, const char *in, int capacity);
void fd_number(char *out, u32 value);
int fd_path(char *out, const char *base, const char *input);
void fd_parent(char *path);
int fd_refresh(Panel *panel);
int fd_copy(const char *source, const char *destination);
int fd_move(const char *source, const char *destination);
int fd_rename(const char *source, const char *destination);
int fd_remove(const char *path);
int fd_mkdir(const char *path);

/* Errors are negative DOS error codes; handles and byte counts are nonnegative. */
int p_stat(const char *path, Entry *entry);
int p_list(const char *path, Entry *entries, int capacity, int *truncated);
int p_open(const char *path);
int p_create(const char *path);
int p_read(int handle, void *buffer, int length);
int p_write(int handle, const void *buffer, int length);
int p_close(int handle);
int p_seek(int handle, u32 position);
int p_time(int handle, u16 time, u16 date);
int p_attrs(const char *path, u8 attrs);
int p_rename(const char *source, const char *destination);
int p_unlink(const char *path);
int p_mkdir(const char *path);
int p_rmdir(const char *path);
int p_cwd(char *path);
int p_chdir(const char *path);
int p_exec(const char *program, const char *arguments);
int p_cancel(void);
void p_init(void);
void p_screen(void);
void p_finish(void);
void p_cell(int x, int y, char character, int highlighted);
int p_key(void);
void p_output(const char *text);

#endif
