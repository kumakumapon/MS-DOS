#ifndef KG_SETTINGS_MODEL_H
#define KG_SETTINGS_MODEL_H
typedef struct {unsigned xms,ems,files,buffers,fd98;} KgBootSettings;
unsigned kg_settings_config_flags(const KgBootSettings *);
int kg_settings_decode(KgBootSettings *next,const KgBootSettings *old,unsigned startup,unsigned flags);
void kg_settings_read(KgBootSettings *,unsigned autoexec,const char *,unsigned);
int kg_settings_build(const KgBootSettings *,const KgBootSettings *,unsigned autoexec,
 const char *,unsigned,char *,unsigned);
#endif
