#ifndef KGUI32_SHELL_UI_H
#define KGUI32_SHELL_UI_H
#include "scene.h"

enum { SHELL_CLOSED, SHELL_KUMA, SHELL_SEARCH, SHELL_SWITCH, SHELL_ABOUT };
enum { SHELL_ROWS=6 };
typedef struct { int action; const char *name, *command; } ShellApp;

const ShellApp *shell_app(int action);
Rect shell_button(void);
Rect shell_panel(void);
Rect shell_row(unsigned row);
unsigned shell_count(const Scene *s);
int shell_item(const Scene *s, unsigned index);
const char *shell_label(const Scene *s, unsigned index);
void shell_timer(Scene *,unsigned modifiers);
int shell_key(Scene *s, const GuiEvent *e);
int shell_mouse(Scene *s, const GuiEvent *e, int down);
#endif
