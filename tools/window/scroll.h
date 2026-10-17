#ifndef KDESK_SCROLL_H
#define KDESK_SCROLL_H
int scroll_clamp(int top,int total,int visible);
int scroll_thumb(int total,int visible,int track);
int scroll_pixel(int top,int total,int visible,int track);
int scroll_row(int pixel,int total,int visible,int track);
#endif
