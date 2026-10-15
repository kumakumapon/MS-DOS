#ifndef KDESK_CLIPBOARD_H
#define KDESK_CLIPBOARD_H
#include "hardware.h"
#define CLIP_CAPACITY 1024
/* Shared-memory ABI: only explicit host actions read/write these fields.
 * active: 0 exited, 1 desktop, 2 suspended parent, 3 viewer, 4 paint. */
typedef struct __attribute__((packed)) {
    char signature[8];
    u16 version, active, length, command;
    u32 sequence;
    u16 result, width, height, segment, reserved;
    char text[CLIP_CAPACITY];
} Clipboard;
extern volatile Clipboard clipboard;
void clip_state(int active);
int clip_copy(const char *text, int length);
void clip_image(unsigned segment, int width, int height);
#endif
