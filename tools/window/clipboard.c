#include "clipboard.h"
volatile Clipboard clipboard = {{'K','D','E','S','K','C','L','P'}, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}, 0, 0};
static unsigned parent_segment, parent_offset;
void clip_state(int active) { clipboard.active = active; clipboard.command = 0; }
int clip_copy(const char *text, int length) {
    if (length < 0 || length >= CLIP_CAPACITY) return 0;
    clipboard.command = 0xfffe;
    for (int i = 0; i < length; i++) clipboard.text[i] = text[i];
    clipboard.text[length] = 0; clipboard.length = length;
    clipboard.result = 0; clipboard.sequence++; clipboard.command = 0; return 1;
}
void clip_image(unsigned segment, int width, int height) {
    clipboard.segment = segment; clipboard.width = width; clipboard.height = height;
    clipboard.sequence++;
}
static unsigned read_word(unsigned segment, unsigned offset) {
    return mem_get(segment, offset) | (unsigned)mem_get(segment, offset + 1) << 8;
}
void clip_mouse_save(void) {
    Mouse now = mouse_position();
    clipboard.cursor_x = now.x; clipboard.cursor_y = now.y; clipboard.reserved = 0x4352;
}
void clip_mouse_resume(void) {
    if (clipboard.reserved == 0x4352) mouse_position_set(clipboard.cursor_x, clipboard.cursor_y);
}
void clip_mouse_inherit(void) {
    u16 segment; __asm__ volatile("movw %%ds,%0" : "=r"(segment));
    unsigned parent = read_word(segment, 0x16);
    if (!parent || parent == segment || parent >= 0xa000) return;
    unsigned kind = mem_get(parent - 1, 0);
    if ((kind != 'M' && kind != 'Z') || read_word(parent - 1, 1) != parent) return;
    u32 limit = read_word(parent - 1, 3) * 16u;
    if (limit > 0x10000) limit = 0x10000;
    /* DOS identifies our parent PSP. Search only that program's 64 KiB block,
     * never reserve BIOS RAM or trust pointers from an unrelated old program. */
    for (unsigned at = 0x100; at + sizeof(Clipboard) <= limit; at++) {
        if (mem_get(parent, at) != 'K') continue;
        int match = 1;
        for (unsigned i = 1; i < 8; i++) if (mem_get(parent, at + i) != (u8)clipboard.signature[i]) match = 0;
        if (!match || read_word(parent, at + 8) != 1 || read_word(parent, at + 10) != 2 ||
            read_word(parent, at + 28) != 0x4352) continue;
        unsigned x = read_word(parent, at + 30 + CLIP_CAPACITY), y = read_word(parent, at + 32 + CLIP_CAPACITY);
        if (x >= 640 || y >= 400) continue;
        parent_segment = parent; parent_offset = at; mouse_position_set(x, y); return;
    }
}
void clip_mouse_return(void) {
    if (!parent_segment || read_word(parent_segment, parent_offset + 10) != 2) return;
    Mouse now = mouse_position(); unsigned at = parent_offset + 30 + CLIP_CAPACITY;
    mem_put(parent_segment, at, now.x); mem_put(parent_segment, at + 1, now.x >> 8);
    mem_put(parent_segment, at + 2, now.y); mem_put(parent_segment, at + 3, now.y >> 8);
}
