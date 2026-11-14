#ifndef KGUI_TERMINAL_VIDEO_H
#define KGUI_TERMINAL_VIDEO_H
#include "../kvideo/video.h"
typedef struct {
 KvMeta meta;
 unsigned char font[KV_FONT_BYTES],planes[4][32768];
 unsigned short codes[4096],attributes[4096];
} KvCanvas;
_Static_assert(sizeof(KvCanvas)==153600,"private terminal cache");
int kt_video_extra(const KvMeta *);
void kt_video_compose(const KvCanvas *,unsigned char *,unsigned,
 const unsigned char *(*)(void *,unsigned),void *);
#endif
