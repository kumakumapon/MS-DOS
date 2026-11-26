#ifndef KG_PREFERENCES_H
#define KG_PREFERENCES_H
#include "../../k38632/sdk/io.h"

#define KG_PREFERENCES_FILE "KGUIPREF.DAT"

enum { KG_THEME_CLASSIC, KG_THEME_CONTRAST };
enum { KG_WALL_GRADIENT, KG_WALL_SOLID, KG_WALL_GRID, KG_WALL_COUNT };
enum { KG_APP_WELCOME, KG_APP_FILES, KG_APP_EDITOR, KG_APP_CLOCK, KG_APP_CALENDAR, KG_APP_IMAGES, KG_APP_COUNT };
typedef struct { unsigned language,theme,ime,large,wallpaper,keyboard,mouse,date,time,startup; } KgPreferences;

int kg_state_read(const char *,void *,unsigned,K32Revision *);
int kg_preferences_load(KgPreferences *,K32Revision *);
int kg_preferences_save(const KgPreferences *,const K32Revision *);
#endif
