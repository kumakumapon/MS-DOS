#ifndef KDESK_BITMAP_H
#define KDESK_BITMAP_H
#include "hardware.h"
#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 200
typedef struct { unsigned segment; int width, height; } Bitmap;
int bitmap_new(Bitmap *image, int width, int height, int color);
void bitmap_free(Bitmap *image);
int bitmap_clone(Bitmap *out, const Bitmap *in);
int bitmap_load(Bitmap *image, const char *path);
int bitmap_save(const Bitmap *image, const char *path);
#endif
