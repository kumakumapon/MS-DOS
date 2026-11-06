/* SDK 2: pure input policy. Rendering is separate; no native/DOS pointers. */
#ifndef KGUI_WIDGETS_H
#define KGUI_WIDGETS_H
#include "protocol.h"
#define KW_VERSION 2
#define KW_INPUT_BYTES 64
typedef struct {int x,y,w,h;} KwRect;
typedef struct {char text[KW_INPUT_BYTES];unsigned length,cursor;} KwInput;
typedef struct {unsigned count,selected,top,rows;} KwList;
typedef struct {KwRect r;int dragging,offset;unsigned top,total,rows;} KwScroll;
typedef struct {unsigned current,count;} KwFocus;
int kw_inside(KwRect,int,int);
int kw_focus(KwFocus *,const KgEvent *);
int kw_input(KwInput *,const KgEvent *);
void kw_reveal(KwList *);
int kw_list(KwList *,KwRect,const KgEvent *);
int kw_scroll(KwScroll *,const KgEvent *);
int kw_button(unsigned,const KgEvent *);
int kw_dialog(const KgEvent *);
void kw_text(int,int,const char *,unsigned,u32);
void kw_draw_input(KwRect,const KwInput *,int);
void kw_draw_scroll(const KwScroll *);
void kw_draw_dialog(unsigned,unsigned,const char *,const char *);
#endif
