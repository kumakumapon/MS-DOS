#ifndef KG_IME_USER_H
#define KG_IME_USER_H
#include "ime.h"
typedef struct {unsigned char data[KI_DICTIONARY];unsigned bytes,dirty;} KiUser;
void ki_user_init(KiUser *);
int ki_user_load(KiUser *,const void *,unsigned);
int ki_user_add(KiUser *,const char *,const char *);
int ki_user_remove(KiUser *,const char *);
int ki_user_view(const KiUser *,KiDictionary *);
#endif
