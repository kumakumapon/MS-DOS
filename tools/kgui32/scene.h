#ifndef KGUI32_SCENE_H
#define KGUI32_SCENE_H
#include "gui.h"
typedef struct {int x,y,w,h;} Rect;
typedef struct {Rect r;int open,hidden;} GuiWindow;
typedef struct {
 int width,height,mx,my,buttons,active,drag,dx,dy;
 GuiWindow window[4];Rect damage;
 u32 counters[2],states[2];int action_result;
 int order[4],slot,key,resize,scroll_x[2],scroll_y[2],graphical[2];
} Scene;
enum {ACT_NONE,ACT_EXIT,ACT_START,ACT_STOP,ACT_NEW,ACT_RUN,ACT_KEY,ACT_END,ACT_PAUSE,ACT_RESUME,ACT_RESTART,ACT_SCROLL,ACT_CALCULATOR,ACT_CLOCK,ACT_SHELL,ACT_FILES};
void scene_init(Scene *,int,int);
void scene_damage(Scene *,Rect);
void scene_window_damage(Scene *,int,Rect);
void scene_focus(Scene *,int);
int scene_event(Scene *,const GuiEvent *);
#endif
