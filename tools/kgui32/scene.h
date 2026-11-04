#ifndef KGUI32_SCENE_H
#define KGUI32_SCENE_H
#include "gui.h"
typedef struct {int x,y,w,h;} Rect;
typedef struct {Rect r;int open,hidden;} GuiWindow;
typedef struct {
 int width,height,mx,my,buttons,active,drag,dx,dy;
 GuiWindow window[2];Rect damage;
 u32 counters[2],states[2];int action_result;
} Scene;
enum {ACT_NONE,ACT_EXIT,ACT_START,ACT_STOP};
void scene_init(Scene *,int,int);
void scene_damage(Scene *,Rect);
int scene_event(Scene *,const GuiEvent *);
#endif
