#ifndef DESKTOP_STYLE_H
#define DESKTOP_STYLE_H

#define DESKTOP_LANGUAGE_FILE "KUMAUI.CFG"
#define DESKTOP_ACTIVE_MARKER_HEIGHT 2
#define DESKTOP_PALETTE_ACCENT 0x36a
#define DESKTOP_PALETTE_FOCUS 0xfb6

static inline int desktop_japanese(const char *value, int length) {
    return length >= 2 && (value[0] == 'J' || value[0] == 'j') &&
           (value[1] == 'A' || value[1] == 'a') &&
           (length == 2 || value[2] == '\r' || value[2] == '\n');
}
#endif
