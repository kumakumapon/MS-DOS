#ifndef WINDOWTOOLS_H
#define WINDOWTOOLS_H
enum { WT_EDITOR, WT_SETTINGS, WT_COUNT };
int wt_open(int tool,const char *path);
void wt_draw(int tool,int x,int y,int w,int h,int top,int active);
void wt_key(int tool,int key);
int wt_close(int tool);
int wt_clean(void);
int wt_click(int tool,int x,int y,int w,int h,int top,int px,int py);
void wt_finish(void);
#endif
