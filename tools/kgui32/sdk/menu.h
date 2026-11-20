/* Private CPL3 command sheet. No right click, hover or nested submenu. */
#ifndef KG_MENU_H
#define KG_MENU_H
#include "protocol.h"
#include "i18n.h"
#include "../../desktop-style.h"
#define KM_MAX 24u
#define KM_ITEM 24576u
#define KM_BACK 24608u
#define KM_PREV 24609u
#define KM_NEXT 24610u
#define KM_ROW_HEIGHT (DESKTOP_CONTROL_HEIGHT + DESKTOP_SPACE_SMALL)
typedef struct {const char *label;unsigned type,key,control;} KmItem;
typedef struct {const KmItem *items;unsigned count,open,enabled,selected,rows;const KuPair *labels;unsigned label_count,disabled,checked,keyboard;} KmMenu;
/* 0: application event, 1: sheet owns event, 2: selected application command. */
int km_event(KmMenu *,KgEvent *,unsigned);
void km_draw(const KmMenu *);
void kg_menu(const KmItem *,unsigned);
void kg_menu_enable(unsigned);
void kg_menu_state(unsigned enabled,unsigned checked);
void kg_menu_labels(const KuPair *,unsigned);
#endif
