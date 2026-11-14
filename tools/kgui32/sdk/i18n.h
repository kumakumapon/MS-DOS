#ifndef KG_I18N_H
#define KG_I18N_H
typedef struct {const char *english,*japanese;} KuPair;
extern unsigned ku_language;
const char *ku_lookup(const KuPair *,unsigned,const char *);
#endif
