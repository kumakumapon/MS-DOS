/* Deliberately bounded grammar: three executables, two pipes, Work redirections. */
#include "shell.h"
static unsigned upper(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int name(const char *s,unsigned n,char out[13]){
 if(!n||n>12)return -22;
 unsigned dot=0;
 for(unsigned i=0;i<n;i++){unsigned c=upper((unsigned char)s[i]);if(c=='.'){if(dot||!i||i>8||n-i<2||n-i>4)return -22;dot=i;}else if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return -22;out[i]=(char)c;}
 if(!dot&&n>8)return -22;
 out[n]=0;return 0;
}
int kp_shell_parse(const char *line,KpShell *out){
 *out=(KpShell){0};unsigned at=0,length=0,quote=0,which=0,total=0;
 while(line[at]){
  unsigned c=(unsigned char)line[at++];if(++total>126||c<32||c>126||((c=='&'||c==';')&&!quote))return -22;
  if(c=='"')quote^=1;
  if(!quote&&(c=='<'||c=='>')){
   char *target=c=='<'?out->input:out->output;if(target[0])return -22;
   while(line[at]==' '){at++;if(++total>126)return -22;}unsigned begin=at;
   while(line[at]&&line[at]!=' '&&line[at]!='|'&&line[at]!='<'&&line[at]!='>'){at++;if(++total>126)return -22;}
   int e=name(line+begin,at-begin,target);if(e)return e;continue;
  }
  if(!quote&&c=='|'){
   while(length&&out->command[which][length-1]==' ')length--;
   if(which+1>=KP_SHELL_COMMANDS||!length)return -22;
   out->command[which][length]=0;which++;length=0;continue;
  }
  if(!length&&c==' ')continue;
  if(length>=127)return -22;
   out->command[which][length++]=(char)c;
 }
 if(quote)return -22;
 while(length&&out->command[which][length-1]==' ')length--;
 if(!length)return -22;
 
   out->command[which][length]=0;
 for(unsigned n=0;n<=which;n++)for(unsigned i=0;out->command[n][i]&&out->command[n][i]!=' ';i++)out->command[n][i]=(char)upper((unsigned char)out->command[n][i]);
 out->count=which+1;return 0;
}
