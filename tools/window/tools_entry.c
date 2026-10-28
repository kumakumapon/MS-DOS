#include "bank.h"
#include "windowtools.h"
int tools_entry(int op,ToolRequest *r){
    switch(op){
    case BANK_OPEN:return wt_open(r->tool,r->path);
    case BANK_DRAW:wt_draw(r->tool,r->x,r->y,r->w,r->h,r->top,r->active);break;
    case BANK_KEY:wt_key(r->tool,r->key);break;
    case BANK_CLOSE:return wt_close(r->tool);
    case BANK_CLEAN:return wt_clean();
    case BANK_CLICK:return wt_click(r->tool,r->x,r->y,r->w,r->h,r->top,r->px,r->py);
    case BANK_FINISH:wt_finish();break;
    }
    return 0;
}
