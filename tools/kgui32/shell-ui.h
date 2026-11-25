#ifndef KGUI32_SHELL_UI_H
#define KGUI32_SHELL_UI_H
#include "scene.h"

enum { SHELL_CLOSED, SHELL_KUMA, SHELL_SEARCH, SHELL_SWITCH, SHELL_ABOUT, SHELL_RECENT, SHELL_REBOOT };
enum { SHELL_ROWS=6 };
typedef struct { int action; const char *name, *command; } ShellApp;

const ShellApp *shell_app(int action);
Rect shell_button(void);
Rect shell_panel(void);
Rect shell_row(unsigned row);
unsigned shell_count(const Scene *s);
int shell_item(const Scene *s, unsigned index);
const char *shell_label(const Scene *s, unsigned index);
void shell_record_launch(Scene *,int);
int shell_pinned(const Scene *,int);
const ShellDocument *shell_document(const Scene *,int);
int shell_accept_document(Scene *,const ShellDocument *);
int shell_history_valid(const ShellHistory *);
void shell_timer(Scene *,unsigned modifiers);
int shell_key(Scene *s, const GuiEvent *e);
int shell_mouse(Scene *s, const GuiEvent *e, int down);
#endif
