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
#define wt_rows tool_rows
#define wt_scroll tool_scroll
#define wt_pointer tool_pointer
#endif
enum { WT_EDITOR, WT_SETTINGS, WT_VIEW, WT_PAINT, WT_COUNT };
int wt_open(int tool,const char *path);
void wt_draw(int tool,int x,int y,int w,int h,int top,int active);
void wt_key(int tool,int key);
int wt_close(int tool);
int wt_clean(void);
int wt_click(int tool,int x,int y,int w,int h,int top,int px,int py);
void wt_finish(void);
int wt_file(int operation,const char *source,const char *destination);
int wt_rows(int tool,int width);
int wt_scroll(int tool,int width,int height,int top,int follow);
int wt_pointer(int tool,int x,int y,int w,int h,int top,int px,int py,int buttons);
#endif
