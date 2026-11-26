#include "preferences.h"
#include "ime.h"

enum { PREFS_MAGIC=0x3150464b };
typedef struct { unsigned magic,version; KgPreferences values; } PreferencesFile;

int kg_state_read(const char *name,void *data,unsigned size,K32Revision *revision){
 *revision=(K32Revision){0};
 unsigned length;int file=k32_io_open(name,0,&length);
 if(file==-2)return 0;
 if(file<0)return file;
 int bytes=length==size?k32_io_read(file,data,size):-22;
 int closed=k32_io_close(file);
 if(closed<0)return closed;
 if(bytes!=(int)size)return bytes<0?bytes:-5;
 unsigned digest=2166136261u;
 const unsigned char *p=data;
 for(unsigned i=0;i<size;i++)digest=(digest^p[i])*16777619u;
 *revision=(K32Revision){1,size,digest};return 0;
}
static int valid(const KgPreferences *p){
 return p->language<=1&&p->theme<=KG_THEME_CONTRAST&&p->ime<=KI_KANA&&p->large<=1&&
  p->wallpaper<KG_WALL_COUNT&&p->keyboard<=1&&p->mouse<=2&&p->date<=2&&p->time<=1&&p->startup<KG_APP_COUNT;
}
int kg_preferences_load(KgPreferences *out,K32Revision *revision){
 PreferencesFile file={0};
 int e=kg_state_read(KG_PREFERENCES_FILE,&file,sizeof file,revision);
 unsigned version=3;
 if(e==-22){version=2;e=kg_state_read(KG_PREFERENCES_FILE,&file,24,revision);}
 if(e==-22){version=1;e=kg_state_read(KG_PREFERENCES_FILE,&file,20,revision);}
 if(e||!revision->exists)return e;
 if(file.magic!=PREFS_MAGIC||file.version!=version||!valid(&file.values))return -22;
 *out=file.values;return 0;
}
int kg_preferences_save(const KgPreferences *prefs,const K32Revision *revision){
 if(!valid(prefs))return -22;
 PreferencesFile file={PREFS_MAGIC,3,*prefs};
 return k32_io_save(KG_PREFERENCES_FILE,&file,sizeof file,revision);
}
