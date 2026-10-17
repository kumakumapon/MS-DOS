#include "scroll.h"
int scroll_clamp(int top,int total,int visible) {
    int end=total>visible?total-visible:0;
    return top<0?0:top>end?end:top;
}
int scroll_thumb(int total,int visible,int track) {
    if(track<=0)return 0;
    int size=total>visible?track*visible/total:track;
    if(size<8)size=8;
    return size>track?track:size;
}
int scroll_pixel(int top,int total,int visible,int track) {
    int end=scroll_clamp(total,total,visible),travel=track-scroll_thumb(total,visible,track);
    return end?scroll_clamp(top,total,visible)*travel/end:0;
}
int scroll_row(int pixel,int total,int visible,int track) {
    int travel=track-scroll_thumb(total,visible,track);
    if(travel<=0)return 0;
    if(pixel<0)pixel=0;
    if(pixel>travel)pixel=travel;
    return (pixel*scroll_clamp(total,total,visible)+travel/2)/travel;
}
