#include "editor.h"
KeVersion *ke_view(KeDocument *d){return &d->v[(d->first+d->current)%KE_HISTORY];}
void ke_select(KeDocument *d,unsigned anchor,unsigned cursor){KeVersion *v=ke_view(d);v->anchor=anchor>v->n?v->n:anchor;v->cursor=cursor>v->n?v->n:cursor;}
int ke_load(KeDocument *d,const char *s,unsigned n){
 if(n>KE_BYTES)return -1;
 unsigned lf=0,crlf=0;
 for(unsigned i=0;i<n;i++){if(s[i]=='\r'&&(i+1==n||s[i+1]!='\n'))return -1;if(s[i]=='\n'){lf++;if(i&&s[i-1]=='\r')crlf++;}if(s[i]!='\n'&&s[i]!='\r'&&s[i]!='\t'&&((unsigned char)s[i]<32||(unsigned char)s[i]>126))return -1;}
 d->first=d->current=0;d->count=1;KeVersion *v=ke_view(d);v->n=v->anchor=v->cursor=0;
 for(unsigned i=0;i<n;i++)if(s[i]!='\r')d->clean[v->n]=v->text[v->n]=s[i],v->n++;
 v->crlf=d->clean_crlf=lf&&lf==crlf;v->text[v->n]=0;d->clean_n=v->n;d->clean_crlf=v->crlf;return 0;
}
int ke_replace(KeDocument *d,const char *s,unsigned n){
 KeVersion *v=ke_view(d);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;
 if(n>KE_BYTES-(v->n-(b-a)))return -1;
 for(unsigned i=0;i<n;i++)if(s[i]!='\n'&&s[i]!='\t'&&((unsigned char)s[i]<32||(unsigned char)s[i]>126))return -1;
 if(!n&&a==b)return 0;
 unsigned next=(d->first+d->current+1)%KE_HISTORY;
 KeVersion *w=&d->v[next];w->n=v->n-(b-a)+n;w->cursor=w->anchor=a+n;w->crlf=v->crlf;
 for(unsigned i=0;i<a;i++)w->text[i]=v->text[i];
 for(unsigned i=0;i<n;i++)w->text[a+i]=s[i];
 for(unsigned i=b;i<v->n;i++)w->text[a+n+i-b]=v->text[i];
 w->text[w->n]=0;
 if(d->current==KE_HISTORY-1){d->first=(d->first+1)%KE_HISTORY;}else d->current++;
 d->count=d->current+1;return 1;
}
int ke_undo(KeDocument *d,int redo){if(redo){if(d->current+1>=d->count)return 0;d->current++;}else{if(!d->current)return 0;d->current--;}return 1;}
int ke_dirty(KeDocument *d){KeVersion *v=ke_view(d);if(v->n!=d->clean_n||v->crlf!=d->clean_crlf)return 1;for(unsigned i=0;i<v->n;i++)if(v->text[i]!=d->clean[i])return 1;return 0;}
void ke_saved(KeDocument *d){KeVersion *v=ke_view(d);d->clean_n=v->n;d->clean_crlf=v->crlf;for(unsigned i=0;i<=v->n;i++)d->clean[i]=v->text[i];}

static int literal(const char *s,unsigned n){for(unsigned i=0;i<n;i++)if(s[i]!='\n'&&s[i]!='\t'&&((unsigned char)s[i]<32||(unsigned char)s[i]>126))return 0;return 1;}
static int match(const char *text,const char *needle,unsigned n){for(unsigned i=0;i<n;i++)if(text[i]!=needle[i])return 0;return 1;}
int ke_find(KeDocument *d,const char *needle,unsigned n,unsigned start,int wrap){
 KeVersion *v=ke_view(d);if(!n||n>KE_BYTES||!literal(needle,n))return -22;if(n>v->n)return 0;if(start>v->n)start=v->n;
 for(unsigned pass=0;pass<(wrap?2u:1u);pass++){
  unsigned begin=pass?0:start,end=pass?start:v->n;
  for(unsigned at=begin;at<=end&&n<=v->n-at;at++)if((!pass||at<start)&&match(v->text+at,needle,n)){ke_select(d,at,at+n);return 1;}
 }
 return 0;
}
static void advance(KeDocument *d){if(d->current==KE_HISTORY-1)d->first=(d->first+1)%KE_HISTORY;else d->current++;d->count=d->current+1;}
int ke_replace_all(KeDocument *d,const char *needle,unsigned n,const char *replacement,unsigned bytes){
 if(!n||n>KE_BYTES||bytes>KE_BYTES||!literal(needle,n)||!literal(replacement,bytes))return -22;
 KeVersion *v=ke_view(d);unsigned count=0,length=v->n;
 for(unsigned at=0;at<v->n;){if(n<=v->n-at&&match(v->text+at,needle,n)){count++;at+=n;}else at++;}
 if(!count)return 0;
 if(bytes>n){if(count>(KE_BYTES-length)/(bytes-n))return -27;length+=count*(bytes-n);}else length-=count*(n-bytes);
 unsigned next=(d->first+d->current+1)%KE_HISTORY;KeVersion *w=&d->v[next];unsigned out=0;
 for(unsigned at=0;at<v->n;){if(n<=v->n-at&&match(v->text+at,needle,n)){for(unsigned i=0;i<bytes;i++)w->text[out++]=replacement[i];at+=n;}else w->text[out++]=v->text[at++];}
 w->n=length;w->text[out]=0;w->cursor=w->anchor=0;w->crlf=v->crlf;advance(d);return (int)count;
}
int ke_set_eol(KeDocument *d,unsigned crlf){
 if(crlf>1)return -22;
 KeVersion *v=ke_view(d);if(v->crlf==crlf)return 0;
 KeVersion *w=&d->v[(d->first+d->current+1)%KE_HISTORY];w->n=v->n;w->anchor=v->anchor;w->cursor=v->cursor;w->crlf=crlf;
 for(unsigned i=0;i<=v->n;i++)w->text[i]=v->text[i];
 advance(d);return 1;
}
int ke_serialize(KeDocument *d,char *out,unsigned capacity){
 KeVersion *v=ke_view(d);unsigned bytes=v->n;
 if(v->crlf)for(unsigned i=0;i<v->n;i++)if(v->text[i]=='\n')bytes++;
 if(bytes>capacity||bytes>KE_BYTES)return -27;
 unsigned at=0;for(unsigned i=0;i<v->n;i++){if(v->crlf&&v->text[i]=='\n')out[at++]='\r';out[at++]=v->text[i];}return (int)bytes;
}
