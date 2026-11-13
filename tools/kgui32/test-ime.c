/* Expected Japanese literals are compiled as Shift-JIS, never runtime UTF-8. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sdk/ime.h"
#include "sdk/editor.h"
#include "sdk/widgets.h"
#include "sdk/text.h"
static KiDictionary dict;static unsigned char bytes[KI_DICTIONARY],bad[KI_DICTIONARY];
static KeDocument doc;
static KiIme fresh(unsigned scheme){return (KiIme){.scheme=scheme};}
static void type(KiIme *v,const char *s){for(;*s;s++)assert(ki_input(v,(unsigned char)*s,0)>0);}
static void expected(KiIme *v,const char *s,unsigned line){char out[KI_TEXT+1];unsigned n=ki_preview(v,out);if(strcmp(out,s)){fprintf(stderr,"preedit mismatch at test line %u: ",line);for(unsigned i=0;i<n;i++)fprintf(stderr,"%02x",(unsigned char)out[i]);fprintf(stderr," != ");for(unsigned i=0;s[i];i++)fprintf(stderr,"%02x",(unsigned char)s[i]);fputc('\n',stderr);abort();}assert(n==strlen(s));assert(kt_valid(out,n,0));}
#define expect(v,s) expected(v,s,__LINE__)
static void hash_header(unsigned char *b,unsigned n){unsigned h=2166136261u;for(unsigned i=32;i<n;i++)h=(h^b[i])*16777619u;for(unsigned i=0;i<4;i++)b[20+i]=h>>(i*8);}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);unsigned n=fread(bytes,1,sizeof bytes,f);assert(feof(f));assert(!fclose(f));
 assert(!ki_dictionary(&dict,bytes,n));assert(dict.entries>700);
 KiIme v=fresh(KI_ROMAJI);
 type(&v,"nihongo");expect(&v,"にほんご");assert(ki_convert(&v,&dict)>0);assert(!strcmp(ki_result(&v),"日本語"));
 ki_clear(&v);type(&v,"konnichiha");expect(&v,"こんにちは");
 ki_clear(&v);type(&v,"nnya");expect(&v,"んにゃ");
 ki_clear(&v);type(&v,"n'ya");expect(&v,"んや");
 ki_clear(&v);type(&v,"nnka");expect(&v,"んか");
 ki_clear(&v);type(&v,"gakkou");expect(&v,"がっこう");
 ki_clear(&v);type(&v,"shashin");expect(&v,"しゃしn");assert(!ki_prepare(&v));expect(&v,"しゃしん");
 ki_clear(&v);type(&v,"kya");assert(ki_input(&v,8,0)>0);expect(&v,"き");
 ki_clear(&v);type(&v,"xtsuxaxixuxexo");expect(&v,"っぁぃぅぇぉ");
 ki_clear(&v);type(&v,"fashiti");expect(&v,"ふぁしち");
 ki_clear(&v);type(&v,"va");expect(&v,"ヴぁ");
 ki_clear(&v);type(&v,"ka");type(&v,"ky");KiIme before=v;assert(ki_convert(&v,&dict)<0);expect(&v,"かky");assert(v.n==before.n&&v.pending==before.pending);
 assert(ki_input(&v,8,0)>0);assert(ki_input(&v,8,0)>0);expect(&v,"か");
 assert(ki_input(&v,260,0)>0);type(&v,"ni");expect(&v,"にか");assert(ki_input(&v,264,0)>0);expect(&v,"に");
 assert(ki_input(&v,258,0)>0);assert(ki_input(&v,259,0)>0);assert(v.cursor==v.n);
 ki_clear(&v);type(&v,"kanji");assert(ki_convert(&v,&dict)>0);assert(v.count==5);assert(!strcmp(ki_result(&v),"漢字"));
 ki_move_candidate(&v,1);assert(!strcmp(ki_result(&v),"感じ"));ki_move_candidate(&v,-2);assert(!strcmp(ki_result(&v),"カンジ"));
 ki_clear(&v);type(&v,"kou");assert(ki_convert(&v,&dict)>0);assert(v.count==14);ki_move_candidate(&v,6);assert(v.selected==6);ki_move_candidate(&v,-7);assert(v.selected==13);
 ki_clear(&v);type(&v,"mofumofu");assert(ki_convert(&v,&dict)>0);assert(v.count==2);assert(!strcmp(ki_result(&v),"もふもふ"));
 v=fresh(KI_KANA);type(&v,"t@f[ez");expect(&v,"がぱいつ");
 assert(ki_input(&v,'3',1)>0);assert(ki_input(&v,'z',1)>0);expect(&v,"がぱいつぁっ");
 ki_clear(&v);type(&v,"f[@@");expect(&v,"は");type(&v,"[");expect(&v,"ぱ");
 ki_clear(&v);assert(ki_input(&v,0xb6,0)>0);assert(ki_input(&v,0xde,0)>0);expect(&v,"が");v.katakana=1;expect(&v,"ガ");
 ki_clear(&v);type(&v,"4@");expect(&v,"ヴ");
 v=fresh(KI_ROMAJI);for(unsigned i=0;i<48;i++)type(&v,"a");assert(v.n==96);before=v;assert(ki_input(&v,'a',0)==-27);assert(v.n==before.n&&!memcmp(v.reading,before.reading,sizeof v.reading));
 assert(ki_input(&v,8,0)>0);type(&v,"n");before=v;/* 94 + n flush fits exactly. */assert(!ki_prepare(&v));assert(v.n==96);
 ki_clear(&v);type(&v,"a");before=v;assert(ki_input(&v,'q',0)==-22);assert(!memcmp(v.reading,before.reading,sizeof v.reading)&&v.pending==before.pending);
 assert(!ki_dictionary(&(KiDictionary){0},bytes,n));
 for(unsigned cut=0;cut<n;cut++)assert(ki_dictionary(&(KiDictionary){0},bytes,cut)<0);
 memcpy(bad,bytes,n);bad[n-1]^=1;assert(ki_dictionary(&(KiDictionary){0},bad,n)<0);
 memcpy(bad,bytes,n);bad[32]=0;bad[33]=0;hash_header(bad,n);assert(ki_dictionary(&(KiDictionary){0},bad,n)<0);
 memcpy(bad,bytes,n);bad[34]=97;hash_header(bad,n);assert(ki_dictionary(&(KiDictionary){0},bad,n)<0);
 memcpy(bad,bytes,n);bad[35]=255;hash_header(bad,n);assert(ki_dictionary(&(KiDictionary){0},bad,n)<0);
 memcpy(bad,bytes,n);bad[36]=0;hash_header(bad,n);assert(ki_dictionary(&(KiDictionary){0},bad,n)<0);
 memcpy(bad,bytes,n);bad[24]=1;assert(ki_dictionary(&(KiDictionary){0},bad,n)<0);
 KiDictionary empty={0};ki_clear(&v);type(&v,"kana");assert(ki_convert(&v,&empty)>0);assert(v.count==2&&v.note);expect(&v,"かな");
 /* Composition and conversion never touch the model; one explicit commit is
    one edit, including replacement, clean state and redo. */
 assert(!ke_load(&doc,"ABCD",4));ke_select(&doc,1,3);unsigned history=doc.current;
 ki_clear(&v);type(&v,"nihon");assert(ki_convert(&v,&dict)>0);assert(doc.current==history&&!ke_dirty(&doc));
 const char *result=ki_result(&v);assert(ke_replace(&doc,result,ki_length(result))>0);assert(!strcmp(ke_view(&doc)->text,"A日本D"));
 assert(ke_undo(&doc,0)>0);assert(!strcmp(ke_view(&doc)->text,"ABCD")&&!ke_dirty(&doc));assert(ke_undo(&doc,1)>0);assert(!strcmp(ke_view(&doc)->text,"A日本D"));
 char *wide=malloc(KE_BYTES);assert(wide);memset(wide,'x',KE_BYTES);assert(!ke_load(&doc,wide,KE_BYTES));free(wide);ke_select(&doc,KE_BYTES,KE_BYTES);history=doc.current;before=v;
 assert(ke_replace(&doc,result,ki_length(result))<0);assert(doc.current==history&&ke_view(&doc)->n==KE_BYTES&&!ke_dirty(&doc));assert(!memcmp(&v,&before,sizeof v));
 KwInput field={0};assert(kw_insert(&field,result,ki_length(result))>0);assert(!strcmp(field.text,"日本"));
 puts("IME: romaji, JIS kana, accents, candidates, malformed dictionary, bounds and atomic history passed");return 0;
}
