/* Shift-JIS byte policy shared by the desktop, clients and editor.
 * JIS X 0208 plus printable ASCII/JIS X 0201 kana; no CP932 extensions.
 * A double-byte glyph occupies two 8-pixel cells. Never split a character.
 */
#ifndef KG_TEXT_H
#define KG_TEXT_H
#include "../../filer/sjis.h"
#define KT_LINES 1u
#define KT_CRLF 2u
static inline unsigned kt_unit(const char *s,unsigned n){
 if(!n)return 0;
 unsigned c=(unsigned char)s[0];
 if((c>=32&&c<=126)||(c>=0xa1&&c<=0xdf))return 1;
 return sjis_lead(c)&&n>=2&&sjis_trail((unsigned char)s[1])?2:0;
}
static inline int kt_valid(const char *s,unsigned n,unsigned flags){
 for(unsigned i=0;i<n;){unsigned k=kt_unit(s+i,n-i);if(k){i+=k;continue;}
  if((flags&KT_LINES)&&(s[i]=='\n'||s[i]=='\t')){i++;continue;}
  if((flags&KT_CRLF)&&s[i]=='\r'&&i+1<n&&s[i+1]=='\n'){i++;continue;}
  return 0;
 }return 1;
}
static inline int kt_cstr(const char *s,unsigned capacity){
 unsigned n=0;while(n<capacity&&s[n])n++;
 return n<capacity&&kt_valid(s,n,0)?(int)n:-1;
}
static inline unsigned kt_floor(const char *s,unsigned n,unsigned at){
 if(at>n)at=n;
 unsigned i=0;while(i<at){unsigned k=kt_unit(s+i,n-i);if(!k)k=1;if(k>at-i)break;i+=k;}return i;
}
static inline unsigned kt_next(const char *s,unsigned n,unsigned at){
 if(at>=n)return n;
 unsigned k=kt_unit(s+at,n-at);return at+(k?k:1);
}
static inline unsigned kt_prev(const char *s,unsigned n,unsigned at){
 if(at>n)at=n;
 return at?kt_floor(s,n,at-1):0;
}
static inline unsigned kt_prefix(const char *s,unsigned n,unsigned capacity){
 if(capacity>n)capacity=n;
 return kt_floor(s,n,capacity);
}
static inline unsigned kt_glyph(unsigned code){
 char s[2];unsigned n;
 if(code>65535)return 0;
 if(code>255){s[0]=code>>8;s[1]=code;n=2;}else{s[0]=code;n=1;}
 return kt_unit(s,n)==n?n:0;
}
#endif
