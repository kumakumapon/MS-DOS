#include "k32x.h"
static void number(unsigned n){char s[11];unsigned j=0;do{s[j++]=(char)('0'+n%10);n/=10;}while(n);while(j)k32_write(s+--j,1);}
int main(int argc,char **argv){
 if(argc!=2){k32_puts("Use K32RUN VIEW32.EXE FILE.TXT\r\n");return 1;}
 int h=k32_open(argv[1]);if(h<0){k32_puts("VIEW32: cannot open A: root file\r\n");return 2;}
 unsigned char *text=k32_alloc(65536);unsigned *line=k32_alloc(8192);if(!text||!line){k32_puts("VIEW32: heap exhausted\r\n");return 3;}
 unsigned bytes=0;int n;while(bytes<65536){n=k32_read(h,text+bytes,4096);if(n<0)return 4;if(!n)break;bytes+=(unsigned)n;}
 unsigned lines=1;line[0]=0;for(unsigned j=0;j<bytes&&lines<1024;j++)if(text[j]=='\n')line[lines++]=j+1;
 line[lines]=bytes;unsigned top=0;
 for(;;){
  k32_puts("\033[2J\033[HVIEW32  ");k32_puts(argv[1]);k32_puts("  line ");number(top+1);k32_puts("/");number(lines);k32_puts("\r\n");
  for(unsigned row=0;row<18;row++){
   if(top+row<lines){unsigned begin=line[top+row],end=line[top+row+1],col=0;char rowtext[78];
    for(unsigned j=begin;j<end&&col<78;j++){unsigned char c=text[j];if(c=='\r'||c=='\n')continue;if(c=='\t')c=' ';if(c<32||c==127)c='.';rowtext[col++]=(char)c;}k32_write(rowtext,col);}
   k32_puts("\r\n");
  }
  k32_puts("n/PgDn p/PgUp j/Down k/Up Home End q:quit Esc:stop\r\n");
  if(bytes==65536||lines==1024)k32_puts("Preview limit: 64 KiB / 1024 lines\r\n");
  int k;do{k32_wait(1,0);k=k32_poll_key();}while(k<0);
  unsigned c=(unsigned)k&255,scan=(unsigned)k>>8;
  if(c=='q'||c=='Q')break;
  unsigned last=lines>18?lines-18:0;
  if(c=='n'||c==' '||scan==0x37)top=top+18>last?last:top+18;
  if(c=='p'||scan==0x36)top=top<18?0:top-18;
  if(c=='j'||scan==0x3d){if(top<last)top++;}
  if(c=='k'||scan==0x3a){if(top)top--;}
  if(scan==0x3e)top=0;
  if(scan==0x3f)top=last;
 }
 if(k32_close(h)||k32_free(line)||k32_free(text))return 5;
 k32_puts("VIEW32: closed\r\n");return 0;
}
