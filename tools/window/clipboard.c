#include "clipboard.h"
volatile Clipboard clipboard = {{'K','D','E','S','K','C','L','P'}, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}};
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
