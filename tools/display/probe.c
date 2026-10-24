#include "cirrus.h"
#include "../window/hardware.h"
static u16 line[800];
int main(void){
    p_output("KDISP: PC-9821 Cirrus RGB565 probe\r\n");
    if(!cirrus_open(800,600)){p_output("KDISP: Cirrus unavailable; display unchanged\r\n");return 1;}
    unsigned ds;__asm__ volatile("movw %%ds,%w0":"=r"(ds));
    for(unsigned y=0;y<600;y++){
        for(unsigned x=0;x<800;x++)line[x]=(x<256&&y<256)?(u16)(y*256+x):((x*31/799)<<11)|((y*63/599)<<5)|((x+y)*31/1398);
        cirrus_write(y*1600ul,ds,(unsigned)line,1600);
    }
    if(cirrus_pixel(32*2)!=32 || cirrus_pixel(8*1600ul)!=2048){
        cirrus_close();p_output("KDISP: VRAM readback FAIL\r\n");return 2;
    }
    p_output("KDISP: VRAM readback PASS\r\n");
    /* A real key returns to the original hardware mode and DOS. */
    unsigned a=0x0800;__asm__ volatile("int $0x21":"+a"(a)::"cc","memory");
    cirrus_close();p_output("KDISP: RGB565 probe returned to DOS\r\n");return 0;
}
