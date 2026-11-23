#include "boot-settings.h"
#include "io.h"

typedef struct {unsigned first,end,next;} Line;
typedef struct {char *data;unsigned length,capacity;int overflow;} Output;
enum {OTHER,XMS,EMS,FILES,BUFFERS,FD98_MARK,FD98_CMD,KXMS,KGUI};

unsigned kg_settings_config_flags(const KgBootSettings *s){
 return (s->xms?K32_BOOT_XMS:0)|(s->ems?K32_BOOT_EMS:0)|(s->files<<8)|(s->buffers<<16);
}
int kg_settings_decode(KgBootSettings *next,const KgBootSettings *old,unsigned startup,unsigned flags){
 *next=*old;
 if(startup){
  if(flags&~K32_BOOT_FD98)return -22;
  next->fd98=!!(flags&K32_BOOT_FD98);return 0;
 }
 if(flags&~0xffff03u)return -22;
 next->xms=!!(flags&K32_BOOT_XMS);next->ems=!!(flags&K32_BOOT_EMS);
 next->files=(flags>>8)&255;next->buffers=(flags>>16)&255;
 if(next->ems&&!next->xms)return -22;
 if(next->files!=old->files&&(next->files<20||next->files>60))return -22;
 if(next->buffers!=old->buffers&&next->buffers!=8&&next->buffers!=16)return -22;
 return 0;
}

static int white(char c){return c==' '||c=='\t';}
static char upper(char c){return c>='a'&&c<='z'?c-32:c;}
static int equal(const char *s,unsigned n,const char *key){
 unsigned i=0;while(i<n&&key[i]&&upper(s[i])==key[i])i++;
 return i==n&&!key[i];
}
static int word(const char *s,unsigned n,const char *key){
 unsigned i=0;while(i<n&&key[i]&&upper(s[i])==key[i])i++;
 return !key[i]&&(i==n||white(s[i]));
}
static Line read_line(const char *text,unsigned n,unsigned at){
 Line l={.first=at,.end=at};
 while(l.end<n&&text[l.end]!='\r'&&text[l.end]!='\n')l.end++;
 l.next=l.end;
 if(l.next<n&&text[l.next]=='\r')l.next++;
 if(l.next<n&&text[l.next]=='\n')l.next++;
 return l;
}
static unsigned classify(const char *text,Line l,unsigned startup,unsigned *number){
 while(l.first<l.end&&white(text[l.first]))l.first++;
 while(l.end>l.first&&white(text[l.end-1]))l.end--;
 const char *s=text+l.first;unsigned n=l.end-l.first;
 if(startup){
  if(equal(s,n,"REM CPANEL AUTO FD98"))return FD98_MARK;
  if(equal(s,n,"FD98"))return FD98_CMD;
  if(word(s,n,"KGUILDR")||word(s,n,"KGUILDR.EXE"))return KGUI;
  return OTHER;
 }
 unsigned eq=0;while(eq<n&&s[eq]!='=')eq++;
 if(eq==n)return OTHER;
 unsigned left=eq;while(left&&white(s[left-1]))left--;
 const char *value=s+eq+1,*end=s+n;
 while(value<end&&white(*value))value++;
 unsigned len=end-value;
 if(equal(s,left,"DEVICE")||equal(s,left,"DEVICEHIGH")){
  if(word(value,len,"FDXMS286.SYS"))return XMS;
  if(word(value,len,"EMM386.EXE"))return EMS;
  if(word(value,len,"KXMS32.SYS"))return KXMS;
  return OTHER;
 }
 unsigned kind=equal(s,left,"FILES")?FILES:equal(s,left,"BUFFERS")?BUFFERS:OTHER;
 if(kind==OTHER)return OTHER;
 unsigned result=0;
 while(value<end&&*value>='0'&&*value<='9'){
  result=result*10+*value++-'0';
  if(result>255)return OTHER;
 }
 if(value!=end||!result)return OTHER;
 *number=result;return kind;
}
static unsigned body_length(const char *data,unsigned n){return n&&(unsigned char)data[n-1]==26?n-1:n;}
void kg_settings_read(KgBootSettings *s,unsigned startup,const char *text,unsigned n){
 if(startup)s->fd98=0;
 else {s->xms=s->ems=0;s->files=20;s->buffers=8;}
 n=body_length(text,n);
 for(unsigned at=0;at<n;){
  unsigned value=0;
  Line l=read_line(text,n,at);unsigned kind=classify(text,l,startup,&value);
  if(kind==XMS)s->xms=1;
  if(kind==EMS)s->ems=1;
  if(kind==FILES)s->files=value;
  if(kind==BUFFERS)s->buffers=value;
  if(kind==FD98_MARK&&l.next<n&&classify(text,read_line(text,n,l.next),1,&value)==FD98_CMD)s->fd98=1;
  at=l.next;
 }
}
static void append(Output *o,const char *s,unsigned n){
 if(n>o->capacity-o->length){o->overflow=1;return;}
 for(unsigned i=0;i<n;i++)o->data[o->length++]=s[i];
}
static void append_line(Output *o,const char *line){
 if(o->length&&o->data[o->length-1]!='\n'&&o->data[o->length-1]!='\r')append(o,"\r\n",2);
 unsigned n=0;while(line[n])n++;
 append(o,line,n);append(o,"\r\n",2);
}
static void number_line(Output *o,unsigned kind,unsigned value){
 char row[14];const char *name=kind==FILES?"FILES=":"BUFFERS=";
 unsigned at=0;while(name[at]){row[at]=name[at];at++;}
 if(value>=100)row[at++]='0'+value/100;
 if(value>=10)row[at++]='0'+(value/10)%10;
 row[at++]='0'+value%10;row[at]=0;
 append_line(o,row);
}
static void install_fd98(Output *o){append_line(o,"REM CPANEL AUTO FD98");append_line(o,"FD98");}
int kg_settings_build(const KgBootSettings *old,const KgBootSettings *next,unsigned startup,
 const char *text,unsigned n,char *output,unsigned capacity){
 Output o={.data=output,.capacity=capacity};
 unsigned body=body_length(text,n),added=0;
 unsigned insert_xms=!old->xms&&next->xms,insert_ems=!old->ems&&next->ems;
 unsigned insert_fd98=!old->fd98&&next->fd98;
 for(unsigned at=0;at<body;){
  unsigned value=0;
  Line l=read_line(text,body,at);
  unsigned kind=classify(text,l,startup,&value);
  if(startup){
   if(kind==KGUI&&insert_fd98){install_fd98(&o);insert_fd98=0;}
   if(kind==FD98_MARK&&l.next<body&&classify(text,read_line(text,body,l.next),1,&value)==FD98_CMD&&!next->fd98){
    at=read_line(text,body,l.next).next;continue;
   }
  }else{
   if(insert_xms&&(kind==KXMS||kind==EMS)){
    append_line(&o,"DEVICE=FDXMS286.SYS");insert_xms=0;
   }
   if((kind==XMS&&!next->xms)||(kind==EMS&&!next->ems)){at=l.next;continue;}
   if((kind==FILES&&old->files!=next->files)||(kind==BUFFERS&&old->buffers!=next->buffers)){
    if(!(added&(1u<<kind))){number_line(&o,kind,kind==FILES?next->files:next->buffers);added|=1u<<kind;}
    at=l.next;continue;
   }
  }
  append(&o,text+at,l.next-at);at=l.next;
 }
 if(startup){if(insert_fd98)install_fd98(&o);}
 else {
  if(insert_xms)append_line(&o,"DEVICE=FDXMS286.SYS");
  if(insert_ems)append_line(&o,"DEVICE=EMM386.EXE EMM=8192");
  if(old->files!=next->files&&!(added&(1u<<FILES)))number_line(&o,FILES,next->files);
  if(old->buffers!=next->buffers&&!(added&(1u<<BUFFERS)))number_line(&o,BUFFERS,next->buffers);
 }
 append(&o,text+body,n-body);
 return o.overflow?-27:(int)o.length;
}
