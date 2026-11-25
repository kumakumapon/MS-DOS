#ifndef KG_PREFERENCES_H
#define KG_PREFERENCES_H
#include "../../k38632/sdk/io.h"

#define KG_PREFERENCES_FILE "KGUIPREF.DAT"

enum { KG_THEME_CLASSIC, KG_THEME_CONTRAST };
typedef struct { unsigned language,theme,ime; } KgPreferences;

int kg_state_read(const char *,void *,unsigned,K32Revision *);
int kg_preferences_load(KgPreferences *,K32Revision *);
int kg_preferences_save(const KgPreferences *,const K32Revision *);
#endif
