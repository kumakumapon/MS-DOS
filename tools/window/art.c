/* Original artwork encoded as rectangle commands; keep near code below 64 KiB. */
#include "utilities.h"
#include "art_data.h"
void utility_icon(int id,int x,int y){
    if(id<0||id>8)id=8;
    const unsigned char *p=icon_shapes[id];
    while(*p!=255){
        int color=p[4];
        if(gfx_colors()<256){if(color==114)color=HANDLE;else if(color==196||color==214)color=GOLD;}
        gfx_rect(x+p[0],y+p[1],p[2],p[3],color);p+=5;
    }
    input_poll();
}
