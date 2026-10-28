/* Resident trampoline: no return address in a bank is used until restored.
 * All banks share the DOS-owned data/stack segment. No CPU mode changes. */
#include "hardware.h"
#include "bank.h"
#include "windowtools.h"
extern char __bank_base[],__gui_load[],__gui_size[],__tools_load[],__tools_size[];
static int loaded;
static ToolRequest request;
static void load_bank(int bank){
    if(bank==loaded)return;
    unsigned cs;__asm__ volatile("movw %%cs,%w0":"=r"(cs));
    unsigned long from=(unsigned long)(bank==1?__gui_load:__tools_load);
    unsigned long size=(unsigned long)(bank==1?__gui_size:__tools_size);
    mem_copy(cs,(unsigned)__bank_base,cs+(from>>4),from&15,size);
    loaded=bank;
}
static int call(int op){int previous=loaded;load_bank(2);int result=tools_entry(op,&request);load_bank(previous);return result;}
int main(void);
int kdesk_boot(void){load_bank(1);return main();}
int wt_open(int tool,const char *path){request.tool=tool;request.path=path;return call(BANK_OPEN);}
void wt_draw(int tool,int x,int y,int w,int h,int top,int active){request.tool=tool;request.x=x;request.y=y;request.w=w;request.h=h;request.top=top;request.active=active;call(BANK_DRAW);}
void wt_key(int tool,int key){request.tool=tool;request.key=key;call(BANK_KEY);}
int wt_close(int tool){request.tool=tool;return call(BANK_CLOSE);}
int wt_clean(void){return call(BANK_CLEAN);}
int wt_click(int tool,int x,int y,int w,int h,int top,int px,int py){request.tool=tool;request.x=x;request.y=y;request.w=w;request.h=h;request.top=top;request.px=px;request.py=py;return call(BANK_CLICK);}
void wt_finish(void){call(BANK_FINISH);}
