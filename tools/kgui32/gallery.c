#include "gallery.h"
#include "ui-gallery.h"

typedef struct {
    const char *label;
    unsigned role;
    unsigned state;
} KGallerySample;

static const KGallerySample samples[] = {
    {"Primary", DESKTOP_BUTTON_PRIMARY, 0},
    {"Secondary", DESKTOP_BUTTON_SECONDARY, 0},
    {"Danger", DESKTOP_BUTTON_DANGER, 0},
    {"Focused", DESKTOP_BUTTON_SECONDARY, DESKTOP_CONTROL_FOCUSED},
    {"Disabled", DESKTOP_BUTTON_SECONDARY, DESKTOP_CONTROL_DISABLED},
    {"Pressed", DESKTOP_BUTTON_PRIMARY, DESKTOP_CONTROL_PRESSED},
};

/* One monochrome folder outline, at the two sizes of the desktop grid. */
static void folder_sample(int x, int y, int size, const KGalleryPainter *p) {
    int unit = size / DESKTOP_ICON_SMALL;
    p->rectangle(x, y + 3 * unit, 7 * unit, unit, DESKTOP_COLOR_ACCENT);
    p->rectangle(x, y + 4 * unit, unit, 9 * unit, DESKTOP_COLOR_ACCENT);
    p->rectangle(x + 7 * unit, y + 4 * unit, 9 * unit, unit, DESKTOP_COLOR_ACCENT);
    p->rectangle(x + 15 * unit, y + 5 * unit, unit, 8 * unit, DESKTOP_COLOR_ACCENT);
    p->rectangle(x, y + 13 * unit, size, unit, DESKTOP_COLOR_ACCENT);
}

void kgallery_draw(int x, int y, const KGalleryPainter *p) {
    const int button_width = 96;
    p->label(x + 18, y + 46, KU("Component samples"), DESKTOP_COLOR_INK);
    for (unsigned i = 0; i < sizeof samples / sizeof *samples; i++) {
        const KGallerySample *s = &samples[i];
        int left = x + 18 + (i % 3) * (button_width + DESKTOP_SPACE_SMALL);
        int top = y + 80 + (i / 3) * (DESKTOP_CONTROL_HEIGHT + DESKTOP_SPACE_LARGE);
        p->button(left, top, button_width, DESKTOP_CONTROL_HEIGHT, KU(s->label), s->role, s->state);
    }
    p->label(x + 18, y + 190, KU("Icon sizes"), DESKTOP_COLOR_INK);
    folder_sample(x + 118, y + 190, DESKTOP_ICON_SMALL, p);
    p->label(x + 142, y + 190, "16", DESKTOP_COLOR_INK);
    folder_sample(x + 182, y + 182, DESKTOP_ICON_LARGE, p);
    p->label(x + 222, y + 190, "32", DESKTOP_COLOR_INK);
    p->button(x + KGALLERY_BACK_X, y + KGALLERY_BACK_Y, KGALLERY_BACK_W, KGALLERY_BACK_H,
              KU("Back"), DESKTOP_BUTTON_SECONDARY, 0);
}
