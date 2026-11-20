#ifndef DESKTOP_STYLE_H
#define DESKTOP_STYLE_H

#define DESKTOP_LANGUAGE_FILE "KUMAUI.CFG"

/* Geometry is in guest pixels. Compact existing controls keep their bounds. */
#define DESKTOP_SPACE_SMALL 4
#define DESKTOP_SPACE_MEDIUM 8
#define DESKTOP_SPACE_LARGE 16
#define DESKTOP_TITLE_HEIGHT 28
#define DESKTOP_CONTROL_HEIGHT 36
#define DESKTOP_FOCUS_WIDTH 2
#define DESKTOP_ACTIVE_MARKER_HEIGHT 2
#define DESKTOP_ICON_SMALL 16
#define DESKTOP_ICON_LARGE 32

/* The native 16-color palette stores RGB as three nibbles. */
#define DESKTOP_PALETTE_ACCENT 0x36a
#define DESKTOP_PALETTE_FOCUS 0xfb6

/* Client drawing uses 24-bit RGB in both PEGC and Cirrus modes. */
#define DESKTOP_COLOR_INK 0x20344au
#define DESKTOP_COLOR_PAPER 0xffffffu
#define DESKTOP_COLOR_SURFACE 0xeaf0f5u
#define DESKTOP_COLOR_BORDER 0xb4c2d0u
#define DESKTOP_COLOR_ACCENT 0x3366aau
#define DESKTOP_COLOR_FOCUS DESKTOP_COLOR_ACCENT
#define DESKTOP_COLOR_SELECTION 0xffbb66u
#define DESKTOP_COLOR_DISABLED 0xd7dfe7u
#define DESKTOP_COLOR_MUTED 0x526377u
#define DESKTOP_COLOR_SUCCESS 0x336644u
#define DESKTOP_COLOR_DANGER 0x882222u
#define DESKTOP_COLOR_PRIMARY_BUTTON 0xaaccffu
#define DESKTOP_COLOR_SECONDARY_BUTTON 0xdce7efu
#define DESKTOP_COLOR_DANGER_BUTTON 0xffbbbbu
#define DESKTOP_COLOR_PRESSED_SHADE 0x222222u

enum { DESKTOP_BUTTON_SECONDARY, DESKTOP_BUTTON_PRIMARY, DESKTOP_BUTTON_DANGER };
enum { DESKTOP_CONTROL_FOCUSED = 1, DESKTOP_CONTROL_DISABLED = 2, DESKTOP_CONTROL_PRESSED = 4 };

static inline unsigned desktop_button_fill(unsigned role, unsigned state) {
    if (state & DESKTOP_CONTROL_DISABLED) return DESKTOP_COLOR_DISABLED;
    unsigned fill = DESKTOP_COLOR_SECONDARY_BUTTON;
    if (role == DESKTOP_BUTTON_PRIMARY) fill = DESKTOP_COLOR_PRIMARY_BUTTON;
    if (role == DESKTOP_BUTTON_DANGER) fill = DESKTOP_COLOR_DANGER_BUTTON;
    return state & DESKTOP_CONTROL_PRESSED ? fill - DESKTOP_COLOR_PRESSED_SHADE : fill;
}

static inline int desktop_japanese(const char *value, int length) {
    return length >= 2 && (value[0] == 'J' || value[0] == 'j') &&
           (value[1] == 'A' || value[1] == 'a') &&
           (length == 2 || value[2] == '\r' || value[2] == '\n');
}
#endif
