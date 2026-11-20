#ifndef KGUI32_GALLERY_H
#define KGUI32_GALLERY_H
#include "sdk/protocol.h"
#include "../desktop-style.h"

enum {
    KGALLERY_BACK_X = 18,
    KGALLERY_BACK_Y = 224,
    KGALLERY_BACK_W = 140,
    KGALLERY_BACK_H = DESKTOP_CONTROL_HEIGHT
};

typedef struct {
    void (*label)(int x, int y, const char *text, u32 rgb);
    void (*button)(int x, int y, int width, int height, const char *text, unsigned role, unsigned state);
    void (*rectangle)(int x, int y, int width, int height, u32 rgb);
} KGalleryPainter;

void kgallery_draw(int x, int y, const KGalleryPainter *painter);
#endif
