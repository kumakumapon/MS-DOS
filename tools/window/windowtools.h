#ifndef WINDOWTOOLS_H
#define WINDOWTOOLS_H
#ifdef KDESK_BANK_TOOLS
#define wt_open tool_open
#define wt_draw tool_draw
#define wt_key tool_key
#define wt_close tool_close
#define wt_clean tool_clean
#define wt_click tool_click
#define wt_finish tool_finish
#endif
enum { WT_EDITOR, WT_SETTINGS, WT_COUNT };
int wt_open(int tool,const char *path);
void wt_draw(int tool,int x,int y,int w,int h,int top,int active);
void wt_key(int tool,int key);
int wt_close(int tool);
int wt_clean(void);
int wt_click(int tool,int x,int y,int w,int h,int top,int px,int py);
void wt_finish(void);
#endif
