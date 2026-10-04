#include "filer.h"
#include "sjis.h"

static u8 dta[43];
static int result(unsigned value) { return (int)value; }
static int done(unsigned a) { return result(a); }
static int call_path(unsigned service, const char *path) {
    unsigned a = service, d = (unsigned)path;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "d"(d) : "cc", "memory");
    return done(a);
}
void p_init(void) {
    unsigned a = 0x1a00, d = (unsigned)dta;
    __asm__ volatile("int $0x21" : "+a"(a) : "d"(d) : "cc", "memory");
}
static int find(const char *path, int next) {
    unsigned a = next ? 0x4f00 : 0x4e00, c = 0x37, d = (unsigned)path;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "c"(c), "d"(d) : "cc", "memory");
    return done(a);
}
static u16 word(int offset) { return dta[offset] | (u16)dta[offset + 1] << 8; }
static void entry_from_dta(Entry *entry) {
    fd_text(entry->name, (const char *)dta + 30, sizeof entry->name);
    entry->attr = dta[21];
    entry->time = word(22); entry->date = word(24);
    entry->size = word(26) | (u32)word(28) << 16;
}
int p_stat(const char *path, Entry *entry) {
    if (fd_len(path) == 3 && path[1] == ':' && path[2] == '\\') {
        unsigned a = 0x3600, d = (unsigned)(path[0] - 'A' + 1);
        __asm__ volatile("int $0x21" : "+a"(a) : "d"(d) : "ebx", "ecx", "cc", "memory");
        if ((a & 65535) == 65535) return -15;
        *entry = (Entry){0}; entry->attr = FD_DIR;
        return 0;
    }
    int error = find(path, 0);
    if (error == -18) return -2; /* FindFirst reports no matches as "no more files". */
    if (error) return error;
    entry_from_dta(entry);
    return 0;
}
int p_list(const char *path, Entry *entries, int capacity, int *truncated) {
    char mask[FD_PATH];
    int error = fd_path(mask, path, ".");
    if (error) return -error;
    int length = fd_len(mask);
    if (length + 5 >= FD_PATH) return -FD_BADNAME;
    if (mask[length - 1] != '\\') mask[length++] = '\\';
    fd_text(mask + length, "*.*", FD_PATH - length);
    int count = 0;
    *truncated = 0;
    error = find(mask, 0);
    while (!error) {
        Entry entry;
        entry_from_dta(&entry);
        if (!(entry.attr & 8) && fd_cmp(entry.name, ".") && fd_cmp(entry.name, "..")) {
            if (count == capacity) { *truncated = 1; break; }
            entries[count++] = entry;
        }
        error = find(mask, 1);
    }
    return error && error != -2 && error != -18 ? error : count;
}
int p_open(const char *path) {
    unsigned a = 0x3d00, d = (unsigned)path;
    __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:"
                     : "+a"(a) : "d"(d) : "cc", "memory");
    return done(a);
}
int p_create(const char *path) {
    unsigned a = 0x5b00, c = 0, d = (unsigned)path;
    __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:"
                     : "+a"(a) : "c"(c), "d"(d) : "cc", "memory");
    return done(a);
}
int p_read(int handle, void *buffer, int length) {
    unsigned a = 0x3f00, b = (unsigned)handle, c = (unsigned)length, d = (unsigned)buffer;
    __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:"
                     : "+a"(a) : "b"(b), "c"(c), "d"(d) : "cc", "memory");
    return done(a);
}
int p_write(int handle, const void *buffer, int length) {
    unsigned a = 0x4000, b = (unsigned)handle, c = (unsigned)length, d = (unsigned)buffer;
    __asm__ volatile("int $0x21; jnc 1f; neg %%eax; 1:"
                     : "+a"(a) : "b"(b), "c"(c), "d"(d) : "cc", "memory");
    return done(a);
}
int p_close(int handle) {
    unsigned a = 0x3e00, b = (unsigned)handle;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "b"(b) : "cc", "memory");
    return done(a);
}
int p_seek(int handle, u32 position) {
    unsigned a = 0x4200, b = (unsigned)handle, c = position >> 16, d = position & 65535;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a), "+d"(d) : "b"(b), "c"(c) : "cc", "memory");
    return done(a);
}
int p_time(int handle, u16 time, u16 date) {
    unsigned a = 0x5701, b = (unsigned)handle, c = time, d = date;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "b"(b), "c"(c), "d"(d) : "cc", "memory");
    return done(a);
}
int p_attrs(const char *path, u8 attrs) {
    unsigned a = 0x4301, c = attrs, d = (unsigned)path;
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "c"(c), "d"(d) : "cc", "memory");
    return done(a);
}
int p_rename(const char *source, const char *destination) {
    unsigned a = 0x5600, d = (unsigned)source, i = (unsigned)destination;
    __asm__ volatile("pushw %%es; pushw %%ds; popw %%es; int $0x21; popw %%es;"
                     "jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "d"(d), "D"(i) : "cc", "memory");
    return done(a);
}
int p_unlink(const char *path) { return call_path(0x4100, path); }
int p_mkdir(const char *path) { return call_path(0x3900, path); }
int p_rmdir(const char *path) { return call_path(0x3a00, path); }
int p_cwd(char *path) {
    unsigned a = 0x1900;
    __asm__ volatile("int $0x21" : "+a"(a) :: "cc", "memory");
    path[0] = (char)('A' + (a & 255)); path[1] = ':'; path[2] = '\\'; path[3] = 0;
    a = 0x4700;
    unsigned d = 0, i = (unsigned)(path + 3);
    __asm__ volatile("int $0x21; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a) : "d"(d), "S"(i) : "cc", "memory");
    return done(a);
}
int p_chdir(const char *path) {
    unsigned a = 0x0e00, d = (unsigned)(path[0] - 'A');
    __asm__ volatile("int $0x21" : "+a"(a) : "d"(d) : "cc", "memory");
    return call_path(0x3b00, path);
}
int p_exec(const char *program, const char *arguments) {
    u16 segment;
    char tail[128];
    struct __attribute__((packed)) { u16 env, tail, tail_segment, fcb1, seg1, fcb2, seg2; } parameters;
    int length = fd_len(arguments);
    if (length > 125) return -FD_BADNAME;
    __asm__ volatile("movw %%ds,%0" : "=r"(segment));
    tail[0] = (char)(length ? length + 1 : 0);
    if (length) { tail[1] = ' '; for (int i = 0; i < length; i++) tail[i + 2] = arguments[i]; }
    tail[(u8)tail[0] + 1] = '\r';
    parameters.env = 0; parameters.tail = (u16)(unsigned)tail; parameters.tail_segment = segment;
    parameters.fcb1 = 0x5c; parameters.seg1 = segment; parameters.fcb2 = 0x6c; parameters.seg2 = segment;
    unsigned a = 0x4b00, b = (unsigned)&parameters, d = (unsigned)program;
    __asm__ volatile("pushw %%ds; pushw %%es; pushw %%ds; popw %%es; int $0x21;"
                     "popw %%es; popw %%ds; jc 1f; xor %%eax,%%eax; jmp 2f; 1: neg %%eax; 2:"
                     : "+a"(a), "+b"(b), "+d"(d) :: "ecx", "esi", "edi", "cc", "memory");
    if ((int)a < 0) return (int)a;
    a = 0x4d00;
    __asm__ volatile("int $0x21" : "+a"(a) :: "cc", "memory");
    int code = a & 255;
    p_init();
    return code;
}
static void far_word(unsigned segment, unsigned offset, u16 value) {
    __asm__ volatile("pushw %%es; movw %w0,%%es; movw %w2,%%es:(%1); popw %%es"
                     :: "r"(segment), "r"(offset), "r"(value) : "memory");
}
#ifdef PC98
static void bios18(unsigned a, unsigned d) {
    __asm__ volatile("int $0x18" : "+a"(a), "+d"(d)
                     :: "ebx", "ecx", "esi", "edi", "cc", "memory");
}
#endif
void p_cell(int x, int y, char character, int highlighted) {
    unsigned offset = (unsigned)(y * 80 + x) * 2;
#ifdef PC98
    far_word(0xa000, offset, (u8)character);
    far_word(0xa000, 0x2000 + offset, highlighted ? 0xe5 : 0xe1);
#else
    far_word(0xb800, offset, (u8)character | (highlighted ? 0x7000 : 0x1700));
#endif
}
void p_pair(int x, int y, u8 lead, u8 trail, int highlighted) {
    if (x < 0 || x >= 79 || y < 0 || y >= 25) return;
#ifdef PC98
    unsigned offset = (unsigned)(y * 80 + x) * 2;
    far_word(0xa000, offset, (u16)sjis_vram(lead, trail));
    far_word(0xa000, offset + 2, 0);
    far_word(0xa000, 0x2000 + offset, highlighted ? 0xe5 : 0xe1);
    far_word(0xa000, 0x2002 + offset, highlighted ? 0xe5 : 0xe1);
#else
    (void)lead; (void)trail;
    p_cell(x, y, '?', highlighted); p_cell(x + 1, y, '?', highlighted);
#endif
}
void p_screen(void) {
#ifdef PC98
    bios18(0x4100, 0); /* Disable graphics so a previously run BASIC sample cannot obscure text. */
    bios18(0x0a00, 0);
    bios18(0x0c00, 0);
    bios18(0x1200, 0);
#else
    unsigned a = 3;
    __asm__ volatile("int $0x10" : "+a"(a) :: "cc", "memory");
    a = 0x0100;
    unsigned c = 0x2000;
    __asm__ volatile("int $0x10" : "+a"(a) : "c"(c) : "cc", "memory");
#endif
    for (int y = 0; y < 25; y++) for (int x = 0; x < 80; x++) p_cell(x, y, ' ', 0);
}
void p_finish(void) {
#ifdef PC98
    /* Let the DOS CON driver reset its own cursor as well as the visible text. */
    p_output("\033[2J\033[H");
    bios18(0x1100, 0);
#else
    unsigned a = 3;
    __asm__ volatile("int $0x10" : "+a"(a) :: "cc", "memory");
#endif
}
int p_key(void) {
    unsigned a = 0;
#ifdef PC98
    __asm__ volatile("int $0x18" : "+a"(a) :: "ebx", "cc", "memory");
    unsigned scan = (a >> 8) & 255;
    if (scan >= 0x62 && scan <= 0x6b) return KEY_F1 + (int)scan - 0x62;
    if (scan == 0x3a) return KEY_UP;
    if (scan == 0x3d) return KEY_DOWN;
    if (scan == 0x3b) return KEY_LEFT;
    if (scan == 0x3c) return KEY_RIGHT;
    if (scan == 0x3e) return KEY_HOME;
    if (scan == 0x3f) return KEY_END;
    if (scan == 0x36) return KEY_PAGEUP;
    if (scan == 0x37) return KEY_PAGEDOWN;
    if (scan == 0x38) return KEY_INSERT;
    if (scan == 0x39) return KEY_DELETE;
#else
    __asm__ volatile("int $0x16" : "+a"(a) :: "cc", "memory");
    unsigned scan = (a >> 8) & 255;
    if (scan >= 0x3b && scan <= 0x44) return KEY_F1 + (int)scan - 0x3b;
    if (scan == 0x48) return KEY_UP;
    if (scan == 0x50) return KEY_DOWN;
    if (scan == 0x4b) return KEY_LEFT;
    if (scan == 0x4d) return KEY_RIGHT;
    if (scan == 0x47) return KEY_HOME;
    if (scan == 0x4f) return KEY_END;
    if (scan == 0x49) return KEY_PAGEUP;
    if (scan == 0x51) return KEY_PAGEDOWN;
    if (scan == 0x52) return KEY_INSERT;
    if (scan == 0x53) return KEY_DELETE;
#endif
    return a & 255;
}
int p_cancel(void) {
    unsigned a = 0x0100;
#ifdef PC98
    unsigned b = 0;
    __asm__ volatile("int $0x18" : "+a"(a), "+b"(b) :: "cc", "memory");
    if (!(b & 0xff00)) return 0;
#else
    __asm__ volatile("int $0x16; jnz 1f; xor %%eax,%%eax; 1:"
                     : "+a"(a) :: "cc", "memory");
#endif
    if ((a & 255) != 27) return 0;
    p_key();
    return 1;
}
void p_output(const char *text) { p_write(1, text, fd_len(text)); }
