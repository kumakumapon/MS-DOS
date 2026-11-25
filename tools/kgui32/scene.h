#ifndef KGUI32_SCENE_H
#define KGUI32_SCENE_H
#include "gui.h"
#include "sdk/protocol.h"
typedef struct {int x,y,w,h;} Rect;
typedef struct {Rect r;int open,hidden;} GuiWindow;
enum {
 GN_NONE, GN_SLOTS_FULL, GN_START_FAILED, GN_APP_MISSING, GN_WORKERS_STARTED, GN_WORKERS_STOPPED,
 GN_WORKER_FAILED, GN_SESSION_FAILED, GN_APP_FAULTED, GN_LAYOUT_SAVE_FAILED
};
typedef struct {
 int width,height,mx,my,buttons,active,drag,dx,dy;
 GuiWindow window[6];Rect damage,restored[4];
 u32 counters[4],states[4];int action_result;
 int order[6],slot,key,resize,help,scroll_x[4],scroll_y[4],graphical[4],slots,menu[4],shell[4],command[4],placement[4];
 u32 ticks,notice_started;
 unsigned notice,notice_click;
 unsigned shell_page,shell_selected,shell_top,shell_length,shell_swallow,shell_alt;
 char shell_query[28];
 unsigned app_pins,app_recent[12],recent_apps,document_index;
 const volatile KgRecentDocument *documents;
} Scene;
enum {ACT_NONE,ACT_EXIT,ACT_START,ACT_STOP,ACT_NEW,ACT_RUN,ACT_KEY,ACT_END,ACT_PAUSE,ACT_RESUME,ACT_RESTART,ACT_SCROLL,ACT_CALCULATOR,ACT_CLOCK,ACT_SHELL,ACT_FILES,ACT_EDITOR,ACT_IMAGE,ACT_WORDS,ACT_SHELL32,ACT_CALENDAR,ACT_SETTINGS,ACT_MEMO,ACT_NOTICE_INPUT,ACT_REBOOT,ACT_OPEN_RECENT,ACT_PIN_CHANGED};
enum { LAUNCHER_BUTTONS=10, LAUNCHER_HINT_Y=258 };
enum { LAUNCHER_APPS, LAUNCHER_GUIDE, LAUNCHER_GALLERY };
Rect scene_launcher_button(unsigned index);
Rect scene_notice_rect(const Scene *);
void scene_notice_set(Scene *,unsigned kind);
void scene_notice_tick(Scene *,u32 ticks);
void scene_init(Scene *,int,int);
void scene_slots(Scene *,unsigned);
int scene_event4(Scene *,const GuiEvent4 *);
void scene_damage(Scene *,Rect);
void scene_window_damage(Scene *,int,Rect);
void scene_focus(Scene *,int);
void scene_close(Scene *,int);
void scene_restore_window(Scene *,int,Rect,unsigned);
int scene_event(Scene *,const GuiEvent *);
#endif
