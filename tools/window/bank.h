#ifndef KDESK_BANK_H
#define KDESK_BANK_H
enum { BANK_OPEN,BANK_DRAW,BANK_KEY,BANK_CLOSE,BANK_CLEAN,BANK_CLICK,BANK_FINISH };
typedef struct {int tool,x,y,w,h,top,active,key,px,py;const char *path;} ToolRequest;
int tools_entry(int op,ToolRequest *r);
#endif
