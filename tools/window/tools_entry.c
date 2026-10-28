#include "bank.h"
#include "../filer/filer.h"
#include "windowtools.h"
int tools_entry(int op,ToolRequest *r){
    switch(op){
    case BANK_OPEN:return wt_open(r->tool,r->path);
    case BANK_DRAW:wt_draw(r->tool,r->x,r->y,r->w,r->h,r->top,r->active);break;
    case BANK_KEY:wt_key(r->tool,r->key);break;
    case BANK_CLOSE:return wt_close(r->tool);
    case BANK_CLEAN:return wt_clean();
    case BANK_CLICK:return wt_click(r->tool,r->x,r->y,r->w,r->h,r->top,r->px,r->py);
    case BANK_FILE:switch(r->key){
        case 2:return fd_rename(r->path,r->destination);
        case 5:return fd_copy(r->path,r->destination);
        case 6:return fd_move(r->path,r->destination);
        case 7:return fd_mkdir(r->destination);
        case 8:return fd_remove(r->path);
    }return FD_BADNAME;
    case BANK_ROWS:return wt_rows(r->tool,r->w);
    case BANK_SCROLL:return wt_scroll(r->tool,r->w,r->h,r->top,r->active);
    case BANK_POINTER:return wt_pointer(r->tool,r->x,r->y,r->w,r->h,r->top,r->px,r->py,r->key);
    case BANK_FINISH:wt_finish();break;
    }
    return 0;
}
