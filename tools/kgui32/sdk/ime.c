#include "ime.h"
#include "text.h"
#include "ime-tables.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
unsigned ki_length(const char *s){unsigned n=0;while(s[n])n++;return n;}
static int equal(const char *a,const char *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void copy(char *a,const char *b,unsigned n){for(unsigned i=0;i<n;i++)a[i]=b[i];a[n]=0;}
static unsigned u16(const unsigned char *s){return s[0]|s[1]<<8;}
static unsigned u32(const unsigned char *s){return u16(s)|(unsigned)u16(s+2)<<16;}
int ki_dictionary(KiDictionary *d,const void *data,unsigned n){
 *d=(KiDictionary){0};const unsigned char *s=data;
 if(n<32||n>KI_DICTIONARY||!s||!equal((const char *)s,"KGIMED1\0",8)||u32(s+8)!=1||u32(s+12)!=n||!u32(s+16)||u32(s+16)>1024||u32(s+24)||u32(s+28))return -22;
 unsigned hash=2166136261u;for(unsigned i=32;i<n;i++)hash=(hash^s[i])*16777619u;if(hash!=u32(s+20))return -22;
 unsigned at=32,entries=0;
 while(at<n){
  if(n-at<4)return -22;
  unsigned bytes=u16(s+at),key=s[at+2],count=s[at+3];
  if(bytes<4||bytes>n-at||!key||key>KI_TEXT||!count||count>12||key>bytes-4)return -22;
  unsigned p=at+4,end=at+bytes;
  if(!kt_valid((const char *)s+p,key,0))return -22;
  p+=key;
  for(unsigned i=0;i<count;i++){if(p==end)return -22;unsigned len=s[p++];if(!len||len>KI_TEXT||len>end-p||!kt_valid((const char *)s+p,len,0))return -22;p+=len;}
  if(p!=end)return -22;
  entries++;at=end;
 }
 if(entries!=u32(s+16))return -22;
 *d=(KiDictionary){s,n,entries};return 0;
}
int ki_active(const KiIme *v){return v->n||v->pending;}
void ki_clear(KiIme *v){unsigned scheme=v->scheme,katakana=v->katakana;*v=(KiIme){.scheme=scheme,.katakana=katakana};}
static int insert(KiIme *v,const char *s){
 unsigned n=ki_length(s);if(n>KI_TEXT-v->n-v->pending)return -27;
 for(unsigned i=v->n+1;i>v->cursor;i--)v->reading[i+n-1]=v->reading[i-1];
 for(unsigned i=0;i<n;i++)v->reading[v->cursor+i]=s[i];
 v->n+=n;v->cursor+=n;return 1;
}
static void erase(KiIme *v,unsigned a,unsigned b){for(unsigned i=b;i<=v->n;i++)v->reading[a+i-b]=v->reading[i];v->n-=b-a;v->cursor=a;}
static int nasal(KiIme *v){v->pending=0;v->roman[0]=0;return insert(v,"\202\361");}
int ki_prepare(KiIme *v){
 if(!v->pending)return 0;
 if((v->pending==1||v->pending==2)&&v->roman[0]=='n'&&(v->pending==1||v->roman[1]=='n')){KiIme before=*v;if(nasal(v)<0){*v=before;v->note="Composition limit: 96 bytes";return -27;}return 0;}
 v->note="Incomplete romaji: edit or cancel";return -22;
}
static int roman(KiIme *v,unsigned c){
 if(c>='A'&&c<='Z')c+=32;
 if(v->pending&&v->roman[0]=='n'&&(v->pending==1||(v->pending==2&&v->roman[1]=='n'))){
  unsigned vowels=c=='a'||c=='i'||c=='u'||c=='e'||c=='o'||c=='y';
  if(c=='\''){return nasal(v);}
  if(v->pending==1&&c=='n'){v->roman[1]='n';v->roman[2]=0;v->pending=2;return 1;}
  if(v->pending==2){if(nasal(v)<0)return -27;if(vowels){v->roman[0]='n';v->roman[1]=0;v->pending=1;}}
  else if(!vowels){if(nasal(v)<0)return -27;}
 }
 if(c<'a'||c>'z'){
  if(v->pending)return -22;
  const char *s=c=='.'?"\201\102":c==','?"\201\101":c=='-'?"\201\133":c=='['?"\201\165":c==']'?"\201\166":0;
  char one[2]={(char)c,0};return insert(v,s?s:one);
 }
 if(v->pending==1&&v->roman[0]==(char)c&&c!='n'&&c!='a'&&c!='i'&&c!='u'&&c!='e'&&c!='o')return insert(v,"\202\301");
 if(v->pending>=6||v->n+v->pending+1>KI_TEXT)return -27;
 v->roman[v->pending++]=c;v->roman[v->pending]=0;
 unsigned prefix=0;
 for(unsigned i=0;i<COUNT(ki_roman);i++){
  unsigned n=ki_length(ki_roman[i].key);if(n<v->pending||!equal(ki_roman[i].key,v->roman,v->pending))continue;
  prefix=1;if(n==v->pending){v->pending=0;v->roman[0]=0;return insert(v,ki_roman[i].text);}
 }
 return prefix||(v->pending==1&&v->roman[0]=='n')?1:-22;
}
static int accent(KiIme *v,unsigned which){
 unsigned a=kt_prev(v->reading,v->n,v->cursor),n=v->cursor-a;
 if(n==2)for(unsigned i=0;i<COUNT(ki_accents);i++)for(unsigned j=0;j<3;j++)if(ki_length(ki_accents[i][j])==2&&equal(v->reading+a,ki_accents[i][j],2)){
  const char *s=ki_accents[i][j==which?0:which];if(!s[0])return 1;
  v->reading[a]=s[0];v->reading[a+1]=s[1];return 1;
 }
 return insert(v,which==1?"\201\112":"\201\113");
}
static int kana(KiIme *v,unsigned c,unsigned shift){
 if(shift&&(c=='['||c=='{'))return insert(v,"\201\165");
 if(shift&&(c==']'||c=='}'))return insert(v,"\201\166");
 if(c==0xde||c=='@'||c=='`')return accent(v,1);
 if(c==0xdf||c=='['||c=='{')return accent(v,2);
 if(c>=0xa1&&c<=0xdf)return insert(v,ki_half[c-0xa1].text);
 if(c>='A'&&c<='Z'){c+=32;shift=1;}
 if(shift){
  for(unsigned i=0;i<COUNT(ki_shifted);i++)if((unsigned char)ki_shifted[i].key[0]==c){c=ki_shifted[i].text[0];break;}
  for(unsigned i=0;i<COUNT(ki_small);i++)if((unsigned char)ki_small[i].key[0]==c)return insert(v,ki_small[i].text);
  if(c==',')return insert(v,"\201\101");
  if(c=='.')return insert(v,"\201\102");
 }
 for(unsigned i=0;i<COUNT(ki_kana);i++)if((unsigned char)ki_kana[i].key[0]==c)return insert(v,ki_kana[i].text);
 return -22;
}
int ki_input(KiIme *v,unsigned key,unsigned modifiers){
 if(!v->scheme)return 0;
 KiIme before=*v;int result=1;v->note=0;
 if(v->selecting){v->note="Esc: edit reading / Enter: commit";return 1;}
 if(key==8){if(v->pending)v->roman[--v->pending]=0;else if(v->cursor)erase(v,kt_prev(v->reading,v->n,v->cursor),v->cursor);}
 else if(key==264){if(v->pending)return 1;if(v->cursor<v->n)erase(v,v->cursor,kt_next(v->reading,v->n,v->cursor));}
 else if(key>=258&&key<=261){
  if(ki_prepare(v)<0)return -22;
  v->cursor=key==258?kt_prev(v->reading,v->n,v->cursor):key==259?kt_next(v->reading,v->n,v->cursor):key==260?0:v->n;
 }
 else if((key>=33&&key<=126)||(key>=0xa1&&key<=0xdf))result=v->scheme==KI_ROMAJI&&key<128?roman(v,key):kana(v,key,modifiers&1);
 else return 0;
 if(result<0){*v=before;v->note=result==-27?"Composition limit: 96 bytes":"Invalid or incomplete input";}
 return result;
}
void ki_script_text(const char *s,unsigned katakana,char *out){
 unsigned n=ki_length(s);copy(out,s,n);if(!katakana)return;
 for(unsigned at=0;at<n;){unsigned k=kt_unit(s+at,n-at);if(k==2)for(unsigned i=0;i<COUNT(ki_script);i++)if(equal(s+at,ki_script[i][0],2)){out[at]=ki_script[i][1][0];out[at+1]=ki_script[i][1][1];break;}at+=k?k:1;}
}
unsigned ki_preview(const KiIme *v,char *out){
 ki_script_text(v->reading,v->katakana,out);unsigned n=v->n;
 /* Pending Latin is inserted at the preedit cursor, never into the document. */
 for(unsigned i=n+1;i>v->cursor;i--)out[i+v->pending-1]=out[i-1];
 for(unsigned i=0;i<v->pending;i++)out[v->cursor+i]=v->roman[i];
 return n+v->pending;
}
static void candidate(KiIme *v,const char *s,unsigned n){
 if(!n||v->count==KI_CANDIDATES)return;
 for(unsigned i=0;i<v->count;i++)if(ki_length(v->candidates[i])==n&&equal(v->candidates[i],s,n))return;
 copy(v->candidates[v->count++],s,n);
}
int ki_convert(KiIme *v,const KiDictionary *d){
 KiIme before=*v;if(ki_prepare(v)<0){*v=before;v->note="Incomplete romaji: edit or cancel";return -22;}
 if(!v->n)return 0;
 v->count=v->selected=0;v->note=0;
 if(d&&d->data)for(unsigned at=32;at<d->bytes;at+=u16(d->data+at)){
  const unsigned char *r=d->data+at;unsigned n=r[2];if(n!=v->n||!equal((const char *)r+4,v->reading,n))continue;
  unsigned p=4+n;for(unsigned i=0;i<r[3];i++){unsigned len=r[p++];candidate(v,(const char *)r+p,len);p+=len;}break;
 }
 else v->note="Dictionary unavailable: kana candidates";
 candidate(v,v->reading,v->n);char text[KI_TEXT+1];ki_script_text(v->reading,1,text);candidate(v,text,v->n);v->selecting=1;return 1;
}
void ki_move_candidate(KiIme *v,int step){if(v->count)v->selected=(v->selected+v->count+(step%(int)v->count))%v->count;}
const char *ki_result(const KiIme *v){return v->selecting&&v->selected<v->count?v->candidates[v->selected]:v->reading;}
