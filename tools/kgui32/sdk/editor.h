#ifndef KG_EDITOR_H
#define KG_EDITOR_H
#define KE_BYTES 65536u
#define KE_HISTORY 8u
typedef struct {unsigned n,cursor,anchor;char text[KE_BYTES+1];} KeVersion;
typedef struct {KeVersion v[KE_HISTORY];unsigned first,count,current;char clean[KE_BYTES+1];unsigned clean_n;} KeDocument;
KeVersion *ke_view(KeDocument *);
int ke_load(KeDocument *,const char *,unsigned);
int ke_replace(KeDocument *,const char *,unsigned);
int ke_undo(KeDocument *,int);
int ke_dirty(KeDocument *);
void ke_select(KeDocument *,unsigned,unsigned);
#endif
