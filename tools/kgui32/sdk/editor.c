#include "editor.h"
KeVersion *ke_view(KeDocument *d){return &d->v[(d->first+d->current)%KE_HISTORY];}
void ke_select(KeDocument *d,unsigned anchor,unsigned cursor){KeVersion *v=ke_view(d);v->anchor=anchor>v->n?v->n:anchor;v->cursor=cursor>v->n?v->n:cursor;}
int ke_load(KeDocument *d,const char *s,unsigned n){
 if(n>KE_BYTES)return -1;
 for(unsigned i=0;i<n;i++)if(s[i]!='\n'&&s[i]!='\r'&&s[i]!='\t'&&((unsigned char)s[i]<32||(unsigned char)s[i]>126))return -1;
 d->first=d->current=0;d->count=1;KeVersion *v=ke_view(d);v->n=v->anchor=v->cursor=0;
 for(unsigned i=0;i<n;i++)if(s[i]!='\r')d->clean[v->n]=v->text[v->n]=s[i],v->n++;
 v->text[v->n]=0;d->clean_n=v->n;return 0;
}
int ke_replace(KeDocument *d,const char *s,unsigned n){
 KeVersion *v=ke_view(d);unsigned a=v->anchor<v->cursor?v->anchor:v->cursor,b=v->anchor>v->cursor?v->anchor:v->cursor;
 if(n>KE_BYTES-(v->n-(b-a)))return -1;
 for(unsigned i=0;i<n;i++)if(s[i]!='\n'&&s[i]!='\t'&&((unsigned char)s[i]<32||(unsigned char)s[i]>126))return -1;
 if(!n&&a==b)return 0;
 unsigned next=(d->first+d->current+1)%KE_HISTORY;
 KeVersion *w=&d->v[next];w->n=v->n-(b-a)+n;w->cursor=w->anchor=a+n;
 for(unsigned i=0;i<a;i++)w->text[i]=v->text[i];
 for(unsigned i=0;i<n;i++)w->text[a+i]=s[i];
 for(unsigned i=b;i<v->n;i++)w->text[a+n+i-b]=v->text[i];
 w->text[w->n]=0;
 if(d->current==KE_HISTORY-1){d->first=(d->first+1)%KE_HISTORY;}else d->current++;
 d->count=d->current+1;return 1;
}
int ke_undo(KeDocument *d,int redo){if(redo){if(d->current+1>=d->count)return 0;d->current++;}else{if(!d->current)return 0;d->current--;}return 1;}
int ke_dirty(KeDocument *d){KeVersion *v=ke_view(d);if(v->n!=d->clean_n)return 1;for(unsigned i=0;i<v->n;i++)if(v->text[i]!=d->clean[i])return 1;return 0;}
void ke_saved(KeDocument *d){KeVersion *v=ke_view(d);d->clean_n=v->n;for(unsigned i=0;i<=v->n;i++)d->clean[i]=v->text[i];}
