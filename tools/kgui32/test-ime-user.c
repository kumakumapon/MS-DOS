#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sdk/ime-user.h"
static unsigned char bytes[KI_DICTIONARY],broken[KI_DICTIONARY];static KiUser user,before;
static void type(KiIme *v,const char *s){for(;*s;s++)assert(ki_key_event(v,(unsigned char)*s,0,0,0,0)==1);}
static void put(unsigned char *p,unsigned at,unsigned v){for(unsigned i=0;i<4;i++)p[at+i]=v>>(i*8);}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);unsigned n=fread(bytes,1,sizeof bytes,f);assert(!fclose(f));KiDictionary system,custom;assert(!ki_dictionary(&system,bytes,n));
 ki_user_init(&user);assert(!ki_user_view(&user,&custom)&&custom.entries==0);
 assert(!ki_user_add(&user,"くま","画像"));assert(!ki_user_view(&user,&custom)&&custom.entries==1&&user.dirty);
 KiIme ime={.scheme=KI_ROMAJI};type(&ime,"kuma");assert(ki_convert_user(&ime,&system,&custom)>0&&!strcmp(ki_result(&ime),"画像"));
 char commit[KI_TEXT+1];KiIme held=ime;assert(ki_key_event(&ime,13,0,&system,&custom,commit)==2&&!strcmp(commit,"画像")&&!memcmp(&held,&ime,sizeof ime));
 assert(ki_key_event(&ime,283,0,&system,&custom,commit)==1&&ime.scheme==KI_ROMAJI&&ime.n==held.n);
 ki_clear(&ime);type(&ime,"gazouhozon");assert(ki_convert_user(&ime,&system,&custom)>0&&!strcmp(ki_result(&ime),"画像保存"));
 ki_clear(&ime);type(&ime,"nihongowokakimasu");assert(ki_convert_user(&ime,&system,&custom)>0&&!strcmp(ki_result(&ime),"日本語を書きます"));
 ki_clear(&ime);type(&ime,"yonda");assert(ki_convert_user(&ime,&system,&custom)>0&&!strcmp(ki_result(&ime),"読んだ"));
 ki_clear(&ime);type(&ime,"tabenai");assert(ki_convert_user(&ime,&system,&custom)>0&&!strcmp(ki_result(&ime),"食べない"));
 before=user;assert(!ki_user_add(&user,"くま","画像")&&!memcmp(&before,&user,sizeof user));assert(ki_user_add(&user,"くま","\202")==-22&&!memcmp(&before,&user,sizeof user));
 memcpy(broken,user.data,user.bytes);broken[user.bytes-1]^=1;assert(ki_user_load(&user,broken,user.bytes)==-22&&!memcmp(&before,&user,sizeof user));
 unsigned record=user.bytes-32;memcpy(broken,user.data,user.bytes);memcpy(broken+user.bytes,user.data+32,record);unsigned total=user.bytes+record;put(broken,12,total);put(broken,16,2);unsigned hash=2166136261u;for(unsigned i=32;i<total;i++)hash=(hash^broken[i])*16777619u;put(broken,20,hash);assert(ki_user_load(&user,broken,total)==-22&&!memcmp(&before,&user,sizeof user));
 assert(!ki_user_remove(&user,"くま"));assert(!ki_user_view(&user,&custom)&&!custom.entries);before=user;assert(ki_user_remove(&user,"なし")==-2&&!memcmp(&before,&user,sizeof user));
 for(unsigned i=0;i<256;i++){char reading[16];snprintf(reading,sizeof reading,"key%u",i);assert(!ki_user_add(&user,reading,"value"));}
 before=user;assert(ki_user_add(&user,"overflow","value")==-27&&!memcmp(&before,&user,sizeof user));
 puts("common IME: compound/inflected conversion, user priority, proposed commit, malformed-load retention and bounded dictionary edits PASS");return 0;
}
