#include <assert.h>
#include <string.h>
#include "sdk/editor.h"
#include "sdk/text.h"
static KeDocument d;static char full[KE_BYTES];
int main(void){
 assert(!ke_load(&d,"A\r\nB",4));assert(!strcmp(ke_view(&d)->text,"A\nB"));assert(!ke_dirty(&d));
 ke_select(&d,1,2);assert(ke_replace(&d,"xyz",3)==1);assert(!strcmp(ke_view(&d)->text,"AxyzB"));assert(ke_dirty(&d));
 assert(ke_undo(&d,0));assert(!ke_dirty(&d));assert(ke_undo(&d,1));assert(!strcmp(ke_view(&d)->text,"AxyzB"));
 assert(ke_undo(&d,0));ke_select(&d,3,3);ke_replace(&d,"C",1);assert(!ke_undo(&d,1));
 for(unsigned i=0;i<100;i++){KeVersion *v=ke_view(&d);ke_select(&d,v->n,v->n);assert(ke_replace(&d,"x",1)==1);}
 unsigned n=ke_view(&d)->n;for(unsigned i=0;i<KE_HISTORY-1;i++)assert(ke_undo(&d,0));assert(!ke_undo(&d,0));for(unsigned i=0;i<KE_HISTORY-1;i++)assert(ke_undo(&d,1));assert(ke_view(&d)->n==n);
 memset(full,'a',sizeof full);assert(!ke_load(&d,full,sizeof full));ke_select(&d,KE_BYTES,KE_BYTES);assert(ke_replace(&d,"x",1)==-1);assert(!ke_dirty(&d));assert(ke_load(&d,"\x80",1)==-1);assert(!ke_dirty(&d));ke_select(&d,0,KE_BYTES);assert(ke_replace(&d,"",0)==1);assert(!ke_view(&d)->n);assert(ke_undo(&d,0));assert(!ke_dirty(&d));
 ke_select(&d,0,1);assert(ke_replace(&d,"z",1)==1);assert(ke_dirty(&d));unsigned count=d.count,current=d.current;ke_saved(&d);assert(!ke_dirty(&d)&&d.count==count&&d.current==current);assert(ke_undo(&d,0)&&ke_dirty(&d));assert(ke_undo(&d,1)&&!ke_dirty(&d));
 /* Literal search wraps, preserves a failed selection, and all replacement
  * is one history step with preflighted capacity and format preservation. */
 assert(!ke_load(&d,"one two one\r\n",13));assert(ke_view(&d)->crlf);
 assert(ke_find(&d,"one",3,0,1)==1&&ke_view(&d)->anchor==0&&ke_view(&d)->cursor==3);
 assert(ke_find(&d,"one",3,3,1)==1&&ke_view(&d)->anchor==8);
 assert(ke_find(&d,"one",3,11,1)==1&&ke_view(&d)->anchor==0);
 unsigned anchor=ke_view(&d)->anchor,cursor=ke_view(&d)->cursor;
 assert(!ke_find(&d,"absent",6,0,1)&&ke_view(&d)->anchor==anchor&&ke_view(&d)->cursor==cursor);
 assert(ke_find(&d,"",0,0,1)==-22);
 assert(ke_replace_all(&d,"one",3,"3",1)==2&&!strcmp(ke_view(&d)->text,"3 two 3\n"));
 assert(ke_view(&d)->crlf&&ke_dirty(&d));assert(ke_undo(&d,0)&&!ke_dirty(&d));assert(ke_undo(&d,1));
 assert(ke_set_eol(&d,0)==1&&ke_dirty(&d));assert(ke_undo(&d,0)&&ke_view(&d)->crlf);assert(ke_undo(&d,1)&&!ke_view(&d)->crlf);
 char serialized[32];assert(ke_serialize(&d,serialized,sizeof serialized)==8&&!memcmp(serialized,"3 two 3\n",8));
 assert(ke_set_eol(&d,1)==1);assert(ke_serialize(&d,serialized,sizeof serialized)==9&&!memcmp(serialized,"3 two 3\r\n",9));
 ke_saved(&d);assert(!ke_dirty(&d));assert(ke_undo(&d,0)&&ke_dirty(&d));assert(ke_undo(&d,1)&&!ke_dirty(&d));
 memset(full,'a',sizeof full);assert(!ke_load(&d,full,sizeof full));count=d.count;current=d.current;
 assert(ke_replace_all(&d,"a",1,"aa",2)==-27&&d.count==count&&d.current==current&&!ke_dirty(&d));
 assert(ke_replace_all(&d,"a",1,"\x80",1)==-22&&!ke_dirty(&d));
 assert(ke_replace_all(&d,"aa",2,"",0)==32768&&!ke_view(&d)->n);assert(ke_undo(&d,0)&&!ke_dirty(&d));
 memset(full,'\n',sizeof full);assert(!ke_load(&d,full,sizeof full));assert(ke_set_eol(&d,1)==1);
 assert(ke_serialize(&d,serialized,sizeof serialized)==-27&&ke_dirty(&d));
 assert(ke_load(&d,"bare\r",5)==-1&&ke_view(&d)->n==KE_BYTES);
 /* Trail bytes may look like ASCII punctuation. They must not become cursor
  * positions, search matches, replacement targets, or clipped draw fragments. */
 const char jp[]="A\x93\xfa\x96\x7b\x8c\xea\xb6\xc5\n";
 assert(!ke_load(&d,jp,sizeof jp-1)&&!ke_dirty(&d));
 ke_select(&d,2,6);assert(ke_view(&d)->anchor==1&&ke_view(&d)->cursor==5);
 assert(kt_next(jp,sizeof jp-1,1)==3&&kt_prev(jp,sizeof jp-1,3)==1);
 assert(ke_find(&d,"{",1,0,1)==0);assert(!ke_replace_all(&d,"{",1,"X",1));
 assert(ke_find(&d,"\x96\x7b",2,2,1)==1&&ke_view(&d)->anchor==3&&ke_view(&d)->cursor==5);
 assert(ke_replace(&d,"J",1)==1);assert(ke_undo(&d,0)&&!ke_dirty(&d));
 assert(ke_set_eol(&d,1)==1);assert(ke_serialize(&d,serialized,sizeof serialized)==(int)sizeof jp);
 assert(!memcmp(serialized,jp,sizeof jp-2)&&serialized[sizeof jp-2]=='\r'&&serialized[sizeof jp-1]=='\n');
 const char *bad_sjis[]={"\x80","\xa0","\xfd","\x81","\x81\x7f","\x81\x30","\xf0\x40"};
 unsigned saved_n=ke_view(&d)->n;count=d.count;
 for(unsigned i=0;i<sizeof bad_sjis/sizeof *bad_sjis;i++){
  unsigned n=strlen(bad_sjis[i]);assert(ke_load(&d,bad_sjis[i],n)==-1&&ke_view(&d)->n==saved_n&&d.count==count);
  assert(ke_replace(&d,bad_sjis[i],n)==-1&&d.count==count);
  assert(ke_find(&d,bad_sjis[i],n,0,1)==-22);
 }
 char wide[50];memset(wide,'a',46);wide[46]='\x93';wide[47]='\xfa';wide[48]='X';wide[49]=0;
 assert(kt_prefix(wide,49,47)==46&&kt_prefix(wide+46,3,2)==2);
 assert(kt_glyph(0x93fa)==2&&kt_glyph(0xb6)==1&&!kt_glyph(0x817f)&&!kt_glyph(0x10093fa));
 assert(!ke_load(&d,"\x93\xfa\x93\xfa",4));assert(ke_replace_all(&d,"\x93\xfa",2,"\xb6",1)==2);
 assert(ke_view(&d)->n==2&&!memcmp(ke_view(&d)->text,"\xb6\xb6",2));assert(ke_undo(&d,0)&&!ke_dirty(&d));
 return 0;
}
