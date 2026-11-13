#ifndef KGUI32_SCENE_H
#define KGUI32_SCENE_H
#include "gui.h"
typedef struct {int x,y,w,h;} Rect;
typedef struct {Rect r;int open,hidden;} GuiWindow;
typedef struct {
 int width,height,mx,my,buttons,active,drag,dx,dy;
 GuiWindow window[6];Rect damage;
 u32 counters[4],states[4];int action_result;
 int order[6],slot,key,resize,scroll_x[4],scroll_y[4],graphical[4],slots;
} Scene;
enum {ACT_NONE,ACT_EXIT,ACT_START,ACT_STOP,ACT_NEW,ACT_RUN,ACT_KEY,ACT_END,ACT_PAUSE,ACT_RESUME,ACT_RESTART,ACT_SCROLL,ACT_CALCULATOR,ACT_CLOCK,ACT_SHELL,ACT_FILES,ACT_EDITOR,ACT_IMAGE};
void scene_init(Scene *,int,int);
void scene_slots(Scene *,unsigned);
int scene_event4(Scene *,const GuiEvent4 *);
void scene_damage(Scene *,Rect);
void scene_window_damage(Scene *,int,Rect);
void scene_focus(Scene *,int);
void scene_close(Scene *,int);
int scene_event(Scene *,const GuiEvent *);
#endif
