#include "i18n.h"
const char *ku_lookup(const KuPair *table,unsigned count,const char *text){
 if(!ku_language)return text;
 for(unsigned i=0;i<count;i++){
  const char *a=table[i].english,*b=text;
  while(*a&&*a==*b){a++;b++;}
  if(!*a&&!*b)return table[i].japanese;
 }
 return text;
}
