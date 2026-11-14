#ifndef KG_IME_STORE_H
#define KG_IME_STORE_H
#include "ime-user.h"
#include "../../k38632/sdk/io.h"
#define KI_USER_FILE "USERIME.DIC"
typedef struct {KiUser words;K32Revision revision;unsigned loaded;int error;} KiStore;
void ki_store_init(KiStore *);
int ki_store_load(KiStore *);
int ki_store_save(KiStore *);
int ki_store_inspect(unsigned,KiDictionary *,K32Revision *);
int ki_store_restore(unsigned,const char *,const K32Revision *);
#endif
