#include <assert.h>
#include <string.h>
#include "sdk/editor.h"
static KeDocument d;static char full[KE_BYTES];
int main(void){
 assert(!ke_load(&d,"A\r\nB",4));assert(!strcmp(ke_view(&d)->text,"A\nB"));assert(!ke_dirty(&d));
 ke_select(&d,1,2);assert(ke_replace(&d,"xyz",3)==1);assert(!strcmp(ke_view(&d)->text,"AxyzB"));assert(ke_dirty(&d));
 assert(ke_undo(&d,0));assert(!ke_dirty(&d));assert(ke_undo(&d,1));assert(!strcmp(ke_view(&d)->text,"AxyzB"));
 assert(ke_undo(&d,0));ke_select(&d,3,3);ke_replace(&d,"C",1);assert(!ke_undo(&d,1));
 for(unsigned i=0;i<100;i++){KeVersion *v=ke_view(&d);ke_select(&d,v->n,v->n);assert(ke_replace(&d,"x",1)==1);}
 unsigned n=ke_view(&d)->n;for(unsigned i=0;i<KE_HISTORY-1;i++)assert(ke_undo(&d,0));assert(!ke_undo(&d,0));for(unsigned i=0;i<KE_HISTORY-1;i++)assert(ke_undo(&d,1));assert(ke_view(&d)->n==n);
 memset(full,'a',sizeof full);assert(!ke_load(&d,full,sizeof full));ke_select(&d,KE_BYTES,KE_BYTES);assert(ke_replace(&d,"x",1)==-1);assert(!ke_dirty(&d));assert(ke_load(&d,"\x80",1)==-1);assert(!ke_dirty(&d));ke_select(&d,0,KE_BYTES);assert(ke_replace(&d,"",0)==1);assert(!ke_view(&d)->n);assert(ke_undo(&d,0));assert(!ke_dirty(&d));
 return 0;
}
