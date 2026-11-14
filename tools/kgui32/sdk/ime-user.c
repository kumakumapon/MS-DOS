#include "ime-user.h"
#include "text.h"
static unsigned char draft[KI_DICTIONARY];
static unsigned word(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static void put(unsigned char *p,unsigned at,unsigned n){for(unsigned i=0;i<4;i++)p[at+i]=n>>(i*8);}
static void copy(unsigned char *a,const unsigned char *b,unsigned n){for(unsigned i=0;i<n;i++)a[i]=b[i];}
static int same(const unsigned char *a,const char *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=(unsigned char)b[i])return 0;return 1;}
static void header(unsigned char *p,unsigned bytes,unsigned entries){
 for(unsigned i=0;i<32;i++)p[i]=0;
 copy(p,(const unsigned char *)"KGIMED1",7);put(p,8,1);put(p,12,bytes);put(p,16,entries);
 unsigned hash=2166136261u;for(unsigned i=32;i<bytes;i++)hash=(hash^p[i])*16777619u;put(p,20,hash);
}
void ki_user_init(KiUser *u){header(u->data,32,0);u->bytes=32;u->dirty=0;}
int ki_user_load(KiUser *u,const void *p,unsigned n){KiDictionary d;int e=ki_dictionary(&d,p,n);if(e)return e;if(d.entries>256)return -27;const unsigned char *data=p;for(unsigned at=32;at<n;at+=word(data+at))for(unsigned prior=32;prior<at;prior+=word(data+prior))if(data[prior+2]==data[at+2]&&same(data+prior+4,(const char *)data+at+4,data[at+2]))return -22;copy(u->data,p,n);u->bytes=n;u->dirty=0;return 0;}
int ki_user_view(const KiUser *u,KiDictionary *d){return ki_dictionary(d,u->data,u->bytes);}
static int modify(KiUser *u,const char *reading,const char *value,int remove){
 unsigned key=ki_length(reading),len=value?ki_length(value):0;if(!key||key>KI_TEXT||!kt_valid(reading,key,0)||(!remove&&(!len||len>KI_TEXT||!kt_valid(value,len,0))))return -22;
 KiDictionary d;if(ki_user_view(u,&d))return -22;unsigned out=32,entries=0,found=0;
 for(unsigned at=32;at<u->bytes;at+=word(u->data+at)){
  unsigned bytes=word(u->data+at);int match=u->data[at+2]==key&&same(u->data+at+4,reading,key);
  if(match){found=1;if(remove)continue;
   unsigned p=at+4+key;for(unsigned i=0;i<u->data[at+3];i++){unsigned n=u->data[p++];if(n==len&&same(u->data+p,value,n))return 0;p+=n;}
   if(u->data[at+3]>=12||bytes+1+len>KI_DICTIONARY-out)return -27;
   copy(draft+out,u->data+at,bytes);draft[out]=(bytes+1+len)&255;draft[out+1]=(bytes+1+len)>>8;draft[out+3]++;draft[out+bytes]=len;copy(draft+out+bytes+1,(const unsigned char *)value,len);out+=bytes+1+len;
  }else {if(bytes>KI_DICTIONARY-out)return -27;copy(draft+out,u->data+at,bytes);out+=bytes;}
  entries++;
 }
 if(!found&&!remove){unsigned n=5+key+len;if(n>KI_DICTIONARY-out||entries==256)return -27;draft[out]=n&255;draft[out+1]=n>>8;draft[out+2]=key;draft[out+3]=1;copy(draft+out+4,(const unsigned char *)reading,key);draft[out+4+key]=len;copy(draft+out+5+key,(const unsigned char *)value,len);out+=n;entries++;}
 if(!found&&remove)return -2;
 header(draft,out,entries);KiDictionary checked;if(ki_dictionary(&checked,draft,out))return -22;
 copy(u->data,draft,out);u->bytes=out;u->dirty=1;return 0;
}
int ki_user_add(KiUser *u,const char *reading,const char *word){return modify(u,reading,word,0);}
int ki_user_remove(KiUser *u,const char *reading){return modify(u,reading,0,1);}
