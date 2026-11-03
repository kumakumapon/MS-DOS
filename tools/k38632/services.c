/* Pure bounded policy, shared by the real-mode broker and host tests. */
#include "services.h"
int k32_arguments(const u8 *tail,unsigned n,K32Args *out){
 K32Args a={0};unsigned i=0;if(n>126)return 1;
 while(i<n){
  while(i<n&&tail[i]==' ')i++;
  if(i==n)break;
  if(a.argc==8)return 1;
  unsigned at=0;int quote=0;
  while(i<n&&(quote||tail[i]!=' ')){
   u8 c=tail[i++];if(c=='"'){quote=!quote;continue;}
   if(c<32||c>126||at==63)return 1;
   a.arg[a.argc][at++]=(char)c;
  }
  if(quote)return 1;
  a.argc++;
 }
 if(a.argc&&a.arg[0][0]=='/'&&(a.arg[0][1]=='T'||a.arg[0][1]=='t')&&!a.arg[0][2]){
  a.budget=64;for(unsigned j=1;j<a.argc;j++)for(unsigned k=0;k<64;k++)a.arg[j-1][k]=a.arg[j][k];a.argc--;
 }
 if(!a.argc||!a.arg[0][0])return 1;
 *out=a;return 0;
}
static int same(const char *s,const char *b){unsigned i=0;for(;s[i]&&s[i]==b[i];i++);return !s[i]&&!b[i];}
int k32_filename(const char *in,unsigned n,char out[16]){
 char base[9]={0};unsigned b=0,e=0;int dot=0;
 if(!n||n>12)return 0;
 out[0]='A';out[1]=':';out[2]='\\';
 for(unsigned i=0;i<n;i++){
  u8 c=(u8)in[i];if(c>='a'&&c<='z')c-=32;
  if(c=='.'){if(dot||!b||i+1==n)return 0;dot=1;}
  else {
   if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='$'))return 0;
   if(!dot){if(b==8)return 0;base[b++]=(char)c;}else if(++e>3)return 0;
  }
  out[i+3]=(char)c;
 }
 out[n+3]=0;
 if(same(base,"CON")||same(base,"PRN")||same(base,"AUX")||same(base,"NUL")||same(base,"CLOCK$"))return 0;
 if((base[0]=='C'&&base[1]=='O'&&base[2]=='M')||(base[0]=='L'&&base[1]=='P'&&base[2]=='T'))if(base[3]>='1'&&base[3]<='9'&&!base[4])return 0;
 return 1;
}
void k32_heap_init(K32Heap *h,u32 base,u32 limit){*h=(K32Heap){.base=base,.limit=limit};}
int k32_heap_find(K32Heap *h,u32 bytes,unsigned *slot,u32 *address,u32 *pages){
 if(!bytes||bytes>0x400000)return -22;
 unsigned free=K32_ALLOCS;for(unsigned i=0;i<K32_ALLOCS;i++)if(!h->block[i].pages){free=i;break;}
 if(free==K32_ALLOCS)return -12;
 u32 count=(bytes+4095)/4096,at=h->base;
 while(at<=h->limit&&count<=(h->limit-at)/4096){
  int overlap=0;u32 end=at+count*4096;
  for(unsigned i=0;i<K32_ALLOCS;i++)if(h->block[i].pages){
   u32 first=h->block[i].first,last=first+h->block[i].pages*4096;
   if(at<last&&first<end){at=last;overlap=1;break;}
  }
  if(!overlap){*slot=free;*address=at;*pages=count;return 0;}
 }
 return -12;
}
int k32_heap_release(K32Heap *h,u32 address,unsigned *slot){
 for(unsigned i=0;i<K32_ALLOCS;i++)if(h->block[i].pages&&h->block[i].first==address){*slot=i;return 0;}
 return -22;
}
int k32_user_range(const u32 *map,u32 at,u32 n,int write){
 if(!n)return 1;
 if(at<K32_BASE||at>=0xc00000||n>0xc00000-at)return 0;
 unsigned first=(at-K32_BASE)/4096,last=(at+n-1-K32_BASE)/4096,flags=write?7:5;
 for(unsigned i=first;i<=last;i++)if((map[i]&flags)!=flags)return 0;
 return 1;
}
