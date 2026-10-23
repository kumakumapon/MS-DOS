/* Original 32px PC-98 application artwork, composed from filled pixel shapes.
 * No external fonts, images or palette changes; all shapes also work in 16 colors. */
#include "utilities.h"

static void box(int x,int y,int w,int h,int color) { gfx_rect(x,y,w,h,color); }
static void paper(int x,int y) {
    box(x+6,y+2,21,28,SHADOW);box(x+4,y,21,28,INACTIVE);
    box(x+5,y+1,19,26,WHITE);box(x+19,y,6,6,SURFACE);
    box(x+19,y+5,6,1,MUTED);
}
void utility_icon(int id,int x,int y) {
    int green=gfx_colors()==256?114:HANDLE;
    int red=gfx_colors()==256?196:GOLD;
    int orange=gfx_colors()==256?214:GOLD;
    if(id==0) { /* Folder with documents and a highlighted lip. */
        box(x+2,y+6,28,24,SHADOW);box(x,y+4,13,8,orange);
        box(x+1,y+7,28,20,orange);box(x+5,y+3,20,20,WHITE);
        for(int i=0;i<3;i++)box(x+8,y+7+i*4,13,1,INACTIVE);
        box(x,y+12,29,16,GOLD);box(x+1,y+13,27,2,WHITE);
        box(x+2,y+27,27,2,orange);
    } else if(id==1) { /* Document and blue rule. */
        paper(x,y);
        box(x+8,y+6,7,4,ACCENT);
        for(int i=0;i<4;i++)box(x+8,y+13+i*3,i==3?9:13,1,MUTED);
    } else if(id==2) { /* Settings sliders. */
        box(x+2,y+2,29,29,SHADOW);box(x,y,29,29,SURFACE);
        box(x+1,y+1,27,27,PAPER);box(x+1,y+1,27,3,HANDLE);
        for(int i=0;i<3;i++) {
            int a=x+6+i*8,b=y+9+(i==1?8:0);
            box(a,y+7,2,17,MUTED);box(a-3,b,8,5,ACCENT);
            box(a-2,b,6,1,WHITE);
        }
    } else if(id==3||id==4) { /* Program listing / terminal. */
        box(x+2,y+2,29,25,SHADOW);box(x,y,29,24,INACTIVE);
        box(x+2,y+2,25,19,INK);box(x+7,y+25,15,3,MUTED);
        box(x+4,y+28,21,2,SURFACE);
        int c=id==3?HANDLE:green;
        for(int i=0;i<4;i++)box(x+5,y+5+i*4,id==3?18-i*3:2+i,1,c);
        box(x+13,y+15,7,2,c);
    } else if(id==5) { /* Photograph: sky, sun and two mountain slopes. */
        paper(x,y);box(x+6,y+6,17,17,ACCENT);
        box(x+17,y+8,4,4,GOLD);
        for(int i=0;i<10;i++)box(x+6,y+22-i,17-i,1,green);
        for(int i=0;i<8;i++)box(x+15,y+22-i,8-i,1,HANDLE);
    } else if(id==6) { /* Palette and diagonally lit paint brush. */
        box(x+4,y+5,23,25,SHADOW);box(x+1,y+7,26,18,SURFACE);
        box(x+4,y+3,20,25,GOLD);box(x+2,y+8,26,14,GOLD);
        box(x+7,y+7,4,4,red);box(x+16,y+6,4,4,ACCENT);
        box(x+4,y+15,4,4,green);box(x+15,y+19,4,4,DESKTOP);
        for(int i=0;i<14;i++)box(x+28-i,y+i,3,3,orange);
        box(x+11,y+15,6,6,MUTED);box(x+8,y+20,6,6,INK);
        box(x+8,y+20,2,4,WHITE);
    } else if(id==7) { /* Calculator with LCD and color-coded equals key. */
        box(x+5,y+2,23,29,SHADOW);box(x+3,y,23,29,ACCENT);
        box(x+5,y+2,19,7,SURFACE);box(x+7,y+4,15,3,green);
        for(int r=0;r<3;r++)for(int c=0;c<3;c++) {
            box(x+6+c*6,y+12+r*5,4,3,c==2&&r==2?GOLD:WHITE);
        }
    } else { /* Clock face and hands. */
        box(x+5,y+1,22,29,SHADOW);box(x+1,y+5,30,20,ACCENT);
        box(x+5,y+1,22,28,ACCENT);box(x+6,y+3,20,24,WHITE);
        box(x+3,y+7,26,16,WHITE);
        box(x+15,y+4,2,3,MUTED);box(x+15,y+23,2,3,MUTED);
        box(x+4,y+14,3,2,MUTED);box(x+25,y+14,3,2,MUTED);
        box(x+15,y+8,2,8,ACCENT);box(x+15,y+14,8,2,ACCENT);
        box(x+14,y+13,4,4,GOLD);
    }
    input_poll();
}
