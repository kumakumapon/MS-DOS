#include "text.h"
#include "../filer/sjis.h"
void k98_reset(K98Text *t){*t=(K98Text){.attribute=K98_WHITE};}
/* Convert the PC-98 ku-0x20 / ten word to a bounded JIS X0208 SJIS glyph. */
unsigned k98_sjis(unsigned code){
 unsigned ku=(code&127)+32,ten=(code>>8)&127;
 if(ku<33||ku>126||ten<33||ten>126)return 0;
 unsigned lead=(ku-33)/2+0x81;if(lead>0x9f)lead+=0x40;
 unsigned trail=(ku&1)?ten+(ten<0x60?0x1f:0x20):ten+0x7e;
 return sjis_lead(lead)&&sjis_trail(trail)?lead*256+trail:0;
}
/* Scan from the row start so horizontal clipping can retain a right half.
 * PC-98 consumes two cells for a kanji lead; the next word need not repeat it. */
unsigned k98_glyph(const unsigned short *row,unsigned column,unsigned *half){
 *half=0;if(column>=80)return ' ';
 for(unsigned at=0;at<=column;){unsigned raw=row[at],wide=raw>>8,code=wide?k98_sjis(raw):0;
  if(wide&&code){if(at==79)return '?';if(column==at||column==at+1){*half=column-at;return code;}at+=2;}
  else{if(at==column){unsigned c=raw&255;return !wide&&((c>=32&&c<=126)||(c>=0xa1&&c<=0xdf))?c:wide?'?':' ';}at++;}
 }return ' ';
}
void k98_row(const unsigned short *row,unsigned short *glyphs,unsigned char *halves){
 for(unsigned at=0;at<80;){unsigned w=row[at],c=(w>>8)?k98_sjis(w):0;
  halves[at]=0;
  if(c&&at<79){glyphs[at]=glyphs[at+1]=c;halves[at+1]=1;at+=2;}
  else{unsigned b=w&255;glyphs[at]=(w>>8)?'?':((b>=32&&b<=126)||(b>=0xa1&&b<=0xdf))?b:32;at++;}
 }
}
unsigned k98_rgb(unsigned a){return ((a&64)?0xff0000u:0)|((a&128)?0x00ff00u:0)|((a&32)?0x0000ffu:0);}
unsigned k98_ink(unsigned bits,unsigned attr,unsigned row,unsigned blink,unsigned cursor){
 unsigned visible=(attr&1)&&(!(attr&2)||blink);bits=visible?bits:0;
 if(attr&4)bits^=255;
 if((attr&8)&&row==15)bits=255;
 if(attr&16)bits|=8; /* PC-98 vertical-line bit, normal (non-semigraphics) mode. */
 if(cursor&&row>=14)bits^=255;
 return bits;
}
static void newline(K98Text *t,const K98Surface *s){if(t->row>=25){s->scroll(s->context,t->attribute);t->row=24;}}
static unsigned cell(K98Text *t){return t->row*80+t->column;}
static void erase_half(K98Text *t,const K98Surface *s,unsigned at){
 if(!(s->get(s->context,0,at)>>8)&&(!(at%80)||!(s->get(s->context,0,at-1)>>8)))return;
 unsigned row=at/80*80;for(unsigned i=row;i<row+80;){unsigned code=s->get(s->context,0,i);if((code>>8)&&k98_sjis(code)&&i+1<row+80){if(at==i||at==i+1){s->fill(s->context,i,2,32,t->attribute);return;}i+=2;}else i++;}
}
static void put(K98Text *t,const K98Surface *s,unsigned code,unsigned columns){
 if(columns==2&&t->column==79){erase_half(t,s,cell(t));s->fill(s->context,cell(t),1,32,t->attribute);t->column=0;t->row++;newline(t,s);}
 unsigned at=cell(t);erase_half(t,s,at);if(columns==2)erase_half(t,s,at+1);
 s->set(s->context,0,at,code);s->set(s->context,1,at,t->attribute);
 if(columns==2){s->set(s->context,0,at+1,0);s->set(s->context,1,at+1,t->attribute);}
 t->column+=columns;if(t->column>=80){t->column=0;t->row++;newline(t,s);}
}
static void rendition(K98Text *t,unsigned p){
 if(!p)t->attribute=K98_WHITE;
 else if(p==4)t->attribute|=8;else if(p==5)t->attribute|=2;else if(p==7)t->attribute|=4;else if(p==8)t->attribute&=~1u;
 else if(p==24)t->attribute&=~8u;else if(p==25)t->attribute&=~2u;else if(p==27)t->attribute&=~4u;else if(p==28)t->attribute|=1;
 else if((p>=30&&p<=37)||p==39){unsigned c=p==39?7:p-30;t->attribute=(t->attribute&31)|((c&1)?64:0)|((c&2)?128:0)|((c&4)?32:0);}
}
static void csi(K98Text *t,unsigned c,const K98Surface *s){
 unsigned p=t->parameter[0],n=p?p:1,at=cell(t);
 if(c=='m')for(unsigned i=0;i<=t->which;i++)rendition(t,t->parameter[i]);
 else if(c=='H'||c=='f'){unsigned r=p?p-1:0,col=t->parameter[1]?t->parameter[1]-1:0;t->row=r<25?r:24;t->column=col<80?col:79;}
 else if(c=='A')t->row=n>t->row?0:t->row-n;
 else if(c=='B')t->row=t->row+n<25?t->row+n:24;
 else if(c=='C')t->column=t->column+n<80?t->column+n:79;
 else if(c=='D')t->column=n>t->column?0:t->column-n;
 else if(c=='G')t->column=n<=80?n-1:79;
 else if(c=='J'&&p<=2){unsigned first=p==0?at:0,end=p==1?at+1:2000;erase_half(t,s,first);erase_half(t,s,end-1);s->fill(s->context,first,end-first,32,t->attribute);}
 else if(c=='K'&&p<=2){unsigned first=p==0?at:t->row*80u,end=p==1?at+1:t->row*80u+80;erase_half(t,s,first);erase_half(t,s,end-1);s->fill(s->context,first,end-first,32,t->attribute);}
 else if(c=='s'){t->saved_column=t->column;t->saved_row=t->row;}
 else if(c=='u'){t->column=t->saved_column;t->row=t->saved_row;}
}
void k98_output(K98Text *t,unsigned c,const K98Surface *s){
 c&=255;
 if(t->pending){unsigned lead=t->pending;t->pending=0;if(sjis_trail(c)){put(t,s,sjis_vram(lead,c),2);return;}put(t,s,'?',1);}
 if(t->escape==1){t->escape=c=='['?2:0;t->which=0;for(unsigned i=0;i<4;i++)t->parameter[i]=0;return;}
 if(t->escape==2){
  if(c>='0'&&c<='9'){unsigned p=t->parameter[t->which];t->parameter[t->which]=p<100?p*10+c-'0':999;return;}
  if(c==';'){if(t->which<3)t->which++;else t->escape=3;return;}
  t->escape=0;csi(t,c,s);return;
 }
 if(t->escape==3){if(c>=0x40&&c<=0x7e)t->escape=0;return;}
 if(c==27){t->escape=1;return;}
 if(sjis_lead(c)){t->pending=c;return;}
 if(c=='\r')t->column=0;
 else if(c=='\n'){t->row++;newline(t,s);}
 else if(c==8){if(t->column)t->column--;else if(t->row){t->row--;t->column=79;}unsigned first=t->row*80u,at=first;while(at<first+t->column){unsigned w=s->get(s->context,0,at);unsigned step=(w>>8)&&k98_sjis(w)&&at+1<first+80?2:1;if(at+step>first+t->column){t->column=at-first;break;}at+=step;}}
 else if(c==9){unsigned n=8-(t->column&7);while(n--)put(t,s,32,1);}
 else if((c>=32&&c<=126)||(c>=0xa1&&c<=0xdf))put(t,s,c,1);
}
