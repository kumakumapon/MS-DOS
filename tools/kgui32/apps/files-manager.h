#ifndef KG_FILES_MANAGER_H
#define KG_FILES_MANAGER_H
#include "../sdk/kgui.h"
void files_manager_open(const char *root);
int files_manager_draw(void);
int files_manager_event(const KgEvent *event);
int files_manager_step(void);
void files_manager_close(void);
#endif
