#ifndef KGUI_APP_STATS
#define KGUI_APP_STATS
#include "../sdk/kgui.h"
typedef struct {u32 magic,version,frames,events,keys,clicks,value,seconds,width,height,token,heartbeat,error,mode,reserved[2];} KgStats;
_Static_assert(sizeof(KgStats)==64,"client diagnostic samples");
static inline int same(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
#endif
